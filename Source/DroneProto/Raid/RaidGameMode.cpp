#include "RaidGameMode.h"

#include "BalanceTelemetryComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "Drone.h"
#include "DronePartInventory.h"
#include "DronePartReturnManager.h"
#include "RaidBoss.h"
#include "RaidGameState.h"
#include "RaidPlayerController.h"
#include "RaidPlayerState.h"
#include "RaidServerAdmissionService.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "GameDelegates.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"

namespace
{
// 타이머 불일치 감시 주기. 제한 시간(분 단위)에 비해 촘촘하면서 서버 tick 비용이 무시할 만한 값이다.
constexpr float RaidTimerWatchdogIntervalSeconds = 5.0f;

FString ResolveBalanceTelemetryVersion()
{
	FString BalanceVersion;
	if (FParse::Value(FCommandLine::Get(), TEXT("BalanceVersion="), BalanceVersion) && !BalanceVersion.IsEmpty())
	{
		return BalanceVersion;
	}
	if (GConfig)
	{
		GConfig->GetString(TEXT("BalanceTelemetry"), TEXT("BalanceTelemetryVersion"), BalanceVersion, GGameIni);
	}
	return BalanceVersion.IsEmpty() ? TEXT("Unspecified") : BalanceVersion;
}

FName ResolveRaidServerSlot()
{
	FString ServerSlot;
	FParse::Value(FCommandLine::Get(), TEXT("RaidServerSlot="), ServerSlot);
	return ServerSlot.IsEmpty() ? FName(TEXT("Local")) : FName(*ServerSlot);
}

const FName RaidEntrySpawnTag(TEXT("RaidEntrySpawn"));
constexpr int32 RequiredRaidEntrySpawnCount = 4;

FString BuildRaidGameModeControllerLogString(const AController* Controller)
{
	return ARaidPlayerController::BuildStableControllerLogString(Controller);
}

void LogControllerPawnState(const TCHAR* Source, const AController* Controller)
{
	const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	UE_LOG(LogTemp, Log, TEXT("[Server] %s PawnState: PlayerController=%s Pawn=%s PawnClass=%s IsADrone=%s"),
		Source,
		*BuildRaidGameModeControllerLogString(Controller),
		Pawn ? *Pawn->GetName() : TEXT("None"),
		Pawn ? *Pawn->GetClass()->GetName() : TEXT("None"),
		Pawn && Pawn->IsA<ADrone>() ? TEXT("true") : TEXT("false"));
}

EDroneReportTrigger ResolveReportTriggerForRaidEndReason(FName Reason)
{
	return Reason == FName(TEXT("BossDefeated"))
		? EDroneReportTrigger::BossDefeated
		: EDroneReportTrigger::RaidTimeLimit;
}

const TCHAR* ToReportTriggerLogString(EDroneReportTrigger Trigger)
{
	switch (Trigger)
	{
	case EDroneReportTrigger::Death:
		return TEXT("Death");
	case EDroneReportTrigger::BossDefeated:
		return TEXT("BossDefeated");
	case EDroneReportTrigger::RaidTimeLimit:
		return TEXT("RaidTimeLimit");
	default:
		return TEXT("Unknown");
	}
}

}

ARaidGameMode::ARaidGameMode()
{
	BalanceTelemetry = CreateDefaultSubobject<UBalanceTelemetryComponent>(TEXT("BalanceTelemetry"));
	GameStateClass       = ARaidGameState::StaticClass();
	PlayerControllerClass = ARaidPlayerController::StaticClass();
	PlayerStateClass      = ARaidPlayerState::StaticClass();
}

void ARaidGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	// Battle 전환과 타이머 시작이 갈라진 상태를 주기적으로 감시한다(BOSS-13·POP-08).
	// GameState 확보 실패로 아래에서 조기 반환하더라도 감시는 유지돼야 하므로 여기서 건다.
	GetWorldTimerManager().SetTimer(
		RaidTimerWatchdogTimerHandle,
		this,
		&ARaidGameMode::HandleRaidTimerWatchdogTickForServer,
		RaidTimerWatchdogIntervalSeconds,
		true);

	// 입장 예약 게이트(ENTRY-03/09/12)는 GameState 확보 실패와 무관하게 켜져야 한다.
	// bAdmissionRequired가 false로 남으면 예약 토큰 없는 접속이 그대로 통과하므로(fail-open)
	// 아래 GameState 조기 반환보다 앞에 둔다.
	if (GetNetMode() == NM_DedicatedServer)
	{
		bAdmissionRequired = true;
		FString RaidSlot;
		FString GameEndpoint;
		int32 ReservationPort = 0;
		FParse::Value(FCommandLine::Get(), TEXT("RaidSlot="), RaidSlot);
		FParse::Value(FCommandLine::Get(), TEXT("RaidGameEndpoint="), GameEndpoint);
		FParse::Value(FCommandLine::Get(), TEXT("RaidReservationPort="), ReservationPort);

		AdmissionService = NewObject<URaidServerAdmissionService>(this);
		if (!AdmissionService->Initialize(this, RaidSlot, static_cast<uint32>(FMath::Max(0, ReservationPort)), GameEndpoint))
		{
			AdmissionService = nullptr;
			UE_LOG(LogTemp, Error, TEXT("[DR_SUMMARY] RaidAdmissionStartFailed Slot=%s ReservationPort=%d GameEndpoint=%s"),
				*RaidSlot,
				ReservationPort,
				*GameEndpoint);
		}

		if (AdmissionService)
		{
			// 입장 도중 끊긴 연결은 PlayerController가 없어 Logout이 불리지 않는다.
			// 엔진이 그 구간의 연결 상실을 알려 주는 유일한 경로가 이 델리게이트다(ENTRY-15).
			PendingConnectionLostDelegateHandle = FGameDelegates::Get().GetPendingConnectionLostDelegate().AddUObject(
				this,
				&ARaidGameMode::HandlePendingConnectionLostForServer);
		}
	}

	ARaidGameState* GS = GetGameState<ARaidGameState>();
	if (!GS)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Server] DronePartInventory spawn failed: RaidGameState is missing"));
		return;
	}

	if (!GS->GetDronePartInventory())
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = GS;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ADronePartInventory* SpawnedInventory = GetWorld()->SpawnActor<ADronePartInventory>(
			ADronePartInventory::StaticClass(),
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			SpawnParams);

		if (SpawnedInventory)
		{
			GS->SetDronePartInventory(SpawnedInventory);
			UE_LOG(LogTemp, Log, TEXT("[Server] DronePartInventory spawned: Name=%s Replicates=%s AlwaysRelevant=%s Dormancy=%d"),
				*SpawnedInventory->GetName(),
				SpawnedInventory->GetIsReplicated() ? TEXT("true") : TEXT("false"),
				SpawnedInventory->bAlwaysRelevant ? TEXT("true") : TEXT("false"),
				static_cast<int32>(SpawnedInventory->NetDormancy));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[Server] DronePartInventory spawn failed"));
		}
	}

	EnsureDronePartReturnManagerForServer();
}

