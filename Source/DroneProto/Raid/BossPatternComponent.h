#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BossPatternTypes.h"
#include "BossPatternComponent.generated.h"

class ABossPatternActorBase;
class ADrone;

UCLASS(ClassGroup = (Raid), meta = (BlueprintSpawnableComponent))
class DRONEPROTO_API UBossPatternComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBossPatternComponent();

	bool StartForServer();
	void StopForServer(FName Reason);
	bool IsRunning() const;
	bool TryApplyPatternDamageForServer(ADrone* Target, int32 DamageAmount);
	void NotifyPopulationChangedForServer(FName Reason);

#if WITH_DEV_AUTOMATION_TESTS
	EBossPatternServerState GetServerStateForTest() const;
	EBossPatternKind GetCurrentPatternForTest() const;
	EBossPatternKind GetNextPatternForTest() const;
	float GetPendingDelayForTest() const;
	int32 GetActivePlayerCountForTest() const;
	int32 GetHitLockCountForTest() const;
	int32 GetTransitionSerialForTest() const;
	bool IsTransitionTimerActiveForTest() const;
	bool FireScheduledTransitionForTest();
	bool FireTransitionForTest(int32 ExpectedSerial);
	ABossPatternActorBase* GetActivePatternActorForTest() const;
#endif

private:
	FBossPatternConfig Config;
	FTimerHandle TransitionTimerHandle;
	TMap<FString, FTimerHandle> HitLockTimerHandles;

	UPROPERTY(Transient)
	TObjectPtr<ABossPatternActorBase> ActivePatternActor = nullptr;

	EBossPatternServerState ServerState = EBossPatternServerState::Stopped;
	EBossPatternKind CurrentPattern = EBossPatternKind::None;
	EBossPatternKind NextPattern = EBossPatternKind::CorruptedActino;
	float PendingDelaySeconds = 0.0f;
	int32 TransitionSerial = 0;
	int32 NextPatternInstanceID = 0;
	int32 ActivePlayerCount = -1;
	bool bRunning = false;

	void ScheduleTransition(float DelaySeconds);
	void HandleTransitionForServer(int32 ExpectedSerial);
	bool AdvanceForServer(int32 ExpectedSerial);
	void BeginTelegraphForServer();
	void BeginActiveForServer();
	void FinishActiveForServer();
	ABossPatternActorBase* SpawnPatternActorForServer(EBossPatternLifecycleState LifecycleState);
	void DestroyActivePatternActorForServer();
	void ClearHitLockForServer(FString PlayerKey);
	void ClearAllHitLocksForServer();
	int32 CountActivePlayersForServer() const;
	void PauseForNoPlayersForServer(FName Reason);
	void RestartAfterNoPlayersForServer(FName Reason);
	float GetServerWorldTimeSeconds() const;
};
