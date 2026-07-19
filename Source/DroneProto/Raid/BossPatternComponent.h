#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BossPatternTypes.h"
#include "BossPatternComponent.generated.h"

class ABossPatternActorBase;

UCLASS(ClassGroup = (Raid), meta = (BlueprintSpawnableComponent))
class DRONEPROTO_API UBossPatternComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBossPatternComponent();

	bool StartForServer();
	void StopForServer(FName Reason);
	bool IsRunning() const;

#if WITH_DEV_AUTOMATION_TESTS
	EBossPatternServerState GetServerStateForTest() const;
	EBossPatternKind GetCurrentPatternForTest() const;
	float GetPendingDelayForTest() const;
	int32 GetTransitionSerialForTest() const;
	bool IsTransitionTimerActiveForTest() const;
	bool FireScheduledTransitionForTest();
	bool FireTransitionForTest(int32 ExpectedSerial);
	ABossPatternActorBase* GetActivePatternActorForTest() const;
#endif

private:
	FBossPatternConfig Config;
	FTimerHandle TransitionTimerHandle;

	UPROPERTY(Transient)
	TObjectPtr<ABossPatternActorBase> ActivePatternActor = nullptr;

	EBossPatternServerState ServerState = EBossPatternServerState::Stopped;
	EBossPatternKind CurrentPattern = EBossPatternKind::None;
	EBossPatternKind NextPattern = EBossPatternKind::CorruptedActino;
	float PendingDelaySeconds = 0.0f;
	int32 TransitionSerial = 0;
	int32 NextPatternInstanceID = 0;
	bool bRunning = false;

	void ScheduleTransition(float DelaySeconds);
	void HandleTransitionForServer(int32 ExpectedSerial);
	bool AdvanceForServer(int32 ExpectedSerial);
	void BeginTelegraphForServer();
	void BeginActiveForServer();
	void FinishActiveForServer();
	ABossPatternActorBase* SpawnPatternActorForServer(EBossPatternLifecycleState LifecycleState);
	void DestroyActivePatternActorForServer();
	float GetServerWorldTimeSeconds() const;
};
