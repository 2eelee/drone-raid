#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RaidLobbyWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;
class UWidget;
class URaidSessionSubsystem;
class FReply;
struct FCharacterEvent;
struct FKeyEvent;

UENUM(BlueprintType)
enum class ERaidLobbyUIState : uint8
{
	Login,
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

	UFUNCTION(BlueprintCallable, Category="Profile")
	bool SubmitCallsign(const FString& RawCallsign);

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
	void SetCallsignInputForTest(UEditableTextBox* InInput);
	void SetCallsignErrorTextForTest(UTextBlock* InErrorText);
	void HandleCallsignKeyCharForTest(const FCharacterEvent& CharacterEvent);
	bool IsCallsignAutoSubmitPendingForTest() const;
	void CompleteCallsignAutoSubmitDelayForTest();
#endif

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	UFUNCTION()
	void HandleRaidJoinClicked();

	UFUNCTION()
	void HandleCallsignTextChanged(const FText& Text);

	UFUNCTION()
	void HandleCancelMatchmakingClicked();

	UFUNCTION()
	void HandleNoServerConfirmClicked();

	void SetLobbyUIState(ERaidLobbyUIState NewState);
	void RestoreLoginInputFocus();
	void BindCallsignInput();
	void TryAutoSubmitCallsign();
	void CancelPendingCallsignAutoSubmit();
	void HandleCallsignAutoSubmitDelayElapsed();
	FReply HandleCallsignKeyChar(const FCharacterEvent& CharacterEvent);
	void SetCallsignErrorMessage(const FText& Message);
	static void SetOptionalWidgetVisibility(UWidget* Widget, bool bShouldShow);

	UPROPERTY()
	TObjectPtr<URaidSessionSubsystem> RaidSubsystem;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidget> MainLobbyPanel;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidget> CallsignLoginPanel;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidget> CallsignDescription;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UEditableTextBox> CallsignInput;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UButton> CallsignSubmitButton;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> CallsignErrorText;

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
	bool bUpdatingCallsignText = false;
	bool bCallsignAutoSubmitInFlight = false;
	FString PendingCallsign;
	FTimerHandle CallsignAutoSubmitTimerHandle;
};
