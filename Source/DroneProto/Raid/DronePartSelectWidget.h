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
	/**
	 * 부품 선택 화면 좌상단에 고정 표시하는 조작 방식 안내다(`SELECT-UI-02`).
	 * 문구는 기획 원문 `현현_드론부품선택시스템_기획서.md:359-371`의 `(5) 조작 방식 팝업`이 문자 단위로 확정한다.
	 * `Text_ControlGuide`가 `BindWidgetOptional`이라 위젯 인스턴스만으로는 검증할 수 없어 static getter로 분리했다.
	 */
	static FText GetControlGuideText();

	/**
	 * 슬롯 하단 회색 원형 안에 들어가는 남은 수량 표시다(`SELECT-UI-01`·`STOCK-07`).
	 * 목업 `08` 확정 계약이 `슬롯 하단 중앙에 회색 원형(남은 수량 표시 위치)`이고 그 안에는 **숫자만** 들어간다.
	 * `남은 수량`은 요소 이름(`:334,342,350`)이자 목업의 설명 주석이지 출력 문자열이 아니며,
	 * `:133-149`의 `레이저포 남은 수량 2 → 남은 수량 1`은 재고 변화를 설명하는 시퀀스 서술이다.
	 * 총량(`/ MaxCount`)도 근거가 없다 — `STOCK-07`은 복제 반영 계약이지 표기 형식이 아니다.
	 * `Text_*Count`가 `BindWidgetOptional`이라 위젯 인스턴스만으로는 검증할 수 없어 static으로 분리했다.
	 */
	static FText FormatPartCountText(int32 CurrentCount);

	/** 서버 남은 시간을 고정 `SS:ff`(ff = 10ms) 선택 타이머 문자열로 만든다. */
	static FText FormatSelectionTimerText(float RemainingSeconds);

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
	TObjectPtr<UTextBlock> Text_CoreName = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_CoreBody = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_RightName = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_RightBody = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_LeftName = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_LeftBody = nullptr;

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
	void RefreshSlot(EDronePartSlot PartSlot, FName PartID, UTextBlock* NameText, UTextBlock* BodyText, UTextBlock* CountText, UImage* Image) const;
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
