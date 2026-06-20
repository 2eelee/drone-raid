#pragma once

#include "CoreMinimal.h"
#include "DronePart.h"
#include "DronePartReturnManager.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "RaidPlayerController.generated.h"

class ADronePartInventory;
class ADrone;
class UTexture2D;
class UDronePartReturnManager;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnPartSelectionResult, EPartSlot, Slot, FName, PartID, bool, bSuccess, FString, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSelectedPartsChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPartSelectUIRefreshRequested);

UCLASS()
class DRONEPROTO_API ARaidPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Drone Parts")
	FOnPartSelectionResult OnPartSelectionResult;

	UPROPERTY(BlueprintAssignable, Category = "Drone Parts")
	FOnSelectedPartsChanged OnSelectedPartsChanged;

	UPROPERTY(BlueprintAssignable, Category = "UI")
	FOnPartSelectUIRefreshRequested OnPartSelectUIRefreshRequested;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UUserWidget> DronePartSelectWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
	bool bAutoShowDronePartSelectUI = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
	TMap<FName, TObjectPtr<UTexture2D>> PartIconOverrides;

	UFUNCTION(BlueprintCallable, Category = "Drone Parts")
	void RequestSelectPartFromUI(EDronePartSlot Slot, FName PartID);

	UFUNCTION(BlueprintCallable, Category = "Drone Parts")
	void RequestCancelPartFromUI(EDronePartSlot Slot);

	UFUNCTION(BlueprintCallable, Category = "Drone Parts")
	void RequestReadyForRaidFromUI();

	UFUNCTION(BlueprintPure, Category = "Drone Parts")
	FName GetSelectedCorePartID() const;

	UFUNCTION(BlueprintPure, Category = "Drone Parts")
	FName GetSelectedLeftWeaponPartID() const;

	UFUNCTION(BlueprintPure, Category = "Drone Parts")
	FName GetSelectedRightWeaponPartID() const;

	UFUNCTION(BlueprintPure, Category = "Drone Parts")
	FName GetSelectedPartIDBySlot(EPartSlot Slot) const;

	UFUNCTION(BlueprintPure, Category = "Drone Parts")
	FName GetSelectedPartForSlot(EDronePartSlot Slot) const;

	UFUNCTION(BlueprintPure, Category = "Drone Parts")
	bool HasSelectedPartForSlot(EDronePartSlot Slot) const;

	UFUNCTION(BlueprintPure, Category = "Drone Parts")
	FName GetEquippedPartIDBySlot(EPartSlot Slot) const;

	UFUNCTION(BlueprintPure, Category = "Drone Parts")
	TArray<FName> GetAvailablePartIDsForSlot(EDronePartSlot Slot) const;

	UFUNCTION(BlueprintPure, Category = "Drone Parts")
	TArray<FName> GetCorePartIDs() const;

	UFUNCTION(BlueprintPure, Category = "Drone Parts")
	TArray<FName> GetWeaponPartIDs() const;

	UFUNCTION(BlueprintPure, Category = "UI")
	FText GetPartDisplayName(FName PartID) const;

	UFUNCTION(BlueprintPure, Category = "UI")
	FText GetPartDescription(FName PartID) const;

	UFUNCTION(BlueprintPure, Category = "UI")
	UTexture2D* GetPartIcon(FName PartID) const;

	UFUNCTION(BlueprintPure, Category = "UI")
	int32 GetPartCurrentCount(FName PartID) const;

	UFUNCTION(BlueprintPure, Category = "UI")
	int32 GetPartMaxCount(FName PartID) const;

	UFUNCTION(BlueprintPure, Category = "Drone Parts")
	ADronePartInventory* GetDronePartInventory() const;

	UFUNCTION(BlueprintPure, Category = "Drone Parts")
	UDronePartReturnManager* GetDronePartReturnManager() const;

	UFUNCTION(BlueprintCallable, Category = "UI")
	bool RefreshDronePartInventoryBinding();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void ShowDronePartSelectUI();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void HideDronePartSelectUI();

	void SetSelectedPartIDForSlotForServer(EPartSlot Slot, FName PartID);
	void SetEquippedPartIDForSlotForServer(EPartSlot Slot, FName PartID);
	bool ReturnSelectedPartsForServer(EDronePartReturnReason Reason);
	bool ReturnEquippedPartsForServer(EDronePartReturnReason Reason);
	bool ReturnSingleSelectedPartForServer(EPartSlot Slot, EDronePartReturnReason Reason);
	bool ReturnSingleEquippedPartForServer(EPartSlot Slot, EDronePartReturnReason Reason);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Drone Parts")
	void Server_RequestSelectPart(EPartSlot Slot, FName NewPartID);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Drone Parts")
	void Server_RequestCancelPart(EPartSlot Slot);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Raid")
	void Server_RequestReadyForRaid();

	UFUNCTION(Client, Reliable, Category = "Drone Parts")
	void Client_NotifyPartSelectionResult(EPartSlot Slot, FName PartID, bool bSuccess, const FString& Reason);

	UFUNCTION(Client, Reliable, Category = "Raid")
	void Client_NotifyRaidReadyResult(bool bSuccess, const FString& Reason, FName CorePartID, FName LeftWeaponPartID, FName RightWeaponPartID);

	UFUNCTION(Exec)
	void D4SelectPart(FString SlotName, FString PartIDText);

	UFUNCTION(Exec)
	void D4CancelPart(FString SlotName);

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleInstanceOnly, Category = "Drone Parts")
	FName SelectedCorePartID = NAME_None;

	UPROPERTY(VisibleInstanceOnly, Category = "Drone Parts")
	FName SelectedLeftWeaponPartID = NAME_None;

	UPROPERTY(VisibleInstanceOnly, Category = "Drone Parts")
	FName SelectedRightWeaponPartID = NAME_None;

	UPROPERTY(VisibleInstanceOnly, Category = "Drone Parts")
	FName EquippedCorePartID = NAME_None;

	UPROPERTY(VisibleInstanceOnly, Category = "Drone Parts")
	FName EquippedLeftWeaponPartID = NAME_None;

	UPROPERTY(VisibleInstanceOnly, Category = "Drone Parts")
	FName EquippedRightWeaponPartID = NAME_None;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> DronePartSelectWidget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ADronePartInventory> BoundDronePartInventory = nullptr;

	FName* GetSelectedPartIDForSlot(EPartSlot Slot);
	const FName* GetSelectedPartIDForSlot(EPartSlot Slot) const;
	FName* GetEquippedPartIDForSlot(EPartSlot Slot);
	const FName* GetEquippedPartIDForSlot(EPartSlot Slot) const;
	void SetSelectedPartIDForSlot(EPartSlot Slot, FName PartID);
	void SetEquippedPartIDForSlot(EPartSlot Slot, FName PartID);
	bool IsPartTypeAllowedForSlot(EPartSlot Slot, EDronePartType PartType) const;
	bool TryParsePartSlot(const FString& SlotName, EPartSlot& OutSlot) const;
	bool ValidateSelectedLoadoutForServer(FString& OutReason) const;
	void MoveSelectedPartsToEquippedForServer();

	UFUNCTION()
	void HandleDronePartStocksChanged();
};
