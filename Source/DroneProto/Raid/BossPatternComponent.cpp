#include "BossPatternComponent.h"
#include "BalanceTelemetryComponent.h"

#include "BossPatternActorBase.h"
#include "BossPatternDataTableResolver.h"
#include "CorruptedActinoPatternActor.h"
#include "Drone.h"
#include "RaidBoss.h"
#include "RaidGameMode.h"
#include "RaidGameState.h"
#include "RaidPlayerController.h"
#include "StellarRemnantPatternActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/DataTable.h"
#include "GameFramework/GameStateBase.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h"

namespace
{
const TCHAR* ToPatternName(EBossPatternKind PatternKind)
{
	switch (PatternKind)
	{
	case EBossPatternKind::CorruptedActino:
		return TEXT("CorruptedActino");
	case EBossPatternKind::StellarRemnant:
		return TEXT("StellarRemnant");
	default:
		return TEXT("None");
	}
}
}

UBossPatternComponent::UBossPatternComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	static ConstructorHelpers::FObjectFinder<UDataTable> BossPatternTableFinder(
		TEXT("/Game/Data/BossPattern/DT_BossPattern.DT_BossPattern"));
	static ConstructorHelpers::FObjectFinder<UDataTable> CorruptedTableFinder(
		TEXT("/Game/Data/BossPattern/DT_CorruptedActino.DT_CorruptedActino"));
	static ConstructorHelpers::FObjectFinder<UDataTable> PresetTableFinder(
		TEXT("/Game/Data/BossPattern/DT_CorruptedActinoPreset.DT_CorruptedActinoPreset"));
	static ConstructorHelpers::FObjectFinder<UDataTable> StellarTableFinder(
		TEXT("/Game/Data/BossPattern/DT_StellarRemnant.DT_StellarRemnant"));
	BossPatternDataTable = BossPatternTableFinder.Object;
	CorruptedActinoDataTable = CorruptedTableFinder.Object;
	CorruptedActinoPresetDataTable = PresetTableFinder.Object;
	StellarRemnantDataTable = StellarTableFinder.Object;
}

void UBossPatternComponent::BeginPlay()
{
	Super::BeginPlay();
	ResolvePatternData();
}

void UBossPatternComponent::ResolvePatternData()
{
	if (bResolvedConfigReady)
	{
		return;
	}

	const FBossPatternDataTableSet Tables =
	{
		BossPatternDataTable,
		CorruptedActinoDataTable,
		CorruptedActinoPresetDataTable,
		StellarRemnantDataTable
	};
	FBossPatternResolvedConfig Candidate;
	EBossPatternDataFallbackReason Reason = EBossPatternDataFallbackReason::None;
	const bool bResolvedFromDataTable = BossPatternData::TryResolve(Tables, Candidate, Reason);
	ResolvedConfig = bResolvedFromDataTable ? Candidate : MakeCanonicalBossPatternResolvedConfig();
	bResolvedConfigReady = true;

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		if (bResolvedFromDataTable)
		{
			UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossPatternData Source=DataTable"));
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossPatternData Source=Fallback Reason=%s"),
				BossPatternData::ToString(Reason));
		}
	}
}

bool UBossPatternComponent::CopyResolvedConfig(FBossPatternResolvedConfig& OutConfig) const
{
	if (!bResolvedConfigReady)
	{
		return false;
	}
	OutConfig = ResolvedConfig;
	return true;
}

