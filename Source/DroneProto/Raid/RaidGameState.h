#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "RaidGameState.generated.h"

class ADronePartInventory;
class ARaidBoss;

UENUM(BlueprintType)
enum class ERaidState : uint8
{
	Waiting  UMETA(DisplayName = "Waiting"),
	Drafting UMETA(DisplayName = "Drafting"),
	Battle   UMETA(DisplayName = "Battle"),
	End      UMETA(DisplayName = "End"),
};

UCLASS()
class DRONEPROTO_API ARaidGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ARaidGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(ReplicatedUsing = OnRep_RaidState, BlueprintReadOnly, Category = "Raid")
	ERaidState RaidState;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Raid")
	int32 CurrentPlayers;

	UPROPERTY(ReplicatedUsing = OnRep_DronePartInventory, BlueprintReadOnly, Category = "Raid")
	TObjectPtr<ADronePartInventory> DronePartInventory;

	UPROPERTY(ReplicatedUsing = OnRep_RaidBoss, BlueprintReadOnly, Category = "Raid")
	TObjectPtr<ARaidBoss> RaidBoss;

	UPROPERTY(ReplicatedUsing = OnRep_RaidTimeEndServerTime, BlueprintReadOnly, Category = "Raid|Timer")
	float RaidTimeEndServerTime = 0.0f;

	UFUNCTION(BlueprintPure, Category = "Raid")
	ADronePartInventory* GetDronePartInventory() const;

	UFUNCTION(BlueprintPure, Category = "Raid")
	ARaidBoss* GetRaidBoss() const;

	UFUNCTION(BlueprintPure, Category = "Raid|Timer")
	float GetRaidTimeEndServerTime() const;

	UFUNCTION(BlueprintPure, Category = "Raid|Timer")
	float GetRaidRemainingSeconds() const;

	void SetDronePartInventory(ADronePartInventory* InDronePartInventory);
	void SetRaidBossForServer(ARaidBoss* InRaidBoss);
	void SetRaidStateForServer(ERaidState NewRaidState);
	void SetRaidTimeEndServerTimeForServer(float NewRaidTimeEndServerTime);

private:
	UFUNCTION()
	void OnRep_RaidState();

	UFUNCTION()
	void OnRep_DronePartInventory();

	UFUNCTION()
	void OnRep_RaidBoss();

	UFUNCTION()
	void OnRep_RaidTimeEndServerTime();

	UFUNCTION()
	void HandleDronePartStocksChanged();

	void BindDronePartInventoryEvents();
	void NotifyLocalPartSelectUIRefresh(const TCHAR* Source);
};
