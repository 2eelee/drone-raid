#pragma once

#include "CoreMinimal.h"
#include "DroneCombatTypes.h"
#include "GameFramework/GameModeBase.h"
#include "RaidGameMode.generated.h"

class UDronePartReturnManager;
class UBalanceTelemetryComponent;
class ARaidBoss;
class URaidServerAdmissionService;
enum class EBossState : uint8;

struct FDroneBossDamageContribution
{
	FString PlayerKey;
	float Damage = 0.0f;
};

UCLASS()
class DRONEPROTO_API ARaidGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARaidGameMode();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal = TEXT("")) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;
	virtual void Logout(AController* Exiting) override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Raid")
	void ReturnAllEquippedPartsForRaidEnd(FName Reason);

	void HandleBossDefeatedForServer();

	// ENTRY-15: 입장 도중 끊긴 연결이 붙잡고 있던 예약을 반납하고 반납 건수를 돌려준다.
	int32 ReleaseAbandonedRaidReservationsForServer(FName Reason);
	void StartRaidTimeLimitTimerForServer();
	void ClearRaidTimeLimitTimerForServer(FName Reason);
	bool IsRaidTimeLimitTimerActiveForServer() const;

	// 원문 예외 4.5(BOSS-13)·4.13(POP-08): Battle인데 RaidTimer가 돌지 않으면 제한 시간 계산이
	// 어긋나고 빈 레이드가 무기한 유지된다. 불일치를 감지해 **타이머만** 재시작한다 —
	// 보스 패턴의 인원 0 대기 상태는 건드리지 않는다.
	bool DetectAndRecoverRaidTimerMismatchForServer();
	virtual ARaidBoss* EnsureRaidBossForServer();

	// Battle 전이 시 모든 보스의 패턴 타이머 시작/정지 오케스트레이션. 개별 타이머는 Boss가 소유한다.
	void StartBossPatternsForServer();
	void StopBossPatternsForServer(FName Reason);

	// 패턴 액터가 서버 실판정 궤적을 그대로 그려 보여줄지 여부. 프로덕션은 항상 false다 —
	// 플레이어에게 보이는 것은 VFX뿐이고 판정 형상은 개발용이기 때문이다.
	// 밸런스 샌드박스만 true로 덮어, 실제로 맞는 영역을 화면에서 확인할 수 있게 한다.
	// 피해 판정 자체는 어느 쪽에서도 바뀌지 않는다 — 서버 궤적이 단일 진실이다.
	virtual bool ShouldVisualizePatternHitGeometry() const;
	bool CanAcceptRaidJoinForServer(FName& OutRejectReason, bool bCheckNewPlayerCapacity = true) const;

	// DroneReport 중복 방지: PC 인스턴스 bool과 별개로 PlayerKey 기반 서버 set을 관리한다.
	// 재접속으로 PC가 새로 만들어져도 같은 플레이어의 Report가 중복 생성되지 않는다.
	bool TryMarkDroneReportGeneratedForServer(class ARaidPlayerController* RaidPC);
	void ClearDroneReportKeyForServer(class ARaidPlayerController* RaidPC, FName Reason);

	// 원문 `:63, :76`(REPORT-05): `SaveDroneReportData()`로 생성된 리포트를 `DroneReportDataList`에
	// 보관한다. 지금까지는 표시된 뒤 사라져 레이드 종료 후 등급 분포를 조회할 수단이 없었고,
	// 그래서 `TEST-02`의 밸런스 체크리스트가 구조적으로 실행 불가능했다.
	// 보관 수명은 GameMode 인스턴스(= 레이드 세션 1회)이며 별도 영속화는 하지 않는다.
	bool SaveDroneReportDataForServer(ARaidPlayerController* RaidPC, const FDroneReportData& ReportData);
	const TArray<FDroneReportData>& GetDroneReportDataListForServer() const;
	void ClearDroneReportDataListForServer(FName Reason);

	bool RecordBossDamageForServer(APlayerController* PlayerController, float DamageAmount);
	float GetBossDamageForPlayerKeyForServer(const FString& PlayerKey) const;
	TArray<FDroneBossDamageContribution> GetSortedBossDamageContributionsForServer() const;
	void ResetBossDamageContributionsForServer(FName Reason);
	static FString BuildStablePlayerKeyForServer(const APlayerController* PlayerController);

	UDronePartReturnManager* GetDronePartReturnManager() const;
	UBalanceTelemetryComponent* GetBalanceTelemetryForServer() const;

#if WITH_DEV_AUTOMATION_TESTS
	bool IsRaidTimeLimitTimerActiveForTest() const;
	void ExpireRaidTimeLimitForTest();
	bool NotifyRaidSpawnFailedForTest(AController* Controller, FName Reason);
	void SetAdmissionServiceForTest(URaidServerAdmissionService* InService, bool bInAdmissionRequired);
	// 실환경에서는 InitGame이 GameState를 spawn해 GameMode에 연결하지만, 합성 World를 쓰는
	// 자동화는 World에만 등록한다. Logout처럼 GameMode 쪽 GameState를 보는 경로를 태우려면 필요하다.
	void SetGameStateForTest(AGameStateBase* InGameState) { GameState = InGameState; }
	void ValidateRaidAdmissionForTest(const FString& Options, FString& OutErrorMessage);
#endif

private:
	UPROPERTY(EditDefaultsOnly, Category = "Raid|Timer", meta = (ClampMin = "1.0"))
	float RaidTimeLimitSeconds = 180.0f;

	UPROPERTY()
	UDronePartReturnManager* DronePartReturnManager = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Raid|Telemetry")
	UBalanceTelemetryComponent* BalanceTelemetry = nullptr;

	UPROPERTY()
	TObjectPtr<URaidServerAdmissionService> AdmissionService;

	bool bAdmissionRequired = false;
	FDelegateHandle PendingConnectionLostDelegateHandle;

	FTimerHandle RaidTimeLimitTimerHandle;
	bool bRaidTimeLimitExpiredForServer = false;
	FTimerHandle RaidTimerWatchdogTimerHandle;

	TSet<FString> GeneratedDroneReportPlayerKeys;
	TArray<FDroneReportData> DroneReportDataList;
	TMap<FString, float> PlayerBossDamageMap;
	TMap<TWeakObjectPtr<AController>, TWeakObjectPtr<AActor>> PlayerStartAssignments;

	bool EnsureDronePartReturnManagerForServer();
	void ValidateRaidAdmission(const FString& Options, FString& OutErrorMessage);
	void HandlePendingConnectionLostForServer(const FUniqueNetIdRepl& ConnectionUniqueId);
	void CollectLiveReservationTokensForServer(TSet<FString>& OutTokens) const;
	void HandleRaidTimeLimitExpiredForServer();
	void HandleRaidTimerWatchdogTickForServer();
	void NotifyBossPatternPopulationAfterLogoutForServer();
	void SetAllBossStatesForServer(EBossState NewBossState, FName Reason);
	bool NotifyRaidSpawnFailedForServer(AController* Controller, FName Reason) const;
};
