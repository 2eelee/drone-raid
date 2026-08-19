#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DroneCombatTypes.h"
#include "DroneReportWidget.generated.h"

class UButton;
class UTextBlock;
class FReply;
struct FKeyEvent;

UCLASS()
class DRONEPROTO_API UDroneReportWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Drone|Report")
	void RefreshReport(const FDroneReportData& InReportData);

	UFUNCTION(BlueprintPure, Category = "Drone|Report")
	FText GetSurvivalTimeText() const;

	UFUNCTION(BlueprintPure, Category = "Drone|Report")
	FText GetCallsignText() const;

	UFUNCTION(BlueprintPure, Category = "Drone|Report")
	FText GetBossDamageText() const;

	UFUNCTION(BlueprintPure, Category = "Drone|Report")
	FText GetBossDamageRatioText() const;

	UFUNCTION(BlueprintPure, Category = "Drone|Report")
	FText GetMoveDistanceText() const;

	UFUNCTION(BlueprintPure, Category = "Drone|Report")
	FText GetHealAmountText() const;

	UFUNCTION(BlueprintPure, Category = "Drone|Report")
	FText GetBonusScoreText() const;

	UFUNCTION(BlueprintPure, Category = "Drone|Report")
	FText GetAchievedBonusText() const;

	UFUNCTION(BlueprintPure, Category = "Drone|Report")
	FText GetGradeText() const;

	UFUNCTION(BlueprintPure, Category = "Drone|Report")
	static FText GetBonusTypeDisplayText(EDroneReportBonusType BonusType);

	UFUNCTION(BlueprintPure, Category = "Drone|Report")
	static FText GetGradeDisplayText(EDroneReportGrade Grade);

	UFUNCTION(BlueprintCallable, Category = "Drone|Report")
	void RequestReturnToLobby();

	/**
	 * 리포트를 화면에서 내리고 확인 버튼을 다시 쓸 수 있는 상태로 되돌린다.
	 *
	 * 컨트롤러가 위젯 인스턴스를 캐시해 다음 리포트에 재사용하므로, 되돌리지 않으면 두 번째 확인이
	 * AlreadyRequested로 무시되고 버튼도 비활성으로 남는다. 위젯이 자기 상태를 스스로 정리하게 두어
	 * 바깥에서 UI 내부를 뒤지지 않도록 한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Drone|Report")
	void DismissReport();

#if WITH_DEV_AUTOMATION_TESTS
	void SetSuppressReturnToLobbyTravelForTest(bool bInSuppressTravel) { bSuppressReturnToLobbyTravelForTest = bInSuppressTravel; }
	int32 GetReturnToLobbyTravelRequestCountForTest() const { return ReturnToLobbyTravelRequestCountForTest; }
	bool IsReturnToLobbyRequestedForTest() const { return bReturnToLobbyRequested; }

	/** 확인 버튼을 붙이고 `NativeConstruct`와 같은 배선 절차를 그대로 돌린다. */
	void BindReturnToLobbyButtonForTest(UButton* InButton);
	/** 직전 배선에서 걷어낸 외부 클릭 경로 수. 0이면 이 위젯이 이미 단독으로 소유하고 있었다. */
	int32 GetReclaimedConfirmBindingCountForTest() const { return ReclaimedConfirmBindingCount; }
	int32 GetConfirmButtonBindingCountForTest() const;
	bool IsConfirmButtonOwnedByNativeHandlerForTest();
#endif

protected:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Drone|Report")
	TObjectPtr<UTextBlock> SurvivalTimeText = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Drone|Report")
	TObjectPtr<UTextBlock> CallsignText = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Drone|Report")
	TObjectPtr<UTextBlock> BossDamageText = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Drone|Report")
	TObjectPtr<UTextBlock> BossDamageRatioText = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Drone|Report")
	TObjectPtr<UTextBlock> MoveDistanceText = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Drone|Report")
	TObjectPtr<UTextBlock> HealAmountText = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Drone|Report")
	TObjectPtr<UTextBlock> BonusScoreText = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Drone|Report")
	TObjectPtr<UTextBlock> AchievedBonusText = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Drone|Report")
	TObjectPtr<UTextBlock> GradeText = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Drone|Report")
	TObjectPtr<UTextBlock> ResultTitleText = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Drone|Report")
	TObjectPtr<UTextBlock> ReportTitleText = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Drone|Report")
	TObjectPtr<UButton> ReturnToLobbyButton = nullptr;

private:
	UFUNCTION()
	void HandleReturnToLobbyClicked();

	/**
	 * 확인 버튼의 클릭 경로를 이 클래스가 단독으로 갖게 만든다.
	 *
	 * 확인을 소유 컨트롤러에게 먼저 묻는 계약(`TryHandleDroneReportConfirmedForLocalPlayer`)은
	 * 버튼에서 나가는 길이 하나일 때만 성립한다. 길이 둘이면 컨트롤러가 이동을 막아도 다른 길이
	 * 그대로 이동해 버린다. 그래서 배선 시점에 이 위젯에 걸린 클릭 경로를 정리하고 다시 건다.
	 */
	void BindReturnToLobbyButton();

	FText CachedSurvivalTimeText;
	FText CachedCallsignText;
	FText CachedBossDamageText;
	FText CachedBossDamageRatioText;
	FText CachedMoveDistanceText;
	FText CachedHealAmountText;
	FText CachedBonusScoreText;
	FText CachedAchievedBonusText;
	FText CachedGradeText;
	bool bReturnToLobbyRequested = false;
	int32 ReclaimedConfirmBindingCount = 0;

#if WITH_DEV_AUTOMATION_TESTS
	bool bSuppressReturnToLobbyTravelForTest = false;
	int32 ReturnToLobbyTravelRequestCountForTest = 0;
#endif

	static FText BuildAchievedBonusText(const TArray<EDroneReportBonusType>& AchievedBonusList);
	static FText BuildAchievedBonusText(
		const TArray<EDroneReportBonusType>& AchievedBonusList,
		const TArray<FText>& AchievedBonusDisplayNames);
	static void SetOptionalText(UTextBlock* TextBlock, const FText& Text);
};