bool UBossPatternComponent::StartForServer()
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !Owner->HasAuthority() || !World || bRunning || !bResolvedConfigReady)
	{
		return false;
	}

	// 원문 예외 4.1(PATTERN-11): BossID 또는 Boss 객체가 Null이면 패턴 요청을 거부하고
	// 레이드 상태를 확인한다. 지금까지는 owner cast 실패가 조용히 통과해, 보스가 아닌 액터에
	// 붙은 컴포넌트도 패턴을 시작할 수 있었다.
	if (!Cast<ARaidBoss>(Owner))
	{
		const ARaidGameState* RaidGameState = World->GetGameState<ARaidGameState>();
		UE_LOG(LogTemp, Warning, TEXT("[DR_SUMMARY] BossPattern StartRejected Reason=BossNull Owner=%s RaidState=%d"),
			*Owner->GetName(),
			RaidGameState ? static_cast<int32>(RaidGameState->RaidState) : -1);
		return false;
	}

	bRunning = true;
	ActivePlayerCount = CountActivePlayersForServer();
	if (ActivePlayerCount <= 0)
	{
		PauseForNoPlayersForServer(FName(TEXT("InitialStart")));
		return true;
	}

	ServerState = EBossPatternServerState::FirstDelay;
	CurrentPattern = EBossPatternKind::None;
	NextPattern = EBossPatternKind::CorruptedActino;
	ScheduleTransition(ResolvedConfig.Common.FirstDelaySeconds);
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossPattern State=FirstDelay Boss=%s Delay=%.2f InstanceID=%d"),
		*Owner->GetName(),
		PendingDelaySeconds,
		TransitionSerial);
	return true;
}

void UBossPatternComponent::StopForServer(FName Reason)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	const bool bWasRunning = bRunning || ActivePatternActor != nullptr;
	++TransitionSerial;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TransitionTimerHandle);
	}
	DestroyActivePatternActorForServer();
	ClearAllHitLocksForServer();
	bRunning = false;
	ServerState = EBossPatternServerState::Stopped;
	CurrentPattern = EBossPatternKind::None;
	NextPattern = EBossPatternKind::CorruptedActino;
	PendingDelaySeconds = 0.0f;
	ActivePlayerCount = -1;

	if (bWasRunning)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossPattern State=Stopped Boss=%s Cleanup Reason=%s InstanceID=%d"),
			*Owner->GetName(),
			Reason.IsNone() ? TEXT("Unknown") : *Reason.ToString(),
			TransitionSerial);
	}
}

bool UBossPatternComponent::IsRunning() const
{
	return bRunning;
}