ARaidBoss* ARaidGameMode::EnsureRaidBossForServer()
{
	if (!HasAuthority())
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	ARaidGameState* GS = World ? World->GetGameState<ARaidGameState>() : nullptr;
	if (!World || !GS)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Server] RaidBoss spawn failed: RaidGameState is missing"));
		return nullptr;
	}

	if (ARaidBoss* RegisteredBoss = GS->GetRaidBoss())
	{
		return RegisteredBoss;
	}

	ARaidBoss* ExistingBoss = nullptr;
	for (TActorIterator<ARaidBoss> It(World); It; ++It)
	{
		ExistingBoss = *It;
		break;
	}

	if (!ExistingBoss)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = GS;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		UClass* const SpawnClass = RaidBossClass.Get() ? RaidBossClass.Get() : ARaidBoss::StaticClass();

		ExistingBoss = World->SpawnActor<ARaidBoss>(
			SpawnClass,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			SpawnParams);
	}

	if (!ExistingBoss)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Server] RaidBoss spawn failed"));
		return nullptr;
	}

	// BOSS-14: 보스는 맵 중앙(원점)에 있어야 한다. 패턴 액터가 보스 트랜스폼을 스냅샷해 스폰되고
	// PlayerStart 4개도 원점 기준 35m·90° 간격으로 배치돼 있어, 보스가 어긋나면 패턴 평면과
	// 진입 배치가 함께 틀어진다. 직접 스폰 경로는 항상 원점이지만 맵 사전 배치 보스를 채택하는
	// 경로는 그 액터의 트랜스폼을 그대로 받으므로 여기서 보정한다.
	constexpr float BossCenterToleranceCm = 1.0f;
	const FVector AdoptedBossLocation = ExistingBoss->GetActorLocation();
	if (!AdoptedBossLocation.Equals(FVector::ZeroVector, BossCenterToleranceCm))
	{
		ExistingBoss->SetActorLocation(FVector::ZeroVector);
		UE_LOG(LogTemp, Warning, TEXT("[DR_SUMMARY] BossPositionCorrected Name=%s From=%s To=X=0.0 Y=0.0 Z=0.0"),
			*ExistingBoss->GetName(),
			*AdoptedBossLocation.ToCompactString());
	}

	GS->SetRaidBossForServer(ExistingBoss);
	UE_LOG(LogTemp, Log, TEXT("[Server] RaidBoss ready: Source=BattleStart Name=%s Replicates=%s"),
		*ExistingBoss->GetName(),
		ExistingBoss->GetIsReplicated() ? TEXT("true") : TEXT("false"));
	return ExistingBoss;
}

void ARaidGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (PendingConnectionLostDelegateHandle.IsValid())
	{
		FGameDelegates::Get().GetPendingConnectionLostDelegate().Remove(PendingConnectionLostDelegateHandle);
		PendingConnectionLostDelegateHandle.Reset();
	}
	if (AdmissionService)
	{
		AdmissionService->Shutdown();
	}
	Super::EndPlay(EndPlayReason);
}

void ARaidGameMode::PreLogin(
	const FString& Options,
	const FString& Address,
	const FUniqueNetIdRepl& UniqueId,
	FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (!ErrorMessage.IsEmpty() || !bAdmissionRequired)
	{
		return;
	}
	ValidateRaidAdmission(Options, ErrorMessage);
}

void ARaidGameMode::ValidateRaidAdmission(const FString& Options, FString& OutErrorMessage)
{
	FName RejectReason;
	if (!CanAcceptRaidJoinForServer(RejectReason, false))
	{
		OutErrorMessage = RejectReason.ToString();
		return;
	}
	if (!AdmissionService)
	{
		OutErrorMessage = TEXT("RaidAdmissionUnavailable");
		return;
	}

	const FString Slot = UGameplayStatics::ParseOption(Options, TEXT("RaidSlot"));
	const FString Token = UGameplayStatics::ParseOption(Options, TEXT("RaidReservation"));
	AdmissionService->TryClaim(Slot, Token, FPlatformTime::Seconds(), OutErrorMessage);
}

void ARaidGameMode::HandlePendingConnectionLostForServer(const FUniqueNetIdRepl& ConnectionUniqueId)
{
	// 엔진은 어떤 연결이 사라졌는지 UniqueId로만 알려 주는데 Dedicated Server 구성에 따라 이 값이
	// 유효하지 않을 수 있어 식별자로 쓰지 않는다. 대신 살아 있는 연결이 들고 있는 토큰과 대조해
	// 주인이 사라진 예약을 찾는다. UNetConnection::CleanUp이 이 알림보다 먼저 연결을 드라이버에서
	// 제거하므로 방금 끊긴 연결은 이미 목록에 없다.
	ReleaseAbandonedRaidReservationsForServer(FName(TEXT("PendingConnectionLost")));
}

