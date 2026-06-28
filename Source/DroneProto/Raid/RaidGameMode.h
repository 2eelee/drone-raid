#pragma once

#include "CoreMinimal.h"
#include "DroneCombatTypes.h"
#include "GameFramework/GameModeBase.h"
#include "RaidGameMode.generated.h"

class UDronePartReturnManager;

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

	UDronePartReturnManager* GetDronePartReturnManager() const;

#if WITH_DEV_AUTOMATION_TESTS
	bool IsRaidTimeLimitTimerActiveForTest() const;
	void ExpireRaidTimeLimitForTest();
#endif

private:
	UPROPERTY(EditDefaultsOnly, Category = "Raid|Timer", meta = (ClampMin = "1.0"))
	float RaidTimeLimitSeconds = 180.0f;

	UPROPERTY()
	UDronePartReturnManager* DronePartReturnManager = nullptr;

	FTimerHandle RaidTimeLimitTimerHandle;

	bool EnsureDronePartReturnManagerForServer();
	void ClearRaidTimeLimitTimerForServer(FName Reason);
	void HandleRaidTimeLimitExpiredForServer();
};
