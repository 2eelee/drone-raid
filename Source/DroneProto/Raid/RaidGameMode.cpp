#include "RaidGameMode.h"
#include "Drone.h"
#include "DronePartInventory.h"
#include "DronePartReturnManager.h"
#include "RaidBoss.h"
#include "RaidGameState.h"
#include "RaidPlayerController.h"
#include "RaidPlayerState.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"

namespace
{
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

	if (!GS->GetRaidBoss())
	{
		ARaidBoss* ExistingBoss = nullptr;
		for (TActorIterator<ARaidBoss> It(GetWorld()); It; ++It)
		{
			ExistingBoss = *It;
			break;
		}

		if (!ExistingBoss)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = GS;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			ExistingBoss = GetWorld()->SpawnActor<ARaidBoss>(
				ARaidBoss::StaticClass(),
				FVector(600.0f, 0.0f, 100.0f),
				FRotator::ZeroRotator,
				SpawnParams);
		}

		if (ExistingBoss)
		{
			GS->SetRaidBossForServer(ExistingBoss);
			UE_LOG(LogTemp, Log, TEXT("[Server] RaidBoss ready: Name=%s Replicates=%s"),
				*ExistingBoss->GetName(),
				ExistingBoss->GetIsReplicated() ? TEXT("true") : TEXT("false"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[Server] RaidBoss spawn failed"));
		}
	}
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
		if (ARaidGameState* GS = GetGameState<ARaidGameState>())
		{
			GS->CurrentPlayers++;
			UE_LOG(LogTemp, Log, TEXT("[Server] PostLogin: CurrentPlayers = %d"), GS->CurrentPlayers);
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

	return SpawnedPawn;
}

void ARaidGameMode::Logout(AController* Exiting)
{
	if (HasAuthority())
	{
		if (ARaidPlayerController* RaidPC = Cast<ARaidPlayerController>(Exiting))
		{
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

			if (ADrone* Drone = Cast<ADrone>(RaidPC->GetPawn()))
			{
				Drone->ClearEquippedLoadoutForServer(FName(TEXT("Disconnect")));
			}
		}

		if (ARaidGameState* GS = GetGameState<ARaidGameState>())
		{
			GS->CurrentPlayers--;
			UE_LOG(LogTemp, Log, TEXT("[Server] Logout: CurrentPlayers = %d"), GS->CurrentPlayers);
		}
	}

	Super::Logout(Exiting);
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
	World->GetTimerManager().SetTimer(
		RaidTimeLimitTimerHandle,
		this,
		&ARaidGameMode::HandleRaidTimeLimitExpiredForServer,
		ClampedTimeLimit,
		false);

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] RaidTimerStart Duration=%.2f"),
		ClampedTimeLimit);
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
#endif

UDronePartReturnManager* ARaidGameMode::GetDronePartReturnManager() const
{
	return DronePartReturnManager;
}
