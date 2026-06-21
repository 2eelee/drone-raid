#pragma once

#include "CoreMinimal.h"
#include "DronePart.h"
#include "DronePartReturnManager.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "RaidPlayerController.generated.h"

class ADronePartInventory;
class ADrone;
class UTexture2D;
class UDronePartReturnManager;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnPartSelectionResult, EPartSlot, Slot, FName, PartID, bool, bSuccess, FString, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSelectedPartsChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPartSelectUIRefreshRequested);

UENUM(BlueprintType)
enum class EPlayerSelectionState : uint8
{
	Selecting UMETA(DisplayName = "Selecting"),
	Locked    UMETA(DisplayName = "Locked"),
	InBattle  UMETA(DisplayName = "InBattle"),
};

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

	UFUNCTION(BlueprintCallable, Category = "Raid|Test")
	void RequestApplyTestDamageToDrone(int32 DamageAmount);

	UFUNCTION(BlueprintCallable, Category = "Raid|Test")
	void RequestRaidEndReturnTest(FName Reason);

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
	EPlayerSelectionState GetPlayerSelectionState() const;

	UFUNCTION(BlueprintPure, Category = "Drone Parts")
	EPlayerSelectionState GetCurrentSelectionState() const;

	UFUNCTION(BlueprintPure, Category = "Drone Parts")
	bool IsSelectionLocked() const;

	UFUNCTION(BlueprintPure, Category = "Drone Parts")
	float GetSelectionRemainingTime() const;

	UFUNCTION(BlueprintPure, Category = "Drone Parts")
	float GetSelectionEndServerTime() const;

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

	static FString BuildStableControllerLogString(const AController* Controller);
	static const TCHAR* SelectionStateToLogString(EPlayerSelectionState State);

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

	UFUNCTION(BlueprintCallable, Category = "UI")
	void RefreshSelectionUI();

	void SetSelectedPartIDForSlotForServer(EPartSlot Slot, FName PartID);
	void SetEquippedPartIDForSlotForServer(EPartSlot Slot, FName PartID);
	bool ReturnSelectedPartsForServer(EDronePartReturnReason Reason);
	bool ReturnEquippedPartsForServer(EDronePartReturnReason Reason);
	bool ReturnSingleSelectedPartForServer(EPartSlot Slot, EDronePartReturnReason Reason);
	bool ReturnSingleEquippedPartForServer(EPartSlot Slot, EDronePartReturnReason Reason);
	void HandleSelectionTimerExpiredForServer();

#if WITH_DEV_AUTOMATION_TESTS
	void SetDronePartReturnManagerForTest(UDronePartReturnManager* InReturnManager);
#endif

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Drone Parts")
	void Server_RequestSelectPart(EPartSlot Slot, FName NewPartID);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Drone Parts")
	void Server_RequestCancelPart(EPartSlot Slot);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Raid")
	void Server_RequestReadyForRaid();

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Raid")
	void Server_RequestStartSelectionTimer();

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Raid|Test")
	void Server_RequestApplyTestDamageToDrone(int32 DamageAmount);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Raid|Test")
	void Server_RequestRaidEndReturnTest(FName Reason);

	UFUNCTION(Client, Reliable, Category = "Drone Parts")
	void Client_NotifyPartSelectionResult(EPartSlot Slot, FName PartID, bool bSuccess, const FString& Reason);

	UFUNCTION(Client, Reliable, Category = "Raid")
	void Client_NotifyRaidReadyResult(bool bSuccess, const FString& Reason, FName CorePartID, FName LeftWeaponPartID, FName RightWeaponPartID);

	UFUNCTION(Exec)
	void D4SelectPart(FString SlotName, FString PartIDText);

	UFUNCTION(Exec)
	void D4CancelPart(FString SlotName);

	UFUNCTION(Exec)
	void D6KillDrone();

	UFUNCTION(Exec)
	void D6RaidEndReturn(FString ReasonText);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	static constexpr float SelectionDurationSeconds = 15.0f;

	UPROPERTY(ReplicatedUsing = OnRep_PlayerSelectionState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Drone Parts", meta = (AllowPrivateAccess = "true"))
	EPlayerSelectionState PlayerSelectionState = EPlayerSelectionState::Selecting;

	UPROPERTY(ReplicatedUsing = OnRep_SelectionEndServerTime, VisibleInstanceOnly, BlueprintReadOnly, Category = "Drone Parts", meta = (AllowPrivateAccess = "true"))
	float SelectionEndServerTime = 0.0f;

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

#if WITH_DEV_AUTOMATION_TESTS
	UDronePartReturnManager* TestDronePartReturnManager = nullptr;
#endif

	FTimerHandle SelectionTimerHandle;

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
	void SetPlayerSelectionStateForServer(EPlayerSelectionState NewState);
	float GetSelectionServerTimeSeconds() const;
	void StartSelectionTimerForServer();
	void StopSelectionTimerForServer(const FString& Reason, bool bLogSummary);
	bool ProcessReadyForRaidForServer(bool bAutoReady);

	UFUNCTION()
	void HandleDronePartStocksChanged();

	UFUNCTION()
	void OnRep_PlayerSelectionState();

	UFUNCTION()
	void OnRep_SelectionEndServerTime();
};