int32 ARaidGameMode::ReleaseAbandonedRaidReservationsForServer(FName Reason)
{
	if (!HasAuthority() || !AdmissionService)
	{
		return 0;
	}

	TSet<FString> LiveReservationTokens;
	CollectLiveReservationTokensForServer(LiveReservationTokens);
	const int32 ReleasedCount = AdmissionService->ReleaseAbandonedClaims(LiveReservationTokens);
	if (ReleasedCount > 0)
	{
		// 원문 (5)의 "서버 매칭 취소"에 해당한다. 아직 commit 전이라 CurrentPlayers는 움직이지 않고
		// 예약 정원만 돌아온다.
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] RaidEntryMatchingCanceled Reason=%s Released=%d LiveConnections=%d CurrentPlayers=%d"),
			*Reason.ToString(),
			ReleasedCount,
			LiveReservationTokens.Num(),
			AdmissionService->GetActivePlayers());
	}
	return ReleasedCount;
}

void ARaidGameMode::CollectLiveReservationTokensForServer(TSet<FString>& OutTokens) const
{
	const UWorld* World = GetWorld();
	const UNetDriver* NetDriver = World ? World->GetNetDriver() : nullptr;
	if (!NetDriver)
	{
		return;
	}

	for (const UNetConnection* Connection : NetDriver->ClientConnections)
	{
		if (!Connection)
		{
			continue;
		}

		// 클라이언트가 NMT_Login에 실어 보낸 URL이 연결에 그대로 남아 있어, 이 연결이 어떤 예약으로
		// 들어왔는지는 여기서만 확인할 수 있다.
		const FString Token = UGameplayStatics::ParseOption(Connection->RequestURL, TEXT("RaidReservation"));
		if (!Token.IsEmpty())
		{
			OutTokens.Add(Token);
		}
	}
}

FString ARaidGameMode::InitNewPlayer(
	APlayerController* NewPlayerController,
	const FUniqueNetIdRepl& UniqueId,
	const FString& Options,
	const FString& Portal)
{
	FString ErrorMessage = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);
	if (!ErrorMessage.IsEmpty() || !bAdmissionRequired)
	{
		return ErrorMessage;
	}

	const FString Token = UGameplayStatics::ParseOption(Options, TEXT("RaidReservation"));
	if (!AdmissionService || !AdmissionService->BindClaimedToken(NewPlayerController, Token))
	{
		return TEXT("RaidReservationInvalid");
	}
	return ErrorMessage;
}

bool ARaidGameMode::EnsureDronePartReturnManagerForServer()
{
	if (!HasAuthority())
	{
		return false;
	}

	if (DronePartReturnManager)
	{
		return true;
	}

	UWorld* World = GetWorld();
	ARaidGameState* GS = World ? World->GetGameState<ARaidGameState>() : nullptr;
	ADronePartInventory* Inventory = GS ? GS->GetDronePartInventory() : nullptr;
	if (!Inventory)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Server] DronePartReturnManager init skipped: DronePartInventory is missing"));
		return false;
	}

	DronePartReturnManager = NewObject<UDronePartReturnManager>(this);
	DronePartReturnManager->Initialize(Inventory);
	UE_LOG(LogTemp, Log, TEXT("[Server] DronePartReturnManager initialized"));
	return true;
}

void ARaidGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	LogControllerPawnState(TEXT("PostLogin"), NewPlayer);

	if (HasAuthority())
	{
		// 예약 서버에서는 정원·상태 판정을 PreLogin의 토큰 claim이 이미 마쳤으므로 여기서는 commit만 한다.
		// 예약이 없는 구성(PIE·리슨)에서만 기존 CanAcceptRaidJoinForServer 게이트를 그대로 쓴다.
		if (bAdmissionRequired)
		{
			if (!AdmissionService || !AdmissionService->CommitForPlayer(NewPlayer))
			{
				UE_LOG(LogTemp, Warning, TEXT("[DR_SUMMARY] RaidJoinRejected PC=%s Reason=ReservationCommitFailed Scope=PostLogin"),
					*BuildRaidGameModeControllerLogString(NewPlayer));
				NewPlayer->Destroy();
				return;
			}
		}
		else
		{
			FName RejectReason;
			if (!CanAcceptRaidJoinForServer(RejectReason))
			{
				UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] RaidJoinRejected PC=%s Reason=%s Scope=PostLogin"),
					*BuildRaidGameModeControllerLogString(NewPlayer),
					RejectReason.IsNone() ? TEXT("Unknown") : *RejectReason.ToString());
				return;
			}
		}

		if (ARaidGameState* GS = GetGameState<ARaidGameState>())
		{
			const bool bLateJoin = GS->RaidState == ERaidState::Battle;
			if (bAdmissionRequired && AdmissionService)
			{
				GS->CurrentPlayers = AdmissionService->GetActivePlayers();
			}
			else
			{
				GS->CurrentPlayers++;
			}
			UE_LOG(LogTemp, Log, TEXT("[Server] PostLogin: CurrentPlayers = %d"), GS->CurrentPlayers);
			if (GS->RaidState == ERaidState::Waiting)
			{
				GS->SetRaidStateForServer(ERaidState::Drafting);
			}
			if (BalanceTelemetry)
			{
				BalanceTelemetry->StartSessionForServer(
					ResolveRaidServerSlot(),
					GetWorld() ? FName(*GetWorld()->GetMapName()) : FName(TEXT("Unknown")),
					ResolveBalanceTelemetryVersion());
				BalanceTelemetry->RecordPlayerJoinedForServer(NewPlayer, bLateJoin, GS->CurrentPlayers);
			}
		}
	}
}

void ARaidGameMode::RestartPlayer(AController* NewPlayer)
{
	Super::RestartPlayer(NewPlayer);
	LogControllerPawnState(TEXT("RestartPlayer"), NewPlayer);

	const APawn* Pawn = NewPlayer ? NewPlayer->GetPawn() : nullptr;
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Spawn PC=%s Pawn=%s PawnClass=%s IsADrone=%s"),
		*BuildRaidGameModeControllerLogString(NewPlayer),
		Pawn ? *Pawn->GetName() : TEXT("None"),
		Pawn ? *Pawn->GetClass()->GetName() : TEXT("None"),
		Pawn && Pawn->IsA<ADrone>() ? TEXT("true") : TEXT("false"));

	if (HasAuthority() && Pawn && Pawn->IsA<ADrone>())
	{
		if (ARaidGameState* RaidGameState = GetWorld() ? GetWorld()->GetGameState<ARaidGameState>() : nullptr)
		{
			if (ARaidBoss* Boss = RaidGameState->GetRaidBoss())
			{
				Boss->NotifyPatternPopulationChangedForServer(FName(TEXT("PlayerSpawnCompleted")));
			}
		}
	}
}

