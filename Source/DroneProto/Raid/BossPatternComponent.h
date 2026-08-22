#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Templates/SubclassOf.h"
#include "BossPatternTypes.h"
#include "BossPatternComponent.generated.h"

class ABossPatternActorBase;
class ACorruptedActinoPatternActor;
class ADrone;
class AStellarRemnantPatternActor;
class UDataTable;

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
	bool CopyResolvedConfig(FBossPatternResolvedConfig& OutConfig) const;

	// 다음에 실행할 패턴을 지정한다. 루프 순서(PATTERN-01)를 바꾸지 않고 시작점만 옮기며,
	// 밸런스 반복 시험에서 특정 패턴으로 바로 가기 위한 진입점이다.
	void SetNextPatternForServer(EBossPatternKind NextPatternKind, FName Reason);

	// 밸런스 샌드박스의 수동 패턴 실행 진입점. 예약이 아니라 재시작이다 —
	// 현재 패턴을 기존 종료 경로로 정리한 뒤 지정한 패턴을 그 자리에서 텔레그래프부터 시작한다.
	//
	// SetNextPatternForServer만으로는 자동 진행에 묻힌다. 패턴이 끝날 때 FinishActiveForServer가
	// 교대 규칙대로 NextPattern을 무조건 덮어쓰기 때문에, 실행 중에 걸어 둔 예약은 반영되기 전에
	// 사라진다. 수동 버튼이 "눌러도 바뀌지 않는" 이유가 그것이다.
	//
	// 자동 진행은 이 함수를 부르지 않으므로 패턴 순서·반복 계약(PATTERN-01)은 그대로다.
	// 시작 조건은 자동 진행과 같은 기준(보스 생존·BossState Battle·RaidState Battle)을 쓴다.
	bool RestartWithPatternForServer(EBossPatternKind PatternKind, FName Reason);

	// 밸런스 상태 패널 조회용. 상태를 바꾸지 않는다.
	EBossPatternKind GetCurrentPattern() const { return CurrentPattern; }
	bool IsPatternDataTableInUse() const { return bResolvedConfigReady; }

#if WITH_DEV_AUTOMATION_TESTS
	EBossPatternServerState GetServerStateForTest() const;
	EBossPatternKind GetCurrentPatternForTest() const;
	EBossPatternKind GetNextPatternForTest() const;
	float GetPendingDelayForTest() const;
	int32 GetActivePlayerCountForTest() const;
	int32 GetHitLockCountForTest() const;
	int32 GetDodgeIgnoredLogKeyCountForTest() const;
	int32 GetHitLockIgnoredLogKeyCountForTest() const;
	int32 GetTransitionSerialForTest() const;
	bool IsTransitionTimerActiveForTest() const;
	bool FireScheduledTransitionForTest();
	bool FireTransitionForTest(int32 ExpectedSerial);
	ABossPatternActorBase* GetActivePatternActorForTest() const;
	void ResolvePatternDataForTest();
	bool IsResolvedConfigReadyForTest() const;
	UClass* ResolvePatternActorClassForTest(EBossPatternKind PatternKind) const;
	void SetPatternActorClassOverridesForTest(UClass* CorruptedClass, UClass* StellarClass);
#endif

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Raid|Boss|Pattern|Data")
	TObjectPtr<UDataTable> BossPatternDataTable = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Raid|Boss|Pattern|Data")
	TObjectPtr<UDataTable> CorruptedActinoDataTable = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Raid|Boss|Pattern|Data")
	TObjectPtr<UDataTable> CorruptedActinoPresetDataTable = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Raid|Boss|Pattern|Data")
	TObjectPtr<UDataTable> StellarRemnantDataTable = nullptr;

	// 패턴 액터를 Blueprint 파생으로 갈아끼우는 자리. **비워 두면 기존 C++ 클래스를 그대로 쓴다.**
	//
	// 패턴 연출·사운드 훅(`BP_OnPatternVisualChanged` / `BP_OnPatternVisualEnded`)은 패턴 액터에 있는데
	// 스폰 클래스를 이 컴포넌트가 직접 정하므로, 지정 수단이 없으면 BP 파생을 만들어도 스폰되지 않는다.
	// 보스처럼 "맵에 배치해 두면 채택된다"는 우회도 불가능하다 — 패턴 액터는 패턴마다 런타임에 스폰된다.
	//
	// `TSubclassOf::Get()`이 파생 관계를 검사하므로 엉뚱한 클래스가 들어오면 자동으로 null이 되고
	// 아래 폴백이 걸린다. 별도 방어 코드를 두지 않는 이유다.
	UPROPERTY(EditDefaultsOnly, Category = "Raid|Boss|Pattern|Visual")
	TSubclassOf<ACorruptedActinoPatternActor> CorruptedActinoPatternActorClass;

	UPROPERTY(EditDefaultsOnly, Category = "Raid|Boss|Pattern|Visual")
	TSubclassOf<AStellarRemnantPatternActor> StellarRemnantPatternActorClass;

	FBossPatternResolvedConfig ResolvedConfig;
	FTimerHandle TransitionTimerHandle;
	TMap<FString, FTimerHandle> HitLockTimerHandles;
	TSet<FString> DodgeIgnoredLoggedPlayerKeys;
	TSet<FString> HitLockIgnoredLoggedPlayerKeys;

	UPROPERTY(Transient)
	TObjectPtr<ABossPatternActorBase> ActivePatternActor = nullptr;

	EBossPatternServerState ServerState = EBossPatternServerState::Stopped;
	EBossPatternKind CurrentPattern = EBossPatternKind::None;
	EBossPatternKind NextPattern = EBossPatternKind::CorruptedActino;
	float PendingDelaySeconds = 0.0f;
	int32 TransitionSerial = 0;
	int32 NextPatternInstanceID = 0;
	int32 ActiveTelemetryPatternInstanceID = 0;
	float ActiveTelemetryPatternStartTime = 0.0f;
	int32 ActivePlayerCount = -1;
	bool bRunning = false;
	bool bResolvedConfigReady = false;

	void ScheduleTransition(float DelaySeconds);
	void HandleTransitionForServer(int32 ExpectedSerial);
	bool AdvanceForServer(int32 ExpectedSerial);
	void BeginTelegraphForServer();
	void BeginActiveForServer();
	void FinishActiveForServer();
	bool TryGetPlayerPlaneZForServer(float& OutPlaneZCm) const;
	ABossPatternActorBase* SpawnPatternActorForServer(EBossPatternLifecycleState LifecycleState);
	// 스폰할 패턴 액터 클래스를 정한다. 오버라이드가 비어 있으면 기존 C++ 클래스를 돌려준다.
	UClass* ResolvePatternActorClassForServer(EBossPatternKind PatternKind) const;
	void DestroyActivePatternActorForServer();
	void ClearHitLockForServer(FString PlayerKey);
	void ClearAllHitLocksForServer();
	int32 CountActivePlayersForServer() const;
	void PauseForNoPlayersForServer(FName Reason);
	void RestartAfterNoPlayersForServer(FName Reason);
	float GetServerWorldTimeSeconds() const;
	void ResolvePatternData();
};