bool UBossPatternComponent::TryApplyPatternDamageForServer(ADrone* Target, int32 DamageAmount)
{
	AActor* Owner = GetOwner();
	const ARaidBoss* Boss = Cast<ARaidBoss>(Owner);
	ARaidPlayerController* PlayerController = Target ? Cast<ARaidPlayerController>(Target->GetController()) : nullptr;
	const ARaidGameState* RaidGameState = GetWorld() ? GetWorld()->GetGameState<ARaidGameState>() : nullptr;
	if (!Owner || !Owner->HasAuthority() || !bRunning || ServerState != EBossPatternServerState::Active
		|| !Boss || Boss->IsDefeated() || Boss->GetBossState() != EBossState::Battle || !Target || !Target->HasAuthority() || DamageAmount <= 0
		|| Target->IsDead() || !PlayerController || PlayerController->GetPawn() != Target
		|| PlayerController->GetPlayerSelectionState() != EPlayerSelectionState::InBattle
		|| !RaidGameState || RaidGameState->RaidState != ERaidState::Battle)
	{
		return false;
	}

	const FString PlayerKey = ARaidGameMode::BuildStablePlayerKeyForServer(PlayerController);
	if (PlayerKey.IsEmpty())
	{
		return false;
	}

	if (Target->IsInvincibleForDamage())
	{
		const bool bFirstIgnoredContact = !DodgeIgnoredLoggedPlayerKeys.Contains(PlayerKey);
		if (bFirstIgnoredContact)
		{
			DodgeIgnoredLoggedPlayerKeys.Add(PlayerKey);
			UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossPattern HitIgnored Reason=DodgeInvincible Pattern=%s Player=%s"),
				ToPatternName(CurrentPattern),
				*PlayerController->GetName());
			if (UBalanceTelemetryComponent* Telemetry = UBalanceTelemetryComponent::FindForServer(this))
			{
				Telemetry->EmitForServer(TEXT("PatternContactResolved"), {
					{TEXT("Player"), Telemetry->GetOrAssignPlayerAliasForServer(PlayerController)},
					{TEXT("Pattern"), ToPatternName(CurrentPattern)},
					{TEXT("PatternInstance"), FString::FromInt(ActiveTelemetryPatternInstanceID)},
					{TEXT("Result"), TEXT("Avoided")},
					{TEXT("Reason"), TEXT("DodgeInvincible")},
					{TEXT("AppliedDamage"), TEXT("0")},
					{TEXT("HPBefore"), FString::FromInt(Target->GetHealth())},
					{TEXT("HPAfter"), FString::FromInt(Target->GetHealth())},
					{TEXT("Killed"), TEXT("0")},
				});
			}
		}
		return false;
	}
	DodgeIgnoredLoggedPlayerKeys.Remove(PlayerKey);

	if (HitLockTimerHandles.Contains(PlayerKey))
	{
		const bool bFirstSuppressedContact = !HitLockIgnoredLoggedPlayerKeys.Contains(PlayerKey);
		if (bFirstSuppressedContact)
		{
			HitLockIgnoredLoggedPlayerKeys.Add(PlayerKey);
			UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossPattern HitIgnored Reason=PatternHitLock Pattern=%s Player=%s Key=%s"),
				ToPatternName(CurrentPattern),
				*PlayerController->GetName(),
				*PlayerKey);
			if (UBalanceTelemetryComponent* Telemetry = UBalanceTelemetryComponent::FindForServer(this))
			{
				Telemetry->EmitForServer(TEXT("PatternContactResolved"), {
					{TEXT("Player"), Telemetry->GetOrAssignPlayerAliasForServer(PlayerController)},
					{TEXT("Pattern"), ToPatternName(CurrentPattern)},
					{TEXT("PatternInstance"), FString::FromInt(ActiveTelemetryPatternInstanceID)},
					{TEXT("Result"), TEXT("Suppressed")},
					{TEXT("Reason"), TEXT("PatternHitLock")},
					{TEXT("AppliedDamage"), TEXT("0")},
					{TEXT("HPBefore"), FString::FromInt(Target->GetHealth())},
					{TEXT("HPAfter"), FString::FromInt(Target->GetHealth())},
					{TEXT("Killed"), TEXT("0")},
				});
			}
		}
		return false;
	}

	const int32 HealthBefore = Target->GetHealth();
	Target->ApplyDamageForServer(DamageAmount, FName(TEXT("BossPattern")));
	const int32 HealthAfter = Target->GetHealth();
	if (HealthAfter >= HealthBefore)
	{
		return false;
	}

	FTimerHandle& HitLockTimer = HitLockTimerHandles.FindOrAdd(PlayerKey);
	FTimerDelegate Delegate = FTimerDelegate::CreateUObject(this, &UBossPatternComponent::ClearHitLockForServer, PlayerKey);
	GetWorld()->GetTimerManager().SetTimer(HitLockTimer, Delegate, ResolvedConfig.Common.GlobalHitLockSeconds, false);
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossPattern Hit Pattern=%s Player=%s Key=%s Damage=%d HPBefore=%d HPAfter=%d"),
		ToPatternName(CurrentPattern),
		*PlayerController->GetName(),
		*PlayerKey,
		HealthBefore - HealthAfter,
		HealthBefore,
		HealthAfter);
	if (UBalanceTelemetryComponent* Telemetry = UBalanceTelemetryComponent::FindForServer(this))
	{
		Telemetry->EmitForServer(TEXT("PatternContactResolved"), {
			{TEXT("Player"), Telemetry->GetOrAssignPlayerAliasForServer(PlayerController)},
			{TEXT("Pattern"), ToPatternName(CurrentPattern)},
			{TEXT("PatternInstance"), FString::FromInt(ActiveTelemetryPatternInstanceID)},
			{TEXT("Result"), TEXT("Hit")},
			{TEXT("Reason"), TEXT("None")},
			{TEXT("AppliedDamage"), FString::FromInt(HealthBefore - HealthAfter)},
			{TEXT("HPBefore"), FString::FromInt(HealthBefore)},
			{TEXT("HPAfter"), FString::FromInt(HealthAfter)},
			{TEXT("Killed"), HealthAfter <= 0 ? TEXT("1") : TEXT("0")},
		});
	}
	return true;
}