AActor* ARaidGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	if (!Player)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	if (const TWeakObjectPtr<AActor>* ExistingAssignment = PlayerStartAssignments.Find(Player))
	{
		if (ExistingAssignment->IsValid())
		{
			return ExistingAssignment->Get();
		}
		PlayerStartAssignments.Remove(Player);
	}

	for (auto It = PlayerStartAssignments.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || !It.Value().IsValid())
		{
			It.RemoveCurrent();
		}
	}

	TArray<APlayerStart*> Starts;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		APlayerStart* Start = *It;
		if (Start && Start->ActorHasTag(RaidEntrySpawnTag))
		{
			Starts.Add(Start);
		}
	}
	Starts.Sort([](const APlayerStart& Left, const APlayerStart& Right)
	{
		return Left.GetFName().LexicalLess(Right.GetFName());
	});

	if (Starts.Num() == RequiredRaidEntrySpawnCount)
	{
		APlayerStart* SelectedStart = Starts[FMath::RandRange(0, Starts.Num() - 1)];
		PlayerStartAssignments.Add(Player, SelectedStart);
		return SelectedStart;
	}

	UE_LOG(LogTemp, Error,
		TEXT("[DR_SUMMARY] RaidEntrySpawn Result=Fallback Reason=InvalidTaggedStartCount Count=%d Required=%d Player=%s"),
		Starts.Num(),
		RequiredRaidEntrySpawnCount,
		*BuildRaidGameModeControllerLogString(Player));

	AActor* FallbackStart = Super::ChoosePlayerStart_Implementation(Player);
	if (FallbackStart)
	{
		PlayerStartAssignments.Add(Player, FallbackStart);
	}
	return FallbackStart;
}

APawn* ARaidGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	UWorld* World = GetWorld();
	UClass* PawnClass = GetDefaultPawnClassForController(NewPlayer);
	if (!PawnClass)
	{
		PawnClass = ADrone::StaticClass();
	}

	if (!World || !PawnClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Server] SpawnDefaultPawnAtTransform Failed: Player=%s World=%s PawnClass=%s"),
			*BuildRaidGameModeControllerLogString(NewPlayer),
			World ? TEXT("Valid") : TEXT("None"),
			*GetNameSafe(PawnClass));
		NotifyRaidSpawnFailedForServer(NewPlayer, FName(TEXT("SpawnFailed")));
		return nullptr;
	}

	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Instigator = GetInstigator();
	SpawnInfo.ObjectFlags |= RF_Transient;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	APawn* SpawnedPawn = World->SpawnActor<APawn>(PawnClass, SpawnTransform, SpawnInfo);
	bool bUsedFallbackAlwaysSpawn = false;
	if (!SpawnedPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Server] SpawnDefaultPawnAtTransform Retry: Player=%s PawnClass=%s Reason=AdjustIfPossibleButAlwaysSpawn returned null"),
			*BuildRaidGameModeControllerLogString(NewPlayer),
			*GetNameSafe(PawnClass));

		SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		bUsedFallbackAlwaysSpawn = true;
		const FVector SpawnLocation = SpawnTransform.GetLocation();
		const FQuat SpawnRotation = SpawnTransform.GetRotation();
		const FVector SpawnScale = SpawnTransform.GetScale3D();
		const FVector RetryOffsets[] = {
			FVector::ZeroVector,
			FVector(150.0f, 0.0f, 0.0f),
			FVector(-150.0f, 0.0f, 0.0f),
			FVector(0.0f, 150.0f, 0.0f),
			FVector(0.0f, -150.0f, 0.0f),
		};

		for (const FVector& RetryOffset : RetryOffsets)
		{
			const FTransform RetryTransform(SpawnRotation, SpawnLocation + RetryOffset, SpawnScale);
			SpawnedPawn = World->SpawnActor<APawn>(PawnClass, RetryTransform, SpawnInfo);
			if (SpawnedPawn)
			{
				break;
			}
		}
	}

	const FString ResultPawnClassName = SpawnedPawn ? SpawnedPawn->GetClass()->GetName() : GetNameSafe(PawnClass);
	UE_LOG(LogTemp, Log, TEXT("[Server] SpawnDefaultPawnAtTransform Result: Player=%s Pawn=%s PawnClass=%s IsADrone=%s SpawnHandling=AdjustIfPossibleButAlwaysSpawn FallbackAlwaysSpawn=%s Transform=%s"),
		*BuildRaidGameModeControllerLogString(NewPlayer),
		SpawnedPawn ? *SpawnedPawn->GetName() : TEXT("None"),
		*ResultPawnClassName,
		SpawnedPawn && SpawnedPawn->IsA<ADrone>() ? TEXT("true") : TEXT("false"),
		bUsedFallbackAlwaysSpawn ? TEXT("true") : TEXT("false"),
		*SpawnTransform.ToHumanReadableString());

	if (!SpawnedPawn)
	{
		NotifyRaidSpawnFailedForServer(NewPlayer, FName(TEXT("SpawnFailed")));
	}

	return SpawnedPawn;
}

bool ARaidGameMode::NotifyRaidSpawnFailedForServer(AController* Controller, FName Reason) const
{
	if (!HasAuthority())
	{
		return false;
	}

	ARaidPlayerController* RaidPC = Cast<ARaidPlayerController>(Controller);
	if (!RaidPC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DR_SUMMARY] RaidLoadFailed NotifySkipped Reason=InvalidPlayerController PC=%s"),
			*BuildRaidGameModeControllerLogString(Controller));
		return false;
	}

	const FName FailureReason = Reason.IsNone() ? FName(TEXT("SpawnFailed")) : Reason;
	RaidPC->Client_NotifyRaidLoadFailed(FailureReason, FName(TEXT("LobbyMap")));
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] RaidLoadFailed NotifySent Player=%s Reason=%s TargetMap=LobbyMap"),
		*BuildRaidGameModeControllerLogString(RaidPC),
		*FailureReason.ToString());
	return true;
}

