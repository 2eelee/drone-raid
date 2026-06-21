#pragma once

#include "CoreMinimal.h"
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

	UDronePartReturnManager* GetDronePartReturnManager() const;

private:
	UPROPERTY()
	UDronePartReturnManager* DronePartReturnManager = nullptr;

	bool EnsureDronePartReturnManagerForServer();
};