void UBossPatternComponent::NotifyPopulationChangedForServer(FName Reason)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !bRunning)
	{
		return;
	}

	const int32 PreviousCount = ActivePlayerCount;
	ActivePlayerCount = CountActivePlayersForServer();
	if (ActivePlayerCount <= 0 && PreviousCount > 0)
	{
		PauseForNoPlayersForServer(Reason);
	}
	else if (ActivePlayerCount > 0 && PreviousCount <= 0 && ServerState == EBossPatternServerState::PausedNoPlayers)
	{
		RestartAfterNoPlayersForServer(Reason);
	}
}

void UBossPatternComponent::ScheduleTransition(float DelaySeconds)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	PendingDelaySeconds = FMath::Max(0.0f, DelaySeconds);
	const int32 ExpectedSerial = ++TransitionSerial;
	FTimerDelegate Delegate = FTimerDelegate::CreateUObject(this, &UBossPatternComponent::HandleTransitionForServer, ExpectedSerial);
	World->GetTimerManager().SetTimer(TransitionTimerHandle, Delegate, PendingDelaySeconds, false);
}

void UBossPatternComponent::HandleTransitionForServer(int32 ExpectedSerial)
{
	AdvanceForServer(ExpectedSerial);
}

bool UBossPatternComponent::AdvanceForServer(int32 ExpectedSerial)
{
	ARaidBoss* Boss = Cast<ARaidBoss>(GetOwner());
	if (!bRunning || !Boss || !Boss->HasAuthority() || ExpectedSerial != TransitionSerial)
	{
		return false;
	}

	if (Boss->IsDefeated())
	{
		StopForServer(FName(TEXT("BossDead")));
		return true;
	}

	// 원문 예외 4.2(PATTERN-12): BossState가 Battle이 아니면 패턴을 중지하고 활성 오브젝트를 제거한다.
	// 기존 중지 경로는 ERaidState::End와 IsDefeated()뿐이라 보스 상태 enum 자체를 보는 가드가 없었다.
	if (Boss->GetBossState() != EBossState::Battle)
	{
		StopForServer(FName(TEXT("BossStateNotBattle")));
		return true;
	}

	const ARaidGameState* RaidGameState = GetWorld() ? GetWorld()->GetGameState<ARaidGameState>() : nullptr;
	if (RaidGameState && RaidGameState->RaidState == ERaidState::End)
	{
		StopForServer(FName(TEXT("RaidEnd")));
		return true;
	}

	switch (ServerState)
	{
	case EBossPatternServerState::FirstDelay:
		CurrentPattern = EBossPatternKind::CorruptedActino;
		BeginActiveForServer();
		break;
	case EBossPatternServerState::Intermission:
		CurrentPattern = NextPattern;
		BeginTelegraphForServer();
		break;
	case EBossPatternServerState::Telegraphing:
		BeginActiveForServer();
		break;
	case EBossPatternServerState::Active:
		FinishActiveForServer();
		break;
	default:
		return false;
	}
	return true;
}

void UBossPatternComponent::BeginTelegraphForServer()
{
	ServerState = EBossPatternServerState::Telegraphing;
	SpawnPatternActorForServer(EBossPatternLifecycleState::Telegraphing);
	const float TelegraphSeconds = CurrentPattern == EBossPatternKind::CorruptedActino
		? ResolvedConfig.Common.CorruptedTelegraphSeconds
		: ResolvedConfig.Common.StellarTelegraphSeconds;
	ScheduleTransition(TelegraphSeconds);
}

