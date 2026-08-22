#include "RaidLobbyWidget.h"

#include "RaidSessionSubsystem.h"
#include "LobbyPlayerController.h"
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
		// 입장 요청을 실제로 보낸 경우만 확정음이다. 서브시스템이 없어 아무것도 못 보냈으면 실패다.
		PlayLobbyUISound(ELobbyUISound::Confirm);
		bRaidEntryRequestInFlight = true;
		RaidSubsystem->RequestRaidEntry(SlotId);
	}
	else
	{
		PlayLobbyUISound(ELobbyUISound::Error);
	}
}

void URaidLobbyWidget::PlayLobbyUISound(ELobbyUISound Sound) const
{
	ALobbyPlayerController* LobbyPC = Cast<ALobbyPlayerController>(GetOwningPlayer());
	if (!LobbyPC)
	{
		return;
	}

	switch (Sound)
	{
	case ELobbyUISound::Focus:
		LobbyPC->PlayUIFocusSound();
		break;
	case ELobbyUISound::Confirm:
		LobbyPC->PlayUIConfirmSound();
		break;
	case ELobbyUISound::Cancel:
		LobbyPC->PlayUICancelSound();
		break;
	case ELobbyUISound::Error:
		LobbyPC->PlayUIErrorSound();
		break;
	case ELobbyUISound::MatchSuccess:
		LobbyPC->PlayUIMatchSuccessSound();
		break;
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

	// 가이드 2절: 확정은 Confirm, 입력 실패는 Error.
	PlayLobbyUISound(bAccepted ? ELobbyUISound::Confirm : ELobbyUISound::Error);

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

	PlayLobbyUISound(ELobbyUISound::Cancel);

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
	PlayLobbyUISound(ELobbyUISound::Confirm);
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
	// 상태가 실제로 바뀐 경우에만 소리를 낸다. 같은 상태로 다시 설정하는 갱신 호출이 있다.
	// 첫 설정은 화면을 여는 초기화라 울리지 않는다.
	if (bHasInitializedLobbyStateAudio && CurrentUIState != NewState)
	{
		switch (NewState)
		{
		// 서버를 못 찾은 팝업. 가이드 2절이 "매칭/로드 실패 팝업"을 Error에 묶었다.
		case ERaidLobbyUIState::NoServer:
			PlayLobbyUISound(ELobbyUISound::Error);
			break;

		// 서버 배정이 끝나 실제로 들어가는 단계다. 가이드의 "서버/레이드 입장 성공"이 여기다.
		case ERaidLobbyUIState::Loading:
			PlayLobbyUISound(ELobbyUISound::MatchSuccess);
			break;

		default:
			break;
		}
	}
	bHasInitializedLobbyStateAudio = true;

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
