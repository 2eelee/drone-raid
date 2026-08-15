#include "RaidLobbyWidget.h"

#include "RaidSessionSubsystem.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "InputCoreTypes.h"
#include "TimerManager.h"
#include "Widgets/Input/SEditableTextBox.h"

namespace
{
constexpr float CallsignAutoSubmitDelaySeconds = 0.45f;

const TCHAR* ToRaidLobbyVisibilityText(ESlateVisibility Visibility)
{
	switch (Visibility)
	{
	case ESlateVisibility::Visible:
		return TEXT("Visible");
	case ESlateVisibility::Collapsed:
		return TEXT("Collapsed");
	case ESlateVisibility::Hidden:
		return TEXT("Hidden");
	case ESlateVisibility::HitTestInvisible:
		return TEXT("HitTestInvisible");
	case ESlateVisibility::SelfHitTestInvisible:
		return TEXT("SelfHitTestInvisible");
	default:
		return TEXT("Unknown");
	}
}

const TCHAR* ToRaidLobbyUIStateText(ERaidLobbyUIState State)
{
	switch (State)
	{
	case ERaidLobbyUIState::Login:
		return TEXT("Login");
	case ERaidLobbyUIState::Main:
		return TEXT("Main");
	case ERaidLobbyUIState::Waiting:
		return TEXT("Waiting");
	case ERaidLobbyUIState::NoServer:
		return TEXT("NoServer");
	case ERaidLobbyUIState::Loading:
		return TEXT("Loading");
	default:
		return TEXT("Unknown");
	}
}

FString SanitizeCallsignInput(const FString& RawText)
{
	FString Result;
	Result.Reserve(3);

	for (const TCHAR Character : RawText)
	{
		const TCHAR UppercaseCharacter = FChar::ToUpper(Character);
		if (UppercaseCharacter >= TEXT('A') && UppercaseCharacter <= TEXT('Z'))
		{
			Result.AppendChar(UppercaseCharacter);
			if (Result.Len() == 3)
			{
				break;
			}
		}
	}

	return Result;
}
}

void URaidLobbyWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);

	if (UGameInstance* GI = GetGameInstance())
	{
		RaidSubsystem = GI->GetSubsystem<URaidSessionSubsystem>();
	}

	if (RaidSubsystem)
	{
		RaidSubsystem->SetActiveLobbyWidget(this);
	}

	if (RaidJoinButton)
	{
		RaidJoinButton->OnClicked.AddUniqueDynamic(this, &URaidLobbyWidget::HandleRaidJoinClicked);
	}

	BindCallsignInput();
	SetOptionalWidgetVisibility(CallsignSubmitButton, false);
	SetOptionalWidgetVisibility(CallsignDescription, false);

	if (CancelMatchmakingButton)
	{
		CancelMatchmakingButton->OnClicked.AddUniqueDynamic(this, &URaidLobbyWidget::HandleCancelMatchmakingClicked);
	}

	if (NoServerConfirmButton)
	{
		NoServerConfirmButton->OnClicked.AddUniqueDynamic(this, &URaidLobbyWidget::HandleNoServerConfirmClicked);
	}

	const UWidget* RootWidget = GetRootWidget();
	const UPanelWidget* RootPanel = Cast<UPanelWidget>(RootWidget);
	const int32 RootChildCount = RootPanel ? RootPanel->GetChildrenCount() : -1;
	const ESlateVisibility RootVisibility = RootWidget ? RootWidget->GetVisibility() : ESlateVisibility::Collapsed;

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] RaidLobbyWidgetNativeConstruct Widget=%s RaidSubsystemValid=%d RootWidget=%s RootVisibility=%s RootChildCount=%d MainLobbyPanel=%d WaitingPopupPanel=%d NoServerPopupPanel=%d LoadingPanel=%d RaidJoinButton=%d CancelMatchmakingButton=%d NoServerConfirmButton=%d"),
		*GetName(),
		RaidSubsystem ? 1 : 0,
		*GetNameSafe(RootWidget),
		ToRaidLobbyVisibilityText(RootVisibility),
		RootChildCount,
		MainLobbyPanel ? 1 : 0,
		WaitingPopupPanel ? 1 : 0,
		NoServerPopupPanel ? 1 : 0,
		LoadingPanel ? 1 : 0,
		RaidJoinButton ? 1 : 0,
		CancelMatchmakingButton ? 1 : 0,
		NoServerConfirmButton ? 1 : 0);

	if (CallsignErrorText)
	{
		CallsignErrorText->SetVisibility(ESlateVisibility::Hidden);
	}

	SetLobbyUIState(RaidSubsystem && RaidSubsystem->IsCallsignIdentified()
		? ERaidLobbyUIState::Main
		: ERaidLobbyUIState::Login);
}