void ARaidGameMode::Logout(AController* Exiting)
{
	PlayerStartAssignments.Remove(Exiting);

	if (HasAuthority())
	{
		if (ARaidPlayerController* RaidPC = Cast<ARaidPlayerController>(Exiting))
		{
			RaidPC->ClearBossTargetForServer(FName(TEXT("Cleanup")));
			if (RaidPC->GetPlayerSelectionState() == EPlayerSelectionState::Selecting)
			{
				UE_LOG(LogTemp, Log, TEXT("[Server] Logout return path: Player=%s PlayerSelectionState=Selecting Source=SelectedParts"),
					*RaidPC->GetName());
				RaidPC->ReturnSelectedPartsForServer(EDronePartReturnReason::Disconnect);
			}
			else
			{
				UE_LOG(LogTemp, Log, TEXT("[Server] Logout return path: Player=%s PlayerSelectionState=%d Source=EquippedParts"),
					*RaidPC->GetName(),
					static_cast<int32>(RaidPC->GetPlayerSelectionState()));
				RaidPC->ReturnEquippedPartsForServer(EDronePartReturnReason::Disconnect);
			}

			// PlayerController가 파괴되면 슬롯 정보가 사라져 복구 수단이 없어진다. 파괴 전 마지막 재처리 기회다.
			if (EnsureDronePartReturnManagerForServer() && DronePartReturnManager)
			{
				DronePartReturnManager->RetryPendingReturnsForServer(RaidPC, FName(TEXT("Logout")));
			}

			if (ADrone* Drone = Cast<ADrone>(RaidPC->GetPawn()))
			{
				Drone->ClearEquippedLoadoutForServer(FName(TEXT("Disconnect")));
			}
		}

		if (ARaidGameState* GS = GetGameState<ARaidGameState>())
		{
			if (bAdmissionRequired)
			{
				if (AdmissionService && AdmissionService->ReleasePlayer(Exiting))
				{
					GS->CurrentPlayers = AdmissionService->GetActivePlayers();
				}
			}
			else
			{
				GS->CurrentPlayers = FMath::Max(0, GS->CurrentPlayers - 1);
			}
			UE_LOG(LogTemp, Log, TEXT("[Server] Logout: CurrentPlayers = %d"), GS->CurrentPlayers);

			// 정원 회수가 끝난 뒤에 찍는다. 종전에는 반환 처리보다 앞에서 방출해
			// CurrentPlayers를 실을 수 없었고, 테스터 로그만으로는 인원 증감을
			// 맞춰볼 수 없었다(계획서 G3 "접속/이탈이 정원에 정확히 반영된다").
			// ExitReason이 항상 Disconnect인 것은 알려진 한계다 — 리포트 후 정상 복귀는
			// 클라이언트 ClientTravel이라 서버에는 연결 종료로만 보인다. 구분하려면
			// 클라이언트가 복귀 의사를 알리는 경로가 따로 필요하다.
			if (BalanceTelemetry)
			{
				if (ARaidPlayerController* LeavingPC = Cast<ARaidPlayerController>(Exiting))
				{
					BalanceTelemetry->EmitForServer(TEXT("PlayerLeft"), {
						{TEXT("Player"), BalanceTelemetry->GetOrAssignPlayerAliasForServer(LeavingPC)},
						{TEXT("ExitReason"), TEXT("Disconnect")},
						{TEXT("CurrentPlayers"), FString::FromInt(GS->CurrentPlayers)},
					});
				}
			}
		}
	}

	Super::Logout(Exiting);

	if (HasAuthority())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimerForNextTick(
				FTimerDelegate::CreateUObject(this, &ARaidGameMode::NotifyBossPatternPopulationAfterLogoutForServer));
		}
	}
}

void ARaidGameMode::NotifyBossPatternPopulationAfterLogoutForServer()
{
	if (!HasAuthority())
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<ARaidBoss> It(World); It; ++It)
		{
			It->NotifyPatternPopulationChangedForServer(FName(TEXT("Logout")));
		}
	}
}

