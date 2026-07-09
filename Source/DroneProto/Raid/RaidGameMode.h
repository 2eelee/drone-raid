#pragma once

#include "CoreMinimal.h"
#include "DroneCombatTypes.h"
#include "GameFramework/GameModeBase.h"
#include "RaidGameMode.generated.h"

class UDronePartReturnManager;
enum class EBossState : uint8;

UCLASS()
class DRONEPROTO_API ARaidGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARaidGameMode();

	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;
	virtual void Logout(AController* Exiting) override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Raid")
	void ReturnAllEquippedPartsForRaidEnd(FName Reason);

	void HandleBossDefeatedForServer();
	void StartRaidTimeLimitTimerForServer();

	// Battle 전이 시 모든 보스의 패턴 타이머 시작/정지 오케스트레이션. 개별 타이머는 Boss가 소유한다.
	void StartBossPatternsForServer();
	void StopBossPatternsForServer(FName Reason);
	bool CanAcceptRaidJoinForServer(FName& OutRejectReason) const;

	// DroneReport 중복 방지: PC 인스턴스 bool과 별개로 PlayerKey 기반 서버 set을 관리한다.
	// 재접속으로 PC가 새로 만들어져도 같은 플레이어의 Report가 중복 생성되지 않는다.
	bool TryMarkDroneReportGeneratedForServer(class ARaidPlayerController* RaidPC);
	void ClearDroneReportKeyForServer(class ARaidPlayerController* RaidPC, FName Reason);

	UDronePartReturnManager* GetDronePartReturnManager() const;

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

	FTimerHandle RaidTimeLimitTimerHandle;
	bool bRaidTimeLimitExpiredForServer = false;

	TSet<FString> GeneratedDroneReportPlayerKeys;

	static FString BuildDroneReportPlayerKeyForServer(const ARaidPlayerController* RaidPC);
	bool EnsureDronePartReturnManagerForServer();
	void ClearRaidTimeLimitTimerForServer(FName Reason);
	void HandleRaidTimeLimitExpiredForServer();
	void SetAllBossStatesForServer(EBossState NewBossState, FName Reason);
	bool NotifyRaidSpawnFailedForServer(AController* Controller, FName Reason) const;
};