void URaidLobbyWidget::NativeDestruct()
{
	CancelPendingCallsignAutoSubmit();

	if (RaidSubsystem)
	{
		RaidSubsystem->ClearActiveLobbyWidget(this);
	}

	Super::NativeDestruct();
}

FReply URaidLobbyWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (CurrentUIState == ERaidLobbyUIState::Main && InKeyEvent.GetKey() == EKeys::Z)
	{
		HandleRaidJoinClicked();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void URaidLobbyWidget::RequestEntry(const FString& SlotId)
{
	if (bRaidEntryRequestInFlight)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] RaidLobbyRequestEntryIgnored Widget=%s Slot=%s Reason=EntryInFlight State=%s Scope=LocalUIOnly"),
			*GetName(),
			*SlotId,
			ToRaidLobbyUIStateText(CurrentUIState));
		return;
	}

	if (!RaidSubsystem)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			RaidSubsystem = GI->GetSubsystem<URaidSessionSubsystem>();
		}
	}

	if (RaidSubsystem)
	{
		RaidSubsystem->SetActiveLobbyWidget(this);
	}

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] RaidLobbyRequestEntryClicked Widget=%s Slot=%s RaidSubsystemValid=%d"),
		*GetName(),
		*SlotId,
		RaidSubsystem ? 1 : 0);

	if (RaidSubsystem)
	{
		bRaidEntryRequestInFlight = true;
		RaidSubsystem->RequestRaidEntry(SlotId);
	}
}

bool URaidLobbyWidget::SubmitCallsign(const FString& RawCallsign)
{
	if (!RaidSubsystem)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			RaidSubsystem = GI->GetSubsystem<URaidSessionSubsystem>();
		}
	}

	const bool bTutorialComplete = RaidSubsystem && RaidSubsystem->HasCompletedTutorial();
	const bool bAccepted = RaidSubsystem
		&& (bTutorialComplete
			? RaidSubsystem->TryLoginWithCallsign(RawCallsign)
			: RaidSubsystem->TryLoginWithCallsignAndTravel(RawCallsign));

	if (CallsignErrorText)
	{
		SetCallsignErrorMessage(bAccepted
			? FText::GetEmpty()
			: FText::FromString(TEXT("영문 3자를 입력해주세요.")));
	}

	if (bAccepted && bTutorialComplete)
	{
		ShowMainLobby();
	}

	return bAccepted;
}

bool URaidLobbyWidget::IsSlotEnabled(const FString& SlotId) const
{
	return RaidSubsystem ? RaidSubsystem->IsSlotEnabled(SlotId) : false;
}

void URaidLobbyWidget::ShowMainLobby()
{
	SetLobbyUIState(ERaidLobbyUIState::Main);
	SetKeyboardFocus();
}

void URaidLobbyWidget::ShowWaitingPopup()
{
	SetLobbyUIState(ERaidLobbyUIState::Waiting);
}

void URaidLobbyWidget::ShowNoServerPopup()
{
	SetLobbyUIState(ERaidLobbyUIState::NoServer);
}

void URaidLobbyWidget::ShowLoading()
{
	SetLobbyUIState(ERaidLobbyUIState::Loading);
}

void URaidLobbyWidget::CancelMatchmakingFromLobby()
{
	if (!RaidSubsystem)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			RaidSubsystem = GI->GetSubsystem<URaidSessionSubsystem>();
		}
	}

	if (RaidSubsystem)
	{
		RaidSubsystem->CancelMatchmaking();
	}
	else
	{
		ShowMainLobby();
	}
}

void URaidLobbyWidget::ConfirmNoServerFromLobby()
{
	ShowMainLobby();
}

void URaidLobbyWidget::ShowDebugWaitingPopup()
{
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] LobbyUIDebugHook Action=ShowWaitingPopup Scope=LocalUIOnly GameplayAuthority=Unaffected"));
	ShowWaitingPopup();
}

void URaidLobbyWidget::ShowDebugNoServerPopup()
{
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] LobbyUIDebugHook Action=ShowNoServerPopup Scope=LocalUIOnly GameplayAuthority=Unaffected"));
	ShowNoServerPopup();
}

void URaidLobbyWidget::ResetDebugMainLobby()
{
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] LobbyUIDebugHook Action=ResetMainLobby Scope=LocalUIOnly GameplayAuthority=Unaffected"));
	ShowMainLobby();
}

void URaidLobbyWidget::HandleRaidJoinClicked()
{
	RequestEntry(TEXT("A"));
}