void ARaidGameMode::ReturnAllEquippedPartsForRaidEnd(FName Reason)
{
	if (!HasAuthority())
	{
		return;
	}

	if (!EnsureDronePartReturnManagerForServer())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Server] RaidEnd return skipped: DronePartReturnManager is missing"));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	ARaidGameState* GS = World->GetGameState<ARaidGameState>();
	if (GS && GS->RaidState == ERaidState::End)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] RaidEndSkipped Reason=AlreadyEnded RequestedReason=%s"),
			Reason.IsNone() ? TEXT("Manual") : *Reason.ToString());
		return;
	}

	const FString ReasonText = Reason.IsNone() ? TEXT("Manual") : Reason.ToString();
	const EDroneReportTrigger ReportTrigger = ResolveReportTriggerForRaidEndReason(Reason);
	const bool bBossDefeated = ReportTrigger == EDroneReportTrigger::BossDefeated;
	ClearRaidTimeLimitTimerForServer(Reason.IsNone() ? FName(TEXT("RaidEnd")) : Reason);
	StopBossPatternsForServer(Reason.IsNone() ? FName(TEXT("RaidEnd")) : Reason);
	int32 EligiblePlayerCount = 0;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (ARaidPlayerController* RaidPC = Cast<ARaidPlayerController>(It->Get()))
		{
			const EPlayerSelectionState SelectionState = RaidPC->GetPlayerSelectionState();
			if (SelectionState == EPlayerSelectionState::Selecting
				|| SelectionState == EPlayerSelectionState::InBattle
				|| SelectionState == EPlayerSelectionState::Locked)
			{
				EligiblePlayerCount++;
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] RaidEnd Reason=%s PlayerCount=%d"),
		*ReasonText,
		EligiblePlayerCount);

	if (GS)
	{
		GS->SetRaidStateForServer(ERaidState::End);
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (ARaidPlayerController* RaidPC = Cast<ARaidPlayerController>(It->Get()))
		{
			const FName TargetClearReason = Reason == FName(TEXT("BossDefeated")) ? FName(TEXT("BossDead")) : FName(TEXT("RaidEnd"));
			RaidPC->ClearBossTargetForServer(TargetClearReason);
			const EPlayerSelectionState SelectionState = RaidPC->GetPlayerSelectionState();
			if (SelectionState == EPlayerSelectionState::Selecting)
			{
				DronePartReturnManager->ReturnSelectedParts(RaidPC, EDronePartReturnReason::RaidEnd);
				if (ADrone* Drone = Cast<ADrone>(RaidPC->GetPawn()))
				{
					Drone->ClearEquippedLoadoutForServer(FName(TEXT("RaidEnd")));
				}
				RaidPC->FinalizeRaidEndForServer(Reason.IsNone() ? FName(TEXT("RaidEnd")) : Reason);
				continue;
			}

			if (SelectionState != EPlayerSelectionState::InBattle && SelectionState != EPlayerSelectionState::Locked)
			{
				continue;
			}

			RaidPC->TryCreateDroneReportForServer(ReportTrigger, bBossDefeated);
			UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ReturnAfterReport Player=%s Trigger=%s"),
				*BuildRaidGameModeControllerLogString(RaidPC),
				ToReportTriggerLogString(ReportTrigger));

			DronePartReturnManager->ReturnEquippedParts(RaidPC, EDronePartReturnReason::RaidEnd);
			if (ADrone* Drone = Cast<ADrone>(RaidPC->GetPawn()))
			{
				Drone->ResetCombatRuntimeStateForServer();
				Drone->ClearEquippedLoadoutForServer(FName(TEXT("RaidEnd")));
			}
			RaidPC->FinalizeRaidEndForServer(Reason.IsNone() ? FName(TEXT("RaidEnd")) : Reason);
		}
	}

	// 레이드 종료는 공유 재고를 되돌릴 마지막 경계다. 여기서 실패한 반환은 이후 회수할 트리거가 없다.
	DronePartReturnManager->RetryPendingReturnsForServer(nullptr, Reason.IsNone() ? FName(TEXT("RaidEnd")) : Reason);

	SetAllBossStatesForServer(EBossState::Clear, Reason.IsNone() ? FName(TEXT("RaidEnd")) : Reason);
	if (BalanceTelemetry)
	{
		const ARaidBoss* Boss = GS ? GS->GetRaidBoss() : nullptr;
		BalanceTelemetry->EndSessionForServer(
			Reason.IsNone() ? FName(TEXT("Manual")) : Reason,
			EligiblePlayerCount,
			Boss ? Boss->GetCurrentHP() : 0.0f);
	}
	ResetBossDamageContributionsForServer(Reason.IsNone() ? FName(TEXT("RaidEnd")) : Reason);

	UE_LOG(LogTemp, Log, TEXT("[Server] RaidEnd part return completed Reason=%s PlayerCount=%d"),
		*ReasonText,
		EligiblePlayerCount);
}

void ARaidGameMode::HandleBossDefeatedForServer()
{
	if (!HasAuthority())
	{
		return;
	}

	ReturnAllEquippedPartsForRaidEnd(FName(TEXT("BossDefeated")));
}

void ARaidGameMode::StartBossPatternsForServer()
{
	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<ARaidBoss> It(World); It; ++It)
	{
		It->StartBossPatternForServer();
	}
}

bool ARaidGameMode::ShouldVisualizePatternHitGeometry() const
{
	return false;
}

void ARaidGameMode::StopBossPatternsForServer(FName Reason)
{
	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<ARaidBoss> It(World); It; ++It)
	{
		It->StopBossPatternForServer(Reason);
	}
}

bool ARaidGameMode::CanAcceptRaidJoinForServer(FName& OutRejectReason, bool bCheckNewPlayerCapacity) const
{
	OutRejectReason = NAME_None;
	if (!HasAuthority())
	{
		OutRejectReason = FName(TEXT("NotAuthority"));
		return false;
	}

	if (bRaidTimeLimitExpiredForServer)
	{
		OutRejectReason = FName(TEXT("TimeOver"));
		return false;
	}

	UWorld* World = GetWorld();
	const ARaidGameState* GS = World ? World->GetGameState<ARaidGameState>() : nullptr;
	if (!GS)
	{
		OutRejectReason = FName(TEXT("NoRaidState"));
		return false;
	}

	if (bCheckNewPlayerCapacity && GS->CurrentPlayers >= 16)
	{
		OutRejectReason = FName(TEXT("Full"));
		return false;
	}

	const ARaidBoss* Boss = GS->GetRaidBoss();
	if (!Boss)
	{
		for (TActorIterator<ARaidBoss> It(World); It; ++It)
		{
			Boss = *It;
			break;
		}
	}

	if (Boss)
	{
		if (Boss->GetBossState() == EBossState::Clear)
		{
			OutRejectReason = FName(TEXT("BossClear"));
			return false;
		}

		if (Boss->GetBossState() == EBossState::Dead || Boss->IsDefeated())
		{
			OutRejectReason = FName(TEXT("BossDead"));
			return false;
		}
	}

	if (GS->RaidState != ERaidState::Waiting
		&& GS->RaidState != ERaidState::Drafting
		&& GS->RaidState != ERaidState::Battle)
	{
		OutRejectReason = GS->RaidState == ERaidState::End
			? FName(TEXT("RaidEnded"))
			: FName(TEXT("InvalidRaidState"));
		return false;
	}

	return true;
}

void ARaidGameMode::SetAllBossStatesForServer(EBossState NewBossState, FName Reason)
{
	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<ARaidBoss> It(World); It; ++It)
	{
		It->SetBossStateForServer(NewBossState, Reason);
	}
}

bool ARaidGameMode::IsRaidTimeLimitTimerActiveForServer() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimerManager().IsTimerActive(RaidTimeLimitTimerHandle);
}