void UBossPatternComponent::BeginActiveForServer()
{
	ServerState = EBossPatternServerState::Active;
	if (ActivePatternActor)
	{
		ActivePatternActor->SetLifecycleForServer(EBossPatternLifecycleState::Active, GetServerWorldTimeSeconds());
	}
	else
	{
		SpawnPatternActorForServer(EBossPatternLifecycleState::Active);
	}

	const float DurationSeconds = CurrentPattern == EBossPatternKind::CorruptedActino
		? ResolvedConfig.Common.CorruptedDurationSeconds
		: ResolvedConfig.Common.StellarDurationSeconds;
	ScheduleTransition(DurationSeconds);
}

void UBossPatternComponent::FinishActiveForServer()
{
	const EBossPatternKind FinishedPattern = CurrentPattern;
	DestroyActivePatternActorForServer();
	CurrentPattern = EBossPatternKind::None;
	NextPattern = FinishedPattern == EBossPatternKind::CorruptedActino
		? EBossPatternKind::StellarRemnant
		: EBossPatternKind::CorruptedActino;
	ServerState = EBossPatternServerState::Intermission;
	const float NextTelegraphSeconds = NextPattern == EBossPatternKind::CorruptedActino
		? ResolvedConfig.Common.CorruptedTelegraphSeconds
		: ResolvedConfig.Common.StellarTelegraphSeconds;
	ScheduleTransition(FMath::Max(0.0f, ResolvedConfig.Common.IntermissionSeconds - NextTelegraphSeconds));
}

bool UBossPatternComponent::TryGetPlayerPlaneZForServer(float& OutPlaneZCm) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// 드론은 진입 높이를 FixedZPosition으로 고정하므로 어느 드론을 봐도 같은 평면이다.
	// 한 기도 없으면 보스 Z를 그대로 쓴다.
	for (TActorIterator<ADrone> It(World); It; ++It)
	{
		const ADrone* Drone = *It;
		if (Drone && !Drone->IsActorBeingDestroyed())
		{
			OutPlaneZCm = Drone->GetActorLocation().Z;
			return true;
		}
	}
	return false;
}

ABossPatternActorBase* UBossPatternComponent::SpawnPatternActorForServer(EBossPatternLifecycleState LifecycleState)
{
	DestroyActivePatternActorForServer();
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner || !bResolvedConfigReady)
	{
		return nullptr;
	}

	UClass* PatternActorClass = ABossPatternActorBase::StaticClass();
	if (CurrentPattern == EBossPatternKind::CorruptedActino)
	{
		PatternActorClass = ACorruptedActinoPatternActor::StaticClass();
	}
	else if (CurrentPattern == EBossPatternKind::StellarRemnant)
	{
		PatternActorClass = AStellarRemnantPatternActor::StaticClass();
	}
	FTransform PatternSpawnTransform = Owner->GetActorTransform();
	PatternSpawnTransform.SetScale3D(FVector::OneVector);
	// 패턴 기하는 확정 명세가 말하는 "플레이어 평면"을 기준으로 놓는다.
	// 보스 Z를 그대로 쓰면 드론의 고정 높이와 어긋나 두 패턴 모두 판정이 깨진다.
	//  - Stellar 피해 파편의 로컬 Z는 0이고 판정은 3D 거리이므로, 평면이 어긋난 만큼이
	//    파편 반경(70cm)을 넘으면 XY가 정확히 겹쳐도 절대 맞지 않는다.
	//  - Corrupted는 높이를 ±75cm로 따로 보므로, 빔 Z 진동(±300cm)이 드론 높이를
	//    스치고 지나갈 때만 우연히 맞는다. 화면의 빔 위치와 피격이 어긋나 보인다.
	if (float PlayerPlaneZ = 0.0f; TryGetPlayerPlaneZForServer(PlayerPlaneZ))
	{
		FVector PatternOrigin = PatternSpawnTransform.GetLocation();
		PatternOrigin.Z = PlayerPlaneZ;
		PatternSpawnTransform.SetLocation(PatternOrigin);
	}
	ActivePatternActor = World->SpawnActorDeferred<ABossPatternActorBase>(
		PatternActorClass,
		PatternSpawnTransform,
		Owner,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (ActivePatternActor)
	{
		ActivePatternActor->SnapshotResolvedConfig(ResolvedConfig);
		ActivePatternActor->FinishSpawning(PatternSpawnTransform);
		const int32 InstanceID = ++NextPatternInstanceID;
		ActivePatternActor->InitializeForServer(CurrentPattern, LifecycleState, InstanceID, GetServerWorldTimeSeconds());
		ActiveTelemetryPatternInstanceID = InstanceID;
		ActiveTelemetryPatternStartTime = GetServerWorldTimeSeconds();
		if (UBalanceTelemetryComponent* Telemetry = UBalanceTelemetryComponent::FindForServer(this))
		{
			Telemetry->EmitForServer(TEXT("PatternStarted"), {
				{TEXT("Pattern"), ToPatternName(CurrentPattern)},
				{TEXT("PatternInstance"), FString::FromInt(InstanceID)},
				{TEXT("Lifecycle"), LifecycleState == EBossPatternLifecycleState::Active ? TEXT("Active") : TEXT("Telegraphing")},
				{TEXT("AlivePlayers"), FString::FromInt(FMath::Max(0, ActivePlayerCount))},
			});
		}
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Spawn Pattern=%s Lifecycle=%s InstanceID=%d Boss=%s"),
			ToPatternName(CurrentPattern),
			LifecycleState == EBossPatternLifecycleState::Active ? TEXT("Active") : TEXT("Telegraphing"),
			InstanceID,
			*Owner->GetName());
	}
	return ActivePatternActor;
}

