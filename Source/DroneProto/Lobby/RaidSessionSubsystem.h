#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RaidAssignmentBase.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TimerManager.h"
#include "RaidSessionSubsystem.generated.h"

UCLASS()
class DRONEPROTO_API URaidSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable, Category="Raid")
	void RequestRaidEntry(const FString& SlotId);

	UFUNCTION(BlueprintPure, Category="Raid")
	bool IsSlotEnabled(const FString& SlotId) const;

	UFUNCTION(BlueprintCallable, Category="Raid|Popups")
	void ShowMatchmakingWait();

	UFUNCTION(BlueprintCallable, Category="Raid|Popups")
	void ShowNoServer();

	UFUNCTION(BlueprintCallable, Category="Raid|Popups")
	void ShowLoadFailed();

	UFUNCTION(BlueprintCallable, Category="Raid|Popups")
	void HideEntryPopups();

	UFUNCTION(BlueprintCallable, Category="Raid|Popups")
	void CancelMatchmaking();

	UPROPERTY(EditDefaultsOnly, Category="Raid|Popups")
	TSubclassOf<UUserWidget> MatchmakingWaitWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category="Raid|Popups")
	TSubclassOf<UUserWidget> NoServerWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category="Raid|Popups")
	TSubclassOf<UUserWidget> LoadFailedWidgetClass;

#if WITH_DEV_AUTOMATION_TESTS
	void SetAssignmentForTest(URaidAssignmentBase* InAssignment);
	void SetSuppressTravelForTest(bool bInSuppressTravel);
	bool IsMatchmakingRetryActiveForTest() const { return bMatchmakingRetryActive; }
	FRaidAssignmentResult GetLastAssignmentResultForTest() const { return LastAssignmentResult; }
	bool WasTravelRequestedForTest() const { return bTravelRequestedForTest; }
	void ResetTravelRequestedForTest();
	void RetryRaidEntryForTest();
	void ExpireMatchmakingWaitForTest();
#endif

private:
	UPROPERTY()
	TObjectPtr<URaidAssignmentBase> Assignment;

	UPROPERTY()
	TObjectPtr<UUserWidget> ActiveMatchmakingWidget;

	UPROPERTY()
	TObjectPtr<UUserWidget> ActiveNoServerWidget;

	UPROPERTY()
	TObjectPtr<UUserWidget> ActiveLoadFailedWidget;

	FTimerHandle MatchmakingRetryTimerHandle;
	FString PendingRaidEntrySlotId;
	double MatchmakingWaitStartTimeSeconds = 0.0;
	bool bMatchmakingRetryActive = false;
	FRaidAssignmentResult LastAssignmentResult;

	static constexpr double MatchmakingTimeoutSeconds = 10.0;
	static constexpr float MatchmakingRetryIntervalSeconds = 1.0f;

#if WITH_DEV_AUTOMATION_TESTS
	bool bSuppressTravelForTest = false;
	bool bTravelRequestedForTest = false;
#endif

	UUserWidget* CreateAndShowPopup(TSubclassOf<UUserWidget> WidgetClass);
	void EvaluateRaidEntry(bool bIsRetry);
	void StartMatchmakingWait();
	void StopMatchmakingRetry();
	void HandleMatchmakingRetry();
	void HandleRaidEntryFailure(const FRaidAssignmentResult& Result);
	void TravelToRaidEndpoint(const FRaidAssignmentResult& Result);
	void RecordAssignmentResult(const FRaidAssignmentResult& Result);
	double GetMatchmakingNowSeconds() const;
};