bool ARaidGameMode::DetectAndRecoverRaidTimerMismatchForServer()
{
	if (!HasAuthority())
	{
		return false;
	}

	UWorld* World = GetWorld();
	const ARaidGameState* GS = World ? World->GetGameState<ARaidGameState>() : nullptr;
	if (!World || !GS)
	{
		return false;
	}

	// Battle이 아니면 타이머가 없는 것이 정상이다. 특히 End에서 되살리면 종료된 레이드가 다시 돈다.
	if (GS->RaidState != ERaidState::Battle)
	{
		return false;
	}

	// 이미 만료돼 종료 절차가 도는 중이면 재시작이 아니라 그대로 두는 것이 맞다.
	if (bRaidTimeLimitExpiredForServer || IsRaidTimeLimitTimerActiveForServer())
	{
		return false;
	}

	// 원문 예외 4.5는 재시작과 전투 시작 롤백을 모두 허용한다. 롤백은 이미 입장한 플레이어의
	// 전투 상태까지 되돌려야 해 부작용이 크므로 재시작을 택한다.
	UE_LOG(LogTemp, Warning, TEXT("[DR_SUMMARY] RaidTimerMismatch Reason=BattleWithoutTimer Recovery=Restart RaidState=%d"),
		static_cast<int32>(GS->RaidState));
	StartRaidTimeLimitTimerForServer();
	return true;
}

void ARaidGameMode::HandleRaidTimerWatchdogTickForServer()
{
	DetectAndRecoverRaidTimerMismatchForServer();
}

void ARaidGameMode::StartRaidTimeLimitTimerForServer()
{
	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	ARaidGameState* GS = World ? World->GetGameState<ARaidGameState>() : nullptr;
	if (!World || !GS)
	{
		return;
	}

	if (GS->RaidState == ERaidState::End)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] RaidTimerSkipped Phase=Start Reason=RaidEnded"));
		return;
	}

	if (GS->RaidState != ERaidState::Battle)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] RaidTimerSkipped Phase=Start Reason=NotBattle State=%d"),
			static_cast<int32>(GS->RaidState));
		return;
	}

	if (World->GetTimerManager().IsTimerActive(RaidTimeLimitTimerHandle))
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] RaidTimerSkipped Phase=Start Reason=AlreadyActive Remaining=%.2f"),
			World->GetTimerManager().GetTimerRemaining(RaidTimeLimitTimerHandle));
		return;
	}

	const float ClampedTimeLimit = FMath::Max(0.01f, RaidTimeLimitSeconds);
	GS->SetRaidTimeEndServerTimeForServer(World->GetTimeSeconds() + ClampedTimeLimit);
	World->GetTimerManager().SetTimer(
		RaidTimeLimitTimerHandle,
		this,
		&ARaidGameMode::HandleRaidTimeLimitExpiredForServer,
		ClampedTimeLimit,
		false);
	bRaidTimeLimitExpiredForServer = false;

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] RaidTimerStart Duration=%.2f EndServerTime=%.2f"),
		ClampedTimeLimit,
		GS->GetRaidTimeEndServerTime());
}

void ARaidGameMode::ClearRaidTimeLimitTimerForServer(FName Reason)
{
	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (ARaidGameState* GS = World->GetGameState<ARaidGameState>())
	{
		GS->SetRaidTimeEndServerTimeForServer(0.0f);
	}

	if (World->GetTimerManager().TimerExists(RaidTimeLimitTimerHandle))
	{
		World->GetTimerManager().ClearTimer(RaidTimeLimitTimerHandle);
		UE_LOG(LogTemp, Log, TEXT("[Server] RaidTimeLimit timer cleared Reason=%s"),
			Reason.IsNone() ? TEXT("RaidEnd") : *Reason.ToString());
	}
}

void ARaidGameMode::HandleRaidTimeLimitExpiredForServer()
{
	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	ARaidGameState* GS = World ? World->GetGameState<ARaidGameState>() : nullptr;
	if (!World || !GS)
	{
		return;
	}

	if (GS->RaidState == ERaidState::End)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] RaidTimerExpired Result=Ignored Reason=AlreadyEnded"));
		return;
	}

	if (GS->RaidState != ERaidState::Battle)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] RaidTimerExpired Result=Ignored Reason=NotBattle State=%d"),
			static_cast<int32>(GS->RaidState));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] RaidTimerExpired Result=RaidEnd Reason=RaidTimeLimit"));
	bRaidTimeLimitExpiredForServer = true;
	ReturnAllEquippedPartsForRaidEnd(FName(TEXT("RaidTimeLimit")));
}

#if WITH_DEV_AUTOMATION_TESTS
bool ARaidGameMode::IsRaidTimeLimitTimerActiveForTest() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimerManager().IsTimerActive(RaidTimeLimitTimerHandle);
}

void ARaidGameMode::ExpireRaidTimeLimitForTest()
{
	HandleRaidTimeLimitExpiredForServer();
}

bool ARaidGameMode::NotifyRaidSpawnFailedForTest(AController* Controller, FName Reason)
{
	const bool bNotified = NotifyRaidSpawnFailedForServer(Controller, Reason);
	if (bNotified)
	{
		const FName FailureReason = Reason.IsNone() ? FName(TEXT("SpawnFailed")) : Reason;
		if (ARaidPlayerController* RaidPC = Cast<ARaidPlayerController>(Controller))
		{
			RaidPC->HandleRaidLoadFailedForClient(FailureReason, FName(TEXT("LobbyMap")));
		}
	}
	return bNotified;
}

void ARaidGameMode::SetAdmissionServiceForTest(URaidServerAdmissionService* InService, bool bInAdmissionRequired)
{
	AdmissionService = InService;
	bAdmissionRequired = bInAdmissionRequired;
}

void ARaidGameMode::ValidateRaidAdmissionForTest(const FString& Options, FString& OutErrorMessage)
{
	OutErrorMessage.Reset();
	ValidateRaidAdmission(Options, OutErrorMessage);
}
#endif

UDronePartReturnManager* ARaidGameMode::GetDronePartReturnManager() const
{
	return DronePartReturnManager;
}

UBalanceTelemetryComponent* ARaidGameMode::GetBalanceTelemetryForServer() const
{
	return HasAuthority() ? BalanceTelemetry : nullptr;
}