void UBossPatternComponent::DestroyActivePatternActorForServer()
{
	if (ActivePatternActor && ActiveTelemetryPatternInstanceID > 0)
	{
		if (UBalanceTelemetryComponent* Telemetry = UBalanceTelemetryComponent::FindForServer(this))
		{
			Telemetry->EmitForServer(TEXT("PatternEnded"), {
				{TEXT("Pattern"), ToPatternName(CurrentPattern)},
				{TEXT("PatternInstance"), FString::FromInt(ActiveTelemetryPatternInstanceID)},
				{TEXT("Duration"), UBalanceTelemetryComponent::Number(FMath::Max(0.0f, GetServerWorldTimeSeconds() - ActiveTelemetryPatternStartTime))},
				{TEXT("EndReason"), TEXT("Destroyed")},
			});
		}
	}
	if (ActivePatternActor && !ActivePatternActor->IsActorBeingDestroyed())
	{
		ActivePatternActor->Destroy();
	}
	ActivePatternActor = nullptr;
	ActiveTelemetryPatternInstanceID = 0;
	ActiveTelemetryPatternStartTime = 0.0f;
}

void UBossPatternComponent::ClearHitLockForServer(FString PlayerKey)
{
	HitLockTimerHandles.Remove(PlayerKey);
	HitLockIgnoredLoggedPlayerKeys.Remove(PlayerKey);
}

void UBossPatternComponent::ClearAllHitLocksForServer()
{
	if (UWorld* World = GetWorld())
	{
		for (TPair<FString, FTimerHandle>& Pair : HitLockTimerHandles)
		{
			World->GetTimerManager().ClearTimer(Pair.Value);
		}
	}
	HitLockTimerHandles.Reset();
	DodgeIgnoredLoggedPlayerKeys.Reset();
	HitLockIgnoredLoggedPlayerKeys.Reset();
}

int32 UBossPatternComponent::CountActivePlayersForServer() const
{
	const UWorld* World = GetWorld();
	const ARaidGameState* RaidGameState = World ? World->GetGameState<ARaidGameState>() : nullptr;
	if (!World || !RaidGameState || RaidGameState->RaidState != ERaidState::Battle)
	{
		return 0;
	}

	int32 Count = 0;
	for (TActorIterator<ARaidPlayerController> It(World); It; ++It)
	{
		const ARaidPlayerController* PlayerController = *It;
		const ADrone* Drone = PlayerController ? Cast<ADrone>(PlayerController->GetPawn()) : nullptr;
		if (PlayerController && !PlayerController->IsActorBeingDestroyed()
			&& PlayerController->GetPlayerSelectionState() == EPlayerSelectionState::InBattle
			&& Drone && !Drone->IsDead())
		{
			++Count;
		}
	}
	return Count;
}

