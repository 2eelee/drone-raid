#include "BossPatternComponent.h"

#include "BossPatternActorBase.h"
#include "RaidBoss.h"
#include "RaidGameState.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
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
}

bool UBossPatternComponent::StartForServer()
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !Owner->HasAuthority() || !World || bRunning)
	{
		return false;
	}

	bRunning = true;
	ServerState = EBossPatternServerState::FirstDelay;
	CurrentPattern = EBossPatternKind::None;
	NextPattern = EBossPatternKind::CorruptedActino;
	ScheduleTransition(Config.FirstDelaySeconds);
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
	bRunning = false;
	ServerState = EBossPatternServerState::Stopped;
	CurrentPattern = EBossPatternKind::None;
	NextPattern = EBossPatternKind::CorruptedActino;
	PendingDelaySeconds = 0.0f;

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
		? Config.CorruptedTelegraphSeconds
		: Config.StellarTelegraphSeconds;
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
		? Config.CorruptedDurationSeconds
		: Config.StellarDurationSeconds;
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
	ScheduleTransition(FinishedPattern == EBossPatternKind::CorruptedActino ? 1.2f : 1.0f);
}

ABossPatternActorBase* UBossPatternComponent::SpawnPatternActorForServer(EBossPatternLifecycleState LifecycleState)
{
	DestroyActivePatternActorForServer();
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = Owner;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ActivePatternActor = World->SpawnActor<ABossPatternActorBase>(
		ABossPatternActorBase::StaticClass(),
		Owner->GetActorTransform(),
		SpawnParameters);
	if (ActivePatternActor)
	{
		const int32 InstanceID = ++NextPatternInstanceID;
		ActivePatternActor->InitializeForServer(CurrentPattern, LifecycleState, InstanceID, GetServerWorldTimeSeconds());
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
	if (ActivePatternActor && !ActivePatternActor->IsActorBeingDestroyed())
	{
		ActivePatternActor->Destroy();
	}
	ActivePatternActor = nullptr;
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

float UBossPatternComponent::GetPendingDelayForTest() const
{
	return PendingDelaySeconds;
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
#endif
