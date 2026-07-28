#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DroneCombatTypes.h"
#include "DroneReportWidget.generated.h"

class UButton;
class UTextBlock;

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

#if WITH_DEV_AUTOMATION_TESTS
	void SetSuppressReturnToLobbyTravelForTest(bool bInSuppressTravel) { bSuppressReturnToLobbyTravelForTest = bInSuppressTravel; }
	int32 GetReturnToLobbyTravelRequestCountForTest() const { return ReturnToLobbyTravelRequestCountForTest; }
#endif

protected:
	virtual void NativeConstruct() override;

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

#if WITH_DEV_AUTOMATION_TESTS
	bool bSuppressReturnToLobbyTravelForTest = false;
	int32 ReturnToLobbyTravelRequestCountForTest = 0;
#endif

	static FText BuildAchievedBonusText(const TArray<EDroneReportBonusType>& AchievedBonusList);
	static void SetOptionalText(UTextBlock* TextBlock, const FText& Text);
};
