#pragma once

#include "CoreMinimal.h"
#include "DroneCombatTypes.h"
#include "GameFramework/GameModeBase.h"
#include "RaidGameMode.generated.h"

class UDronePartReturnManager;
class UBalanceTelemetryComponent;
class ARaidBoss;
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
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;
	virtual void Logout(AController* Exiting) override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Raid")
	void ReturnAllEquippedPartsForRaidEnd(FName Reason);

	void HandleBossDefeatedForServer();
	void StartRaidTimeLimitTimerForServer();
	void ClearRaidTimeLimitTimerForServer(FName Reason);
	bool IsRaidTimeLimitTimerActiveForServer() const;

	// 원문 예외 4.5(BOSS-13)·4.13(POP-08): Battle인데 RaidTimer가 돌지 않으면 제한 시간 계산이
	// 어긋나고 빈 레이드가 무기한 유지된다. 불일치를 감지해 **타이머만** 재시작한다 —
	// 보스 패턴의 인원 0 대기 상태는 건드리지 않는다.
	bool DetectAndRecoverRaidTimerMismatchForServer();
	ARaidBoss* EnsureRaidBossForServer();

	// Battle 전이 시 모든 보스의 패턴 타이머 시작/정지 오케스트레이션. 개별 타이머는 Boss가 소유한다.
	void StartBossPatternsForServer();
	void StopBossPatternsForServer(FName Reason);
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
#endif

private:
	UPROPERTY(EditDefaultsOnly, Category = "Raid|Timer", meta = (ClampMin = "1.0"))
	float RaidTimeLimitSeconds = 180.0f;

	UPROPERTY()
	UDronePartReturnManager* DronePartReturnManager = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Raid|Telemetry")
	UBalanceTelemetryComponent* BalanceTelemetry = nullptr;

	FTimerHandle RaidTimeLimitTimerHandle;
	bool bRaidTimeLimitExpiredForServer = false;
	FTimerHandle RaidTimerWatchdogTimerHandle;

	TSet<FString> GeneratedDroneReportPlayerKeys;
	TArray<FDroneReportData> DroneReportDataList;
	TMap<FString, float> PlayerBossDamageMap;
	TMap<TWeakObjectPtr<AController>, TWeakObjectPtr<AActor>> PlayerStartAssignments;

	bool EnsureDronePartReturnManagerForServer();
	void HandleRaidTimeLimitExpiredForServer();
	void HandleRaidTimerWatchdogTickForServer();
	void NotifyBossPatternPopulationAfterLogoutForServer();
	void SetAllBossStatesForServer(EBossState NewBossState, FName Reason);
	bool NotifyRaidSpawnFailedForServer(AController* Controller, FName Reason) const;
};