void UBossPatternComponent::PauseForNoPlayersForServer(FName Reason)
{
	++TransitionSerial;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TransitionTimerHandle);
	}
	DestroyActivePatternActorForServer();
	ClearAllHitLocksForServer();
	ServerState = EBossPatternServerState::PausedNoPlayers;
	CurrentPattern = EBossPatternKind::None;
	NextPattern = EBossPatternKind::CorruptedActino;
	PendingDelaySeconds = 0.0f;
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossPattern Pause Reason=NoAlivePlayers Source=%s Boss=%s InstanceID=%d"),
		Reason.IsNone() ? TEXT("Unknown") : *Reason.ToString(),
		*GetNameSafe(GetOwner()),
		TransitionSerial);
}

void UBossPatternComponent::RestartAfterNoPlayersForServer(FName Reason)
{
	ServerState = EBossPatternServerState::FirstDelay;
	CurrentPattern = EBossPatternKind::None;
	NextPattern = EBossPatternKind::CorruptedActino;
	ScheduleTransition(ResolvedConfig.Common.FirstDelaySeconds);
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossPattern Restart Reason=AlivePlayerJoined Source=%s Boss=%s Delay=%.2f InstanceID=%d"),
		Reason.IsNone() ? TEXT("Unknown") : *Reason.ToString(),
		*GetNameSafe(GetOwner()),
		PendingDelaySeconds,
		TransitionSerial);
}

float UBossPatternComponent::GetServerWorldTimeSeconds() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0f);
}

#if WITH_DEV_AUTOMATION_TESTS
EBossPatternServerState UBossPatternComponent::GetServerStateForTest() const
{
	return ServerState;
}

EBossPatternKind UBossPatternComponent::GetCurrentPatternForTest() const
{
	return CurrentPattern;
}

EBossPatternKind UBossPatternComponent::GetNextPatternForTest() const
{
	return NextPattern;
}

float UBossPatternComponent::GetPendingDelayForTest() const
{
	return PendingDelaySeconds;
}

int32 UBossPatternComponent::GetActivePlayerCountForTest() const
{
	return ActivePlayerCount;
}

int32 UBossPatternComponent::GetHitLockCountForTest() const
{
	return HitLockTimerHandles.Num();
}

int32 UBossPatternComponent::GetDodgeIgnoredLogKeyCountForTest() const
{
	return DodgeIgnoredLoggedPlayerKeys.Num();
}

int32 UBossPatternComponent::GetHitLockIgnoredLogKeyCountForTest() const
{
	return HitLockIgnoredLoggedPlayerKeys.Num();
}

int32 UBossPatternComponent::GetTransitionSerialForTest() const
{
	return TransitionSerial;
}

bool UBossPatternComponent::IsTransitionTimerActiveForTest() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimerManager().IsTimerActive(TransitionTimerHandle);
}

bool UBossPatternComponent::FireScheduledTransitionForTest()
{
	return FireTransitionForTest(TransitionSerial);
}

bool UBossPatternComponent::FireTransitionForTest(int32 ExpectedSerial)
{
	if (ExpectedSerial == TransitionSerial)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(TransitionTimerHandle);
		}
	}
	return AdvanceForServer(ExpectedSerial);
}

ABossPatternActorBase* UBossPatternComponent::GetActivePatternActorForTest() const
{
	return ActivePatternActor;
}

void UBossPatternComponent::ResolvePatternDataForTest()
{
	ResolvePatternData();
}

bool UBossPatternComponent::IsResolvedConfigReadyForTest() const
{
	return bResolvedConfigReady;
}
#endif
