#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/SaveGame.h"
#include "RaidAssignmentBase.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TimerManager.h"
#include "RaidSessionSubsystem.generated.h"

class URaidLobbyWidget;

UCLASS()
class DRONEPROTO_API UDroneLocalProfileSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FString Callsign = TEXT("AAA");

	UPROPERTY()
	bool bHasCompletedTutorial = false;
};

UCLASS()
class DRONEPROTO_API URaidSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	void SetActiveLobbyWidget(URaidLobbyWidget* InWidget);
	void ClearActiveLobbyWidget(URaidLobbyWidget* InWidget);

	UFUNCTION(BlueprintCallable, Category="Raid")
	void RequestRaidEntry(const FString& SlotId);

	UFUNCTION(BlueprintCallable, Category="Profile")
	bool TryLoginWithCallsign(const FString& RawCallsign);

	UFUNCTION(BlueprintPure, Category="Profile")
	FString GetCallsign() const { return Callsign; }

	UFUNCTION(BlueprintPure, Category="Profile")
	bool HasCompletedTutorial() const { return bHasCompletedTutorial; }

	UFUNCTION(BlueprintPure, Category="Profile")
	FName GetPostLoginMapName() const;

	UFUNCTION(BlueprintCallable, Category="Profile")
	bool MarkTutorialCompleted();

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
	int32 GetTravelRequestCountForTest() const { return TravelRequestCountForTest; }
	void ResetTravelRequestedForTest();
	void RetryRaidEntryForTest();
	void ExpireMatchmakingWaitForTest();
	void SetProfileSaveSlotForTest(const FString& InSlotName) { ProfileSaveSlotName = InSlotName; }
	bool ReloadLocalProfileForTest();
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

	UPROPERTY()
	TObjectPtr<URaidLobbyWidget> ActiveLobbyWidget;

	FTimerHandle MatchmakingRetryTimerHandle;
	FString PendingRaidEntrySlotId;
	double MatchmakingWaitStartTimeSeconds = 0.0;
	bool bMatchmakingRetryActive = false;
	FRaidAssignmentResult LastAssignmentResult;
	FString Callsign = TEXT("AAA");
	bool bHasCompletedTutorial = false;
	FString ProfileSaveSlotName = TEXT("DroneLocalProfile");

	static constexpr double MatchmakingTimeoutSeconds = 10.0;
	static constexpr float MatchmakingRetryIntervalSeconds = 1.0f;
	static constexpr int32 ProfileSaveUserIndex = 0;

#if WITH_DEV_AUTOMATION_TESTS
	bool bSuppressTravelForTest = false;
	bool bTravelRequestedForTest = false;
	int32 TravelRequestCountForTest = 0;
#endif

	UUserWidget* CreateAndShowPopup(TSubclassOf<UUserWidget> WidgetClass);
	void EvaluateRaidEntry(bool bIsRetry);
	void StartMatchmakingWait();
	void StopMatchmakingRetry();
	void HandleMatchmakingRetry();
	void HandleRaidEntryFailure(const FRaidAssignmentResult& Result);
	bool LoadLocalProfile();
	bool SaveLocalProfile() const;
	static bool TryNormalizeCallsign(const FString& RawCallsign, FString& OutCallsign);
	void TravelToRaidEndpoint(const FRaidAssignmentResult& Result);
	void RecordAssignmentResult(const FRaidAssignmentResult& Result);
	double GetMatchmakingNowSeconds() const;
};