bool ARaidGameMode::RecordBossDamageForServer(APlayerController* PlayerController, float DamageAmount)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ContributionIgnored Reason=NotAuthority Player=%s Damage=%.2f"),
			*BuildRaidGameModeControllerLogString(PlayerController),
			DamageAmount);
		return false;
	}

	if (DamageAmount <= KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ContributionIgnored Reason=NoDamage Player=%s Damage=%.2f"),
			*BuildRaidGameModeControllerLogString(PlayerController),
			DamageAmount);
		return false;
	}

	ARaidPlayerController* RaidPC = Cast<ARaidPlayerController>(PlayerController);
	if (!RaidPC)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ContributionIgnored Reason=InvalidPlayer Player=%s Damage=%.2f"),
			*BuildRaidGameModeControllerLogString(PlayerController),
			DamageAmount);
		return false;
	}

	const FString PlayerKey = BuildStablePlayerKeyForServer(RaidPC);
	if (PlayerKey.IsEmpty())
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ContributionIgnored Reason=InvalidPlayerKey Player=%s Damage=%.2f"),
			*BuildRaidGameModeControllerLogString(RaidPC),
			DamageAmount);
		return false;
	}

	float& TotalDamage = PlayerBossDamageMap.FindOrAdd(PlayerKey);
	TotalDamage += DamageAmount;
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ContributionDamage Player=%s Key=%s Added=%.2f Total=%.2f"),
		*BuildRaidGameModeControllerLogString(RaidPC),
		*PlayerKey,
		DamageAmount,
		TotalDamage);
	return true;
}

float ARaidGameMode::GetBossDamageForPlayerKeyForServer(const FString& PlayerKey) const
{
	if (!HasAuthority() || PlayerKey.IsEmpty())
	{
		return 0.0f;
	}

	if (const float* Damage = PlayerBossDamageMap.Find(PlayerKey))
	{
		return *Damage;
	}
	return 0.0f;
}

TArray<FDroneBossDamageContribution> ARaidGameMode::GetSortedBossDamageContributionsForServer() const
{
	TArray<FDroneBossDamageContribution> Contributions;
	if (!HasAuthority())
	{
		return Contributions;
	}

	Contributions.Reserve(PlayerBossDamageMap.Num());
	for (const TPair<FString, float>& Pair : PlayerBossDamageMap)
	{
		FDroneBossDamageContribution Contribution;
		Contribution.PlayerKey = Pair.Key;
		Contribution.Damage = Pair.Value;
		Contributions.Add(Contribution);
	}

	Contributions.Sort([](const FDroneBossDamageContribution& Left, const FDroneBossDamageContribution& Right)
	{
		if (FMath::IsNearlyEqual(Left.Damage, Right.Damage, 0.001f))
		{
			return Left.PlayerKey < Right.PlayerKey;
		}
		return Left.Damage > Right.Damage;
	});
	return Contributions;
}

bool ARaidGameMode::SaveDroneReportDataForServer(ARaidPlayerController* RaidPC, const FDroneReportData& ReportData)
{
	if (!HasAuthority())
	{
		return false;
	}

	DroneReportDataList.Add(ReportData);
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ReportSaved Player=%s Callsign=%s Grade=%d ReportScore=%.2f StoredCount=%d"),
		*GetNameSafe(RaidPC),
		*ReportData.Callsign,
		static_cast<int32>(ReportData.Grade),
		ReportData.ReportScore,
		DroneReportDataList.Num());
	return true;
}

const TArray<FDroneReportData>& ARaidGameMode::GetDroneReportDataListForServer() const
{
	return DroneReportDataList;
}

void ARaidGameMode::ClearDroneReportDataListForServer(FName Reason)
{
	if (!HasAuthority() || DroneReportDataList.IsEmpty())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ReportStoreCleared Reason=%s ClearedCount=%d"),
		Reason.IsNone() ? TEXT("Unknown") : *Reason.ToString(),
		DroneReportDataList.Num());
	DroneReportDataList.Reset();
}

void ARaidGameMode::ResetBossDamageContributionsForServer(FName Reason)
{
	if (!HasAuthority())
	{
		return;
	}

	const int32 PreviousCount = PlayerBossDamageMap.Num();
	PlayerBossDamageMap.Reset();
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ContributionReset Reason=%s PreviousCount=%d"),
		Reason.IsNone() ? TEXT("Unknown") : *Reason.ToString(),
		PreviousCount);
}

FString ARaidGameMode::BuildStablePlayerKeyForServer(const APlayerController* PlayerController)
{
	if (!PlayerController)
	{
		return FString();
	}

	if (const APlayerState* PS = PlayerController->PlayerState)
	{
		const FUniqueNetIdRepl& UniqueId = PS->GetUniqueId();
		if (UniqueId.IsValid())
		{
			return FString::Printf(TEXT("UID:%s"), *UniqueId.ToString());
		}
		return FString::Printf(TEXT("PID:%d"), PS->GetPlayerId());
	}

	// Offline/test worlds may not provide PlayerState, so isolate by PC name.
	return FString::Printf(TEXT("PC:%s"), *PlayerController->GetName());
}

bool ARaidGameMode::TryMarkDroneReportGeneratedForServer(ARaidPlayerController* RaidPC)
{
	if (!HasAuthority() || !RaidPC)
	{
		return false;
	}

	const FString PlayerKey = BuildStablePlayerKeyForServer(RaidPC);
	if (PlayerKey.IsEmpty())
	{
		return false;
	}

	if (GeneratedDroneReportPlayerKeys.Contains(PlayerKey))
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] DroneReport DuplicateIgnored: Player=%s Reason=AlreadyGeneratedForPlayerKey Key=%s"),
			*RaidPC->GetName(),
			*PlayerKey);
		return false;
	}

	GeneratedDroneReportPlayerKeys.Add(PlayerKey);
	return true;
}

void ARaidGameMode::ClearDroneReportKeyForServer(ARaidPlayerController* RaidPC, FName Reason)
{
	if (!HasAuthority() || !RaidPC)
	{
		return;
	}

	const FString PlayerKey = BuildStablePlayerKeyForServer(RaidPC);
	if (!PlayerKey.IsEmpty() && GeneratedDroneReportPlayerKeys.Remove(PlayerKey) > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] DroneReport KeyCleared: Player=%s Key=%s Reason=%s"),
			*RaidPC->GetName(),
			*PlayerKey,
			Reason.IsNone() ? TEXT("Unknown") : *Reason.ToString());
	}
}