void URaidLobbyWidget::HandleCallsignTextChanged(const FText& Text)
{
	if (!CallsignInput || bUpdatingCallsignText)
	{
		return;
	}

	const FString RawText = Text.ToString();
	const FString SanitizedText = SanitizeCallsignInput(RawText);
	if (!RawText.Equals(SanitizedText, ESearchCase::CaseSensitive))
	{
		bool bContainsInvalidCharacter = false;
		int32 LetterCount = 0;
		for (const TCHAR Character : RawText)
		{
			const TCHAR UppercaseCharacter = FChar::ToUpper(Character);
			if (UppercaseCharacter >= TEXT('A') && UppercaseCharacter <= TEXT('Z'))
			{
				++LetterCount;
			}
			else
			{
				bContainsInvalidCharacter = true;
			}
		}

		SetCallsignErrorMessage(FText::FromString(bContainsInvalidCharacter
			? TEXT("영문만 입력할 수 있습니다.")
			: LetterCount > 3
				? TEXT("영문 3자까지만 입력할 수 있습니다.")
				: TEXT("")));

		bUpdatingCallsignText = true;
		CallsignInput->SetText(FText::FromString(SanitizedText));
		bUpdatingCallsignText = false;
	}
	else
	{
		SetCallsignErrorMessage(FText::GetEmpty());
	}

	TryAutoSubmitCallsign();
}

void URaidLobbyWidget::HandleCancelMatchmakingClicked()
{
	CancelMatchmakingFromLobby();
}

void URaidLobbyWidget::HandleNoServerConfirmClicked()
{
	ConfirmNoServerFromLobby();
}

void URaidLobbyWidget::SetLobbyUIState(ERaidLobbyUIState NewState)
{
	CurrentUIState = NewState;
	bRaidEntryRequestInFlight = (NewState == ERaidLobbyUIState::Waiting || NewState == ERaidLobbyUIState::Loading);

	SetOptionalWidgetVisibility(CallsignLoginPanel, NewState == ERaidLobbyUIState::Login);
	SetOptionalWidgetVisibility(MainLobbyPanel, NewState == ERaidLobbyUIState::Main);
	SetOptionalWidgetVisibility(WaitingPopupPanel, NewState == ERaidLobbyUIState::Waiting);
	SetOptionalWidgetVisibility(NoServerPopupPanel, NewState == ERaidLobbyUIState::NoServer);
	SetOptionalWidgetVisibility(LoadingPanel, NewState == ERaidLobbyUIState::Loading);

	if (NewState == ERaidLobbyUIState::Login && CallsignInput)
	{
		SetDesiredFocusWidget(CallsignInput);

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimerForNextTick(
				FTimerDelegate::CreateUObject(this, &URaidLobbyWidget::RestoreLoginInputFocus));
		}
		else
		{
			RestoreLoginInputFocus();
		}
	}
	else
	{
		SetDesiredFocusWidget(NAME_None);
	}

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] LobbyUIState State=%s Widget=%s MainLobbyPanel=%d WaitingPopupPanel=%d NoServerPopupPanel=%d LoadingPanel=%d"),
		ToRaidLobbyUIStateText(NewState),
		*GetName(),
		MainLobbyPanel ? 1 : 0,
		WaitingPopupPanel ? 1 : 0,
		NoServerPopupPanel ? 1 : 0,
		LoadingPanel ? 1 : 0);
}

void URaidLobbyWidget::RestoreLoginInputFocus()
{
	if (CurrentUIState == ERaidLobbyUIState::Login && CallsignInput)
	{
		CallsignInput->SetKeyboardFocus();
	}
}

void URaidLobbyWidget::SetOptionalWidgetVisibility(UWidget* Widget, bool bShouldShow)
{
	if (Widget)
	{
		Widget->SetVisibility(bShouldShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void URaidLobbyWidget::BindCallsignInput()
{
	CancelPendingCallsignAutoSubmit();

	if (CallsignInput)
	{
		CallsignInput->OnTextChanged.AddUniqueDynamic(this, &URaidLobbyWidget::HandleCallsignTextChanged);
		bUpdatingCallsignText = true;
		CallsignInput->SetText(FText::GetEmpty());
		CallsignInput->SetHintText(FText::FromString(TEXT("AAA")));
		bUpdatingCallsignText = false;
		bCallsignAutoSubmitInFlight = false;

		TSharedRef<SEditableTextBox> SlateInput =
			StaticCastSharedRef<SEditableTextBox>(CallsignInput->TakeWidget());
		SlateInput->SetOnKeyCharHandler(FOnKeyChar::CreateWeakLambda(
			this,
			[this](const FGeometry&, const FCharacterEvent& CharacterEvent)
			{
				return HandleCallsignKeyChar(CharacterEvent);
			}));
	}
}

void URaidLobbyWidget::TryAutoSubmitCallsign()
{
	const FString CurrentCallsign = CallsignInput
		? CallsignInput->GetText().ToString()
		: FString();
	if (!CallsignInput || !RaidSubsystem || bCallsignAutoSubmitInFlight || CurrentCallsign.Len() != 3)
	{
		CancelPendingCallsignAutoSubmit();
		return;
	}

	if (PendingCallsign == CurrentCallsign)
	{
		return;
	}

	CancelPendingCallsignAutoSubmit();
	PendingCallsign = CurrentCallsign;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			CallsignAutoSubmitTimerHandle,
			this,
			&URaidLobbyWidget::HandleCallsignAutoSubmitDelayElapsed,
			CallsignAutoSubmitDelaySeconds,
			false);
	}
}

void URaidLobbyWidget::CancelPendingCallsignAutoSubmit()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CallsignAutoSubmitTimerHandle);
	}
	CallsignAutoSubmitTimerHandle.Invalidate();
	PendingCallsign.Reset();
}

