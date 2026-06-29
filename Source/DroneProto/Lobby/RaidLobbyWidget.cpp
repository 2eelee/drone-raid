#include "RaidLobbyWidget.h"

#include "RaidSessionSubsystem.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"

namespace
{
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
}

void URaidLobbyWidget::NativeConstruct()
{
	Super::NativeConstruct();

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

	ShowMainLobby();
}

void URaidLobbyWidget::NativeDestruct()
{
	if (RaidSubsystem)
	{
		RaidSubsystem->ClearActiveLobbyWidget(this);
	}

	Super::NativeDestruct();
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

bool URaidLobbyWidget::IsSlotEnabled(const FString& SlotId) const
{
	return RaidSubsystem ? RaidSubsystem->IsSlotEnabled(SlotId) : false;
}

void URaidLobbyWidget::ShowMainLobby()
{
	SetLobbyUIState(ERaidLobbyUIState::Main);
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

	SetOptionalWidgetVisibility(MainLobbyPanel, NewState == ERaidLobbyUIState::Main);
	SetOptionalWidgetVisibility(WaitingPopupPanel, NewState == ERaidLobbyUIState::Waiting);
	SetOptionalWidgetVisibility(NoServerPopupPanel, NewState == ERaidLobbyUIState::NoServer);
	SetOptionalWidgetVisibility(LoadingPanel, NewState == ERaidLobbyUIState::Loading);

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] LobbyUIState State=%s Widget=%s MainLobbyPanel=%d WaitingPopupPanel=%d NoServerPopupPanel=%d LoadingPanel=%d"),
		ToRaidLobbyUIStateText(NewState),
		*GetName(),
		MainLobbyPanel ? 1 : 0,
		WaitingPopupPanel ? 1 : 0,
		NoServerPopupPanel ? 1 : 0,
		LoadingPanel ? 1 : 0);
}

void URaidLobbyWidget::SetOptionalWidgetVisibility(UWidget* Widget, bool bShouldShow)
{
	if (Widget)
	{
		Widget->SetVisibility(bShouldShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

#if WITH_DEV_AUTOMATION_TESTS
void URaidLobbyWidget::SetRaidSubsystemForTest(URaidSessionSubsystem* InSubsystem)
{
	RaidSubsystem = InSubsystem;
}
#endif
