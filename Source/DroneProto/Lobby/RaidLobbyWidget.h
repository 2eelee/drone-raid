#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RaidLobbyWidget.generated.h"

class UButton;
class UWidget;
class URaidSessionSubsystem;

UENUM(BlueprintType)
enum class ERaidLobbyUIState : uint8
{
	Main,
	Waiting,
	NoServer,
	Loading
};

UCLASS()
class DRONEPROTO_API URaidLobbyWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Raid")
	void RequestEntry(const FString& SlotId);

	UFUNCTION(BlueprintPure, Category="Raid")
	bool IsSlotEnabled(const FString& SlotId) const;

	UFUNCTION(BlueprintCallable, Category="Raid|Lobby")
	void ShowMainLobby();

	UFUNCTION(BlueprintCallable, Category="Raid|Lobby")
	void ShowWaitingPopup();

	UFUNCTION(BlueprintCallable, Category="Raid|Lobby")
	void ShowNoServerPopup();

	UFUNCTION(BlueprintCallable, Category="Raid|Lobby")
	void ShowLoading();

	UFUNCTION(BlueprintPure, Category="Raid|Lobby")
	ERaidLobbyUIState GetCurrentLobbyUIState() const { return CurrentUIState; }

	UFUNCTION(BlueprintCallable, Category="Raid|Lobby")
	void CancelMatchmakingFromLobby();

	UFUNCTION(BlueprintCallable, Category="Raid|Lobby")
	void ConfirmNoServerFromLobby();

	// Local UI verification hooks only. They do not change server availability,
	// gameplay authority, replicated state, or raid assignment policy.
	UFUNCTION(BlueprintCallable, Category="Raid|Lobby|Debug")
	void ShowDebugWaitingPopup();

	UFUNCTION(BlueprintCallable, Category="Raid|Lobby|Debug")
	void ShowDebugNoServerPopup();

	UFUNCTION(BlueprintCallable, Category="Raid|Lobby|Debug")
	void ResetDebugMainLobby();

#if WITH_DEV_AUTOMATION_TESTS
	void SetRaidSubsystemForTest(URaidSessionSubsystem* InSubsystem);
#endif

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void HandleRaidJoinClicked();

	UFUNCTION()
	void HandleCancelMatchmakingClicked();

	UFUNCTION()
	void HandleNoServerConfirmClicked();

	void SetLobbyUIState(ERaidLobbyUIState NewState);
	static void SetOptionalWidgetVisibility(UWidget* Widget, bool bShouldShow);

	UPROPERTY()
	TObjectPtr<URaidSessionSubsystem> RaidSubsystem;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidget> MainLobbyPanel;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidget> WaitingPopupPanel;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidget> NoServerPopupPanel;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidget> LoadingPanel;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UButton> RaidJoinButton;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UButton> CancelMatchmakingButton;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UButton> NoServerConfirmButton;

	ERaidLobbyUIState CurrentUIState = ERaidLobbyUIState::Main;
	bool bRaidEntryRequestInFlight = false;
};