void URaidLobbyWidget::HandleCallsignAutoSubmitDelayElapsed()
{
	const FString ExpectedCallsign = PendingCallsign;
	PendingCallsign.Reset();
	CallsignAutoSubmitTimerHandle.Invalidate();

	if (!CallsignInput
		|| !RaidSubsystem
		|| bCallsignAutoSubmitInFlight
		|| ExpectedCallsign.Len() != 3
		|| CallsignInput->GetText().ToString() != ExpectedCallsign)
	{
		return;
	}

	bCallsignAutoSubmitInFlight = true;
	if (!SubmitCallsign(ExpectedCallsign))
	{
		bCallsignAutoSubmitInFlight = false;
	}
}

FReply URaidLobbyWidget::HandleCallsignKeyChar(const FCharacterEvent& CharacterEvent)
{
	const TCHAR Character = CharacterEvent.GetCharacter();
	if (Character == TCHAR(8))
	{
		CancelPendingCallsignAutoSubmit();
		FString CurrentCallsign = SanitizeCallsignInput(CallsignInput->GetText().ToString());
		if (!CurrentCallsign.IsEmpty())
		{
			CurrentCallsign.LeftChopInline(1);
			CallsignInput->SetText(FText::FromString(CurrentCallsign));
		}
		SetCallsignErrorMessage(FText::GetEmpty());
		return FReply::Handled();
	}

	if (FChar::IsControl(Character))
	{
		return FReply::Unhandled();
	}

	const TCHAR UppercaseCharacter = FChar::ToUpper(Character);
	if (UppercaseCharacter >= TEXT('A') && UppercaseCharacter <= TEXT('Z'))
	{
		FString CurrentCallsign = SanitizeCallsignInput(CallsignInput->GetText().ToString());
		if (CurrentCallsign.Len() < 3)
		{
			CurrentCallsign.AppendChar(UppercaseCharacter);
			CallsignInput->SetText(FText::FromString(CurrentCallsign));
			SetCallsignErrorMessage(FText::GetEmpty());
			TryAutoSubmitCallsign();
		}
		else
		{
			SetCallsignErrorMessage(
				FText::FromString(TEXT("영문 3자까지만 입력할 수 있습니다.")));
		}
	}
	else
	{
		SetCallsignErrorMessage(
			FText::FromString(TEXT("영문만 입력할 수 있습니다.")));
	}

	return FReply::Handled();
}

void URaidLobbyWidget::SetCallsignErrorMessage(const FText& Message)
{
	if (CallsignErrorText)
	{
		CallsignErrorText->SetText(Message);
		CallsignErrorText->SetVisibility(
			Message.IsEmpty()
				? ESlateVisibility::Hidden
				: ESlateVisibility::HitTestInvisible);
	}
}

#if WITH_DEV_AUTOMATION_TESTS
void URaidLobbyWidget::SetRaidSubsystemForTest(URaidSessionSubsystem* InSubsystem)
{
	RaidSubsystem = InSubsystem;
}

void URaidLobbyWidget::SetCallsignInputForTest(UEditableTextBox* InInput)
{
	CallsignInput = InInput;
	BindCallsignInput();
}

void URaidLobbyWidget::SetCallsignErrorTextForTest(UTextBlock* InErrorText)
{
	CallsignErrorText = InErrorText;
}

void URaidLobbyWidget::HandleCallsignKeyCharForTest(
	const FCharacterEvent& CharacterEvent)
{
	HandleCallsignKeyChar(CharacterEvent);
}

bool URaidLobbyWidget::IsCallsignAutoSubmitPendingForTest() const
{
	return !PendingCallsign.IsEmpty();
}

void URaidLobbyWidget::CompleteCallsignAutoSubmitDelayForTest()
{
	HandleCallsignAutoSubmitDelayElapsed();
}
#endif
