#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Containers/Ticker.h"
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
	virtual void Deinitialize() override;

	void SetActiveLobbyWidget(URaidLobbyWidget* InWidget);
	void ClearActiveLobbyWidget(URaidLobbyWidget* InWidget);

	UFUNCTION(BlueprintCallable, Category="Raid")
	void RequestRaidEntry(const FString& SlotId);

	UFUNCTION(BlueprintCallable, Category="Profile")
	bool TryLoginWithCallsign(const FString& RawCallsign);

	UFUNCTION(BlueprintCallable, Category="Profile")
	bool TryLoginWithCallsignAndTravel(const FString& RawCallsign);

	UFUNCTION(BlueprintPure, Category="Profile")
	FString GetCallsign() const { return Callsign; }

	UFUNCTION(BlueprintPure, Category="Profile")
	bool HasCompletedTutorial() const { return bHasCompletedTutorial; }

	UFUNCTION(BlueprintPure, Category="Profile")
	bool IsCallsignIdentified() const { return bCallsignIdentified; }

	UFUNCTION(BlueprintPure, Category="Profile")
	FName GetPostLoginMapName() const;

	UFUNCTION(BlueprintPure, Category="Profile")
	FString GetPostLoginTravelOptions() const;

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
	bool IsRaidLoadWatchdogActiveForTest() const { return bRaidLoadWatchdogActive; }
	int32 GetRaidLoadFailureHandleCountForTest() const { return RaidLoadFailureHandleCountForTest; }
	void CompleteRaidLoadForTest();
	void ExpireRaidLoadWatchdogForTest();
	void NotifyRaidTravelFailureForTest();
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
	FTSTicker::FDelegateHandle RaidLoadWatchdogTickerHandle;
	FDelegateHandle PostLoadMapDelegateHandle;
	FDelegateHandle TravelFailureDelegateHandle;
	FDelegateHandle NetworkFailureDelegateHandle;
	FString PendingRaidEntrySlotId;
	FServerEndpoint PendingRaidLoadEndpoint;
	TWeakObjectPtr<UWorld> RaidLoadSourceWorld;
	double MatchmakingWaitStartTimeSeconds = 0.0;
	double RaidLoadWatchdogStartTimeSeconds = 0.0;
	bool bMatchmakingRetryActive = false;
	bool bRaidLoadWatchdogActive = false;
	bool bRaidLoadFailureHandled = false;
	bool bPendingLoadFailedPopupAfterLobbyReturn = false;
	bool bLobbyReturnRequestedForLoadFailure = false;
	FRaidAssignmentResult LastAssignmentResult;
	FString Callsign = TEXT("AAA");
	bool bHasCompletedTutorial = false;
	bool bCallsignIdentified = false;
	FString ProfileSaveSlotName = TEXT("DroneLocalProfile");

	static constexpr double MatchmakingTimeoutSeconds = 10.0;
	static constexpr float MatchmakingRetryIntervalSeconds = 1.0f;
	static constexpr double RaidLoadTimeoutSeconds = 10.0;
	static constexpr float RaidLoadWatchdogTickIntervalSeconds = 0.10f;
	static constexpr int32 ProfileSaveUserIndex = 0;

#if WITH_DEV_AUTOMATION_TESTS
	bool bSuppressTravelForTest = false;
	bool bTravelRequestedForTest = false;
	int32 TravelRequestCountForTest = 0;
	int32 RaidLoadFailureHandleCountForTest = 0;
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
	void StartRaidLoadWatchdog(UWorld* SourceWorld, const FServerEndpoint& Endpoint);
	void StopRaidLoadWatchdog();
	bool HandleRaidLoadWatchdogTick(float DeltaSeconds);
	void HandlePostLoadMap(UWorld* LoadedWorld);
	void CompletePendingRaidLoad();
	void HandlePendingRaidLoadFailure(const FString& DebugReason);
	void PresentPendingRaidLoadFailure();
	bool IsLobbyWorld(const UWorld* World) const;
	void RecordAssignmentResult(const FRaidAssignmentResult& Result);
	double GetMatchmakingNowSeconds() const;
};
