#pragma once

#include "CoreMinimal.h"
#include "DronePart.h"
#include "RaidPlayerController.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "DronePartSelectWidget.generated.h"

class UButton;
class UBorder;
class UImage;
class UTextBlock;

UCLASS(Blueprintable, BlueprintType)
class DRONEPROTO_API UDronePartSelectWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "UI")
	ARaidPlayerController* GetOwningRaidPlayerController() const;

	UFUNCTION(BlueprintCallable, Category = "UI")
	void RefreshFromController();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void MovePreview(EDronePartSlot PartSlot, int32 Delta);

	UFUNCTION(BlueprintCallable, Category = "UI")
	void FocusNextSlot();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void FocusPrevSlot();

	UFUNCTION(BlueprintPure, Category = "UI")
	bool IsCombatStartFocused() const;

	UFUNCTION(BlueprintCallable, Category = "UI")
	void ActivateFocusedControl();

	UFUNCTION(BlueprintPure, Category = "UI")
	FName GetPreviewPartIDForSlot(EDronePartSlot PartSlot) const;

	UFUNCTION(BlueprintCallable, Category = "UI")
	void SelectFocusedPart();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void CancelFocusedPart();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void ShowPartSelectionServerError();

	UFUNCTION(BlueprintPure, Category = "UI")
	float GetSelectionRemainingTime() const;

	UFUNCTION(BlueprintPure, Category = "UI")
	EPlayerSelectionState GetCurrentSelectionState() const;

	UFUNCTION(BlueprintPure, Category = "UI")
	FName GetSelectedCorePartID() const;

	UFUNCTION(BlueprintPure, Category = "UI")
	FName GetSelectedLeftWeaponPartID() const;

	UFUNCTION(BlueprintPure, Category = "UI")
	FName GetSelectedRightWeaponPartID() const;

	UFUNCTION(BlueprintPure, Category = "UI")
	bool IsSelectionLocked() const;

	UFUNCTION(BlueprintImplementableEvent, Category = "UI")
	void OnPartStockChangedForUI();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	UFUNCTION()
	void HandlePartSelectUIRefreshRequested();

	UFUNCTION()
	void HandlePartSelectionResult(EPartSlot PartSlot, FName PartID, bool bSuccess, FString Reason);

	UFUNCTION()
	void RetryRefreshFromController();

	UFUNCTION()
	void HandleCorePrevClicked();

	UFUNCTION()
	void HandleCoreNextClicked();

	UFUNCTION()
	void HandleRightPrevClicked();

	UFUNCTION()
	void HandleRightNextClicked();

	UFUNCTION()
	void HandleLeftPrevClicked();

	UFUNCTION()
	void HandleLeftNextClicked();

	UFUNCTION()
	void HandleCombatStartClicked();

	UFUNCTION()
	void ApplyPlanningLayout();

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_ControlGuide = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_CoreDescription = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_RightWeaponDescription = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_LeftWeaponDescription = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_CoreCount = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_RightWeaponCount = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_LeftWeaponCount = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_FocusedSlot = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TimerText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CoreSelectedText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LeftWeaponSelectedText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RightWeaponSelectedText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ResultText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> ServerErrorPopupPanel = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ServerErrorPopupText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Core = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_RightWeapon = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_LeftWeapon = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_CorePrev = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_CoreNext = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_RightPrev = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_RightNext = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_LeftPrev = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_LeftNext = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_CombatStart = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ARaidPlayerController> CachedRaidPlayerController = nullptr;

	EDronePartSlot FocusedSlot = EDronePartSlot::Core;
	// 화면을 처음 열 때의 초기 포커스 설정에는 소리를 내지 않는다. 실제 이동만 센다.
	bool bHasInitializedFocusAudio = false;
	bool bCombatStartFocused = false;
	int32 CorePreviewIndex = 0;
	int32 RightWeaponPreviewIndex = 0;
	int32 LeftWeaponPreviewIndex = 0;

	TArray<FName> CorePartIDs;
	TArray<FName> WeaponPartIDs;

	FTimerHandle RefreshRetryTimerHandle;
	FTimerHandle TimerTextRefreshTimerHandle;
	int32 RefreshRetryCount = 0;
	static constexpr int32 MaxRefreshRetryCount = 10;
	static constexpr float RefreshRetryDelaySeconds = 0.2f;
	static constexpr float TimerTextRefreshIntervalSeconds = 0.1f;

	void InitializeCandidatesFromController();
	void SyncPreviewIndicesToSelection();
	void BindButtonEvents();
	void UnbindButtonEvents();
	void LogOptionalWidgetBindings() const;
	void StartTimerTextRefresh();
	void StopTimerTextRefresh();
	void RefreshTimerText();
	bool ShouldRefreshTimerText() const;
	void SetFocusedSlot(EDronePartSlot NewFocusedSlot);
	void RefreshSlot(EDronePartSlot PartSlot, FName PartID, UTextBlock* DescriptionText, UTextBlock* CountText, UImage* Image) const;
	int32& GetMutablePreviewIndexForSlot(EDronePartSlot PartSlot);
	int32 GetPreviewIndexForSlot(EDronePartSlot PartSlot) const;
	const TArray<FName>& GetPartIDsForSlot(EDronePartSlot PartSlot) const;
	FText GetSlotDisplayText(EDronePartSlot PartSlot) const;
	bool IsInventoryDataReadyForRefresh(FString& OutReason) const;
	void ScheduleRefreshRetry(const FString& Reason);
	void ClearRefreshRetry();
	void SetLoadingText(const FString& Reason);
	void HidePartSelectionServerError();
};
