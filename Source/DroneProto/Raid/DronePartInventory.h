#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "DronePart.h"
#include "DronePartInventory.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDronePartStocksChanged);

USTRUCT(BlueprintType)
struct DRONEPROTO_API FDronePartStock
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Drone Parts")
	FName PartID = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Drone Parts")
	EDronePartType PartType = EDronePartType::Core;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Drone Parts")
	int32 CurrentCount = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Drone Parts")
	int32 MaxCount = 0;
};

UCLASS()
class DRONEPROTO_API ADronePartInventory : public AInfo
{
	GENERATED_BODY()

public:
	ADronePartInventory();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Drone Parts|Server")
	bool TryConsumePart(FName PartID);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Drone Parts|Server")
	bool ReturnDronePart(FName PartID);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Drone Parts|Server")
	void ReturnPart(FName PartID);

	UFUNCTION(BlueprintPure, Category = "Drone Parts")
	TArray<FDronePartStock> GetPartStocks() const;

	UFUNCTION(BlueprintPure, Category = "Drone Parts")
	int32 GetCurrentCount(FName PartID) const;

	UFUNCTION(BlueprintPure, Category = "Drone Parts")
	int32 GetMaxCount(FName PartID) const;

	UFUNCTION(BlueprintPure, Category = "Drone Parts")
	bool IsPartAvailable(FName PartID) const;

	UFUNCTION(BlueprintPure, Category = "Drone Parts")
	bool GetPartType(FName PartID, EDronePartType& OutType) const;

	UFUNCTION(BlueprintPure, Category = "Drone Parts|Part IDs")
	static FName GetCoreZenithPartID();

	UFUNCTION(BlueprintPure, Category = "Drone Parts|Part IDs")
	static FName GetCoreBoosterPartID();

	UFUNCTION(BlueprintPure, Category = "Drone Parts|Part IDs")
	static FName GetCoreDrainPartID();

	UFUNCTION(BlueprintPure, Category = "Drone Parts|Part IDs")
	static FName GetPulseLaserPartID();

	UFUNCTION(BlueprintPure, Category = "Drone Parts|Part IDs")
	static FName GetFractureBurstPartID();

	UFUNCTION(BlueprintPure, Category = "Drone Parts|Part IDs")
	static FName GetVectorCannonPartID();

	UPROPERTY(BlueprintAssignable, Category = "Drone Parts")
	FOnDronePartStocksChanged OnPartStocksChanged;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_PartStocks, VisibleInstanceOnly, BlueprintReadOnly, Category = "Drone Parts")
	TArray<FDronePartStock> PartStocks;

	UFUNCTION()
	void OnRep_PartStocks();

private:
	void InitializeDefaultStocks();
	FDronePartStock* FindStock(FName PartID);
	const FDronePartStock* FindStock(FName PartID) const;
};
