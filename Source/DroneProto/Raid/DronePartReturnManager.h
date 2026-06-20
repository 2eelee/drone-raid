#pragma once

#include "CoreMinimal.h"
#include "DronePart.h"
#include "UObject/Object.h"
#include "DronePartReturnManager.generated.h"

class ADronePartInventory;
class ARaidPlayerController;

UENUM(BlueprintType)
enum class EDronePartReturnReason : uint8
{
	Cancel,
	Replace,
	Death,
	Disconnect,
	RaidEnd,
	Error
};

USTRUCT(BlueprintType)
struct DRONEPROTO_API FReturnedDronePartLog
{
	GENERATED_BODY()

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Drone Parts|Return")
	int32 LogID = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Drone Parts|Return")
	FString PlayerID;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Drone Parts|Return")
	FName DronePartID = NAME_None;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Drone Parts|Return")
	EPartSlot Slot = EPartSlot::Core;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Drone Parts|Return")
	EDronePartReturnReason ReturnReason = EDronePartReturnReason::Error;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Drone Parts|Return")
	FDateTime ReturnTime;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Drone Parts|Return")
	bool bIsProcessed = false;
};

UCLASS()
class DRONEPROTO_API UDronePartReturnManager : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(ADronePartInventory* InInventory);

	bool ReturnSelectedParts(ARaidPlayerController* PC, EDronePartReturnReason Reason);
	bool ReturnEquippedParts(ARaidPlayerController* PC, EDronePartReturnReason Reason);
	bool ReturnSingleSelectedPart(ARaidPlayerController* PC, EPartSlot Slot, EDronePartReturnReason Reason);
	bool ReturnSingleEquippedPart(ARaidPlayerController* PC, EPartSlot Slot, EDronePartReturnReason Reason);
	bool ReturnSinglePart(ARaidPlayerController* PC, FName PartID, EPartSlot Slot, EDronePartReturnReason Reason);

	bool ValidateReturn(ARaidPlayerController* PC, FName PartID, EPartSlot Slot, EDronePartReturnReason Reason) const;
	bool IsSelectedSlotAlreadyEmpty(const ARaidPlayerController* PC, EPartSlot Slot) const;
	bool IsEquippedSlotAlreadyEmpty(const ARaidPlayerController* PC, EPartSlot Slot) const;

	const TArray<FReturnedDronePartLog>& GetReturnLogs() const;

private:
	UPROPERTY()
	ADronePartInventory* Inventory = nullptr;

	UPROPERTY()
	TArray<FReturnedDronePartLog> ReturnLogs;

	int32 NextLogID = 1;

	void SaveReturnLog(ARaidPlayerController* PC, FName PartID, EPartSlot Slot, EDronePartReturnReason Reason, bool bIsProcessed);
	FString BuildPlayerID(const ARaidPlayerController* PC) const;
};
