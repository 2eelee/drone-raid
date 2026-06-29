#include "RaidSessionSubsystem.h"

#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "LocalAssignment.h"
#include "RaidLobbyWidget.h"

namespace
{
const TCHAR* ToRaidAssignmentResultText(ERaidAssignmentResultType Result)
{
	switch (Result)
	{
	case ERaidAssignmentResultType::Success:
		return TEXT("Success");
	case ERaidAssignmentResultType::Waiting:
		return TEXT("Waiting");
	case ERaidAssignmentResultType::Failed:
		return TEXT("Failed");
	case ERaidAssignmentResultType::Canceled:
		return TEXT("Canceled");
	default:
		return TEXT("Unknown");
	}
}

const TCHAR* ToRaidFailReasonText(ERaidEntryFailReason Reason)
{
	switch (Reason)
	{
	case ERaidEntryFailReason::None:
		return TEXT("None");
	case ERaidEntryFailReason::ServerListFailed:
		return TEXT("ServerListFailed");
	case ERaidEntryFailReason::NoServerAvailable:
		return TEXT("NoServerAvailable");
	case ERaidEntryFailReason::MapLoadFailed:
		return TEXT("MapLoadFailed");
	case ERaidEntryFailReason::SpawnFailed:
		return TEXT("SpawnFailed");
	case ERaidEntryFailReason::Cancelled:
		return TEXT("Cancelled");
	default:
		return TEXT("Unknown");
	}
}
}

void URaidSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Assignment = NewObject<ULocalAssignment>(this);
}

void URaidSessionSubsystem::SetActiveLobbyWidget(URaidLobbyWidget* InWidget)
{
	ActiveLobbyWidget = InWidget;
}

void URaidSessionSubsystem::ClearActiveLobbyWidget(URaidLobbyWidget* InWidget)
{
	if (ActiveLobbyWidget == InWidget)
	{
		ActiveLobbyWidget = nullptr;
	}
}

void URaidSessionSubsystem::RequestRaidEntry(const FString& SlotId)
{
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] RaidEntryRequest Slot=%s AuthorityModel=LocalPrototype"), *SlotId);

	StopMatchmakingRetry();
	PendingRaidEntrySlotId = SlotId;
	EvaluateRaidEntry(false);
}

bool URaidSessionSubsystem::IsSlotEnabled(const FString& SlotId) const
{
	if (const ULocalAssignment* LocalAssignment = Cast<ULocalAssignment>(Assignment))
	{
		return LocalAssignment->IsSlotEnabled(SlotId);
	}

	return false;
}

void URaidSessionSubsystem::EvaluateRaidEntry(bool bIsRetry)
{
	if (bIsRetry)
	{
		const double ElapsedSeconds = GetMatchmakingNowSeconds() - MatchmakingWaitStartTimeSeconds;
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] RaidEntryRetry Slot=%s ElapsedSeconds=%.2f TimeoutSeconds=%.2f"),
			*PendingRaidEntrySlotId,
			ElapsedSeconds,
			MatchmakingTimeoutSeconds);

		if (ElapsedSeconds > MatchmakingTimeoutSeconds)
		{
			const FRaidAssignmentResult TimeoutResult = FRaidAssignmentResult::Failed(
				ERaidEntryFailReason::NoServerAvailable,
				TEXT("MatchmakingTimeout"));
			RecordAssignmentResult(TimeoutResult);
			StopMatchmakingRetry();
			HandleRaidEntryFailure(TimeoutResult);
			return;
		}
	}

	if (!Assignment)
	{
		const FRaidAssignmentResult MissingAssignmentResult = FRaidAssignmentResult::Failed(
			ERaidEntryFailReason::ServerListFailed,
			TEXT("MissingAssignment"));
		RecordAssignmentResult(MissingAssignmentResult);
		HandleRaidEntryFailure(MissingAssignmentResult);
		return;
	}

	const FRaidAssignmentResult Result = Assignment->ResolveRaidAssignment(PendingRaidEntrySlotId);
	RecordAssignmentResult(Result);

	switch (Result.Result)
	{
	case ERaidAssignmentResultType::Success:
		StopMatchmakingRetry();
		HideEntryPopups();
		if (ActiveLobbyWidget)
		{
			ActiveLobbyWidget->ShowLoading();
		}
		TravelToRaidEndpoint(Result);
		break;

	case ERaidAssignmentResultType::Waiting:
		StartMatchmakingWait();
		break;

	case ERaidAssignmentResultType::Failed:
		StopMatchmakingRetry();
		HandleRaidEntryFailure(Result);
		break;

	case ERaidAssignmentResultType::Canceled:
		StopMatchmakingRetry();
		HideEntryPopups();
		if (ActiveLobbyWidget)
		{
			ActiveLobbyWidget->ShowMainLobby();
		}
		break;

	default:
		break;
	}
}

void URaidSessionSubsystem::StartMatchmakingWait()
{
	if (bMatchmakingRetryActive)
	{
		return;
	}

	bMatchmakingRetryActive = true;
	MatchmakingWaitStartTimeSeconds = GetMatchmakingNowSeconds();

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] RaidEntryWait Slot=%s TimeoutSeconds=%.2f RetryIntervalSeconds=%.2f"),
		*PendingRaidEntrySlotId,
		MatchmakingTimeoutSeconds,
		MatchmakingRetryIntervalSeconds);

	ShowMatchmakingWait();

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UWorld* World = GameInstance->GetWorld())
		{
			World->GetTimerManager().SetTimer(
				MatchmakingRetryTimerHandle,
				this,
				&URaidSessionSubsystem::HandleMatchmakingRetry,
				MatchmakingRetryIntervalSeconds,
				true);
		}
	}
}

void URaidSessionSubsystem::StopMatchmakingRetry()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UWorld* World = GameInstance->GetWorld())
		{
			World->GetTimerManager().ClearTimer(MatchmakingRetryTimerHandle);
		}
	}

	bMatchmakingRetryActive = false;
}

void URaidSessionSubsystem::HandleMatchmakingRetry()
{
	EvaluateRaidEntry(true);
}

void URaidSessionSubsystem::HandleRaidEntryFailure(const FRaidAssignmentResult& Result)
{
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] RaidEntryFail Slot=%s FailReason=%s DebugReason=%s"),
		*PendingRaidEntrySlotId,
		ToRaidFailReasonText(Result.FailReason),
		*Result.DebugReason);

	if (Result.FailReason == ERaidEntryFailReason::MapLoadFailed)
	{
		ShowLoadFailed();
	}
	else
	{
		ShowNoServer();
	}
}

void URaidSessionSubsystem::TravelToRaidEndpoint(const FRaidAssignmentResult& Result)
{
	if (Result.Result != ERaidAssignmentResultType::Success)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] RaidEntryTravel Slot=%s Target=%s bIsLevelName=%d"),
		*Result.Endpoint.SlotId,
		*Result.Endpoint.TravelTarget,
		Result.Endpoint.bIsLevelName ? 1 : 0);

#if WITH_DEV_AUTOMATION_TESTS
	bTravelRequestedForTest = true;
	++TravelRequestCountForTest;
	if (bSuppressTravelForTest)
	{
		return;
	}
#endif

	UGameInstance* GameInstance = GetGameInstance();
	UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	if (!World)
	{
		const FRaidAssignmentResult LoadFailedResult = FRaidAssignmentResult::Failed(
			ERaidEntryFailReason::MapLoadFailed,
			TEXT("MissingWorld"),
			Result.Endpoint);
		RecordAssignmentResult(LoadFailedResult);
		HandleRaidEntryFailure(LoadFailedResult);
		return;
	}

	if (Result.Endpoint.bIsLevelName)
	{
		UGameplayStatics::OpenLevel(World, FName(*Result.Endpoint.TravelTarget));
	}
	else if (APlayerController* PC = World->GetFirstPlayerController())
	{
		PC->ClientTravel(Result.Endpoint.TravelTarget, TRAVEL_Absolute);
	}
	else
	{
		const FRaidAssignmentResult LoadFailedResult = FRaidAssignmentResult::Failed(
			ERaidEntryFailReason::MapLoadFailed,
			TEXT("MissingPlayerController"),
			Result.Endpoint);
		RecordAssignmentResult(LoadFailedResult);
		HandleRaidEntryFailure(LoadFailedResult);
	}
}

void URaidSessionSubsystem::RecordAssignmentResult(const FRaidAssignmentResult& Result)
{
	LastAssignmentResult = Result;

	const FString SelectedSlot = Result.SelectedSlotId.ToString();
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] RaidAssignmentResult Result=%s Slot=%s FailReason=%s AuthorityModel=LocalPrototype DebugReason=%s"),
		ToRaidAssignmentResultText(Result.Result),
		*SelectedSlot,
		ToRaidFailReasonText(Result.FailReason),
		*Result.DebugReason);
}

double URaidSessionSubsystem::GetMatchmakingNowSeconds() const
{
	return FPlatformTime::Seconds();
}

UUserWidget* URaidSessionSubsystem::CreateAndShowPopup(TSubclassOf<UUserWidget> WidgetClass)
{
	if (!WidgetClass)
	{
		return nullptr;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		return nullptr;
	}

	UUserWidget* Widget = CreateWidget<UUserWidget>(PC, WidgetClass);
	if (Widget)
	{
		Widget->AddToViewport();
	}
	return Widget;
}

void URaidSessionSubsystem::ShowMatchmakingWait()
{
	HideEntryPopups();
	if (ActiveLobbyWidget)
	{
		ActiveLobbyWidget->ShowWaitingPopup();
		return;
	}

	ActiveMatchmakingWidget = CreateAndShowPopup(MatchmakingWaitWidgetClass);
}

void URaidSessionSubsystem::ShowNoServer()
{
	HideEntryPopups();
	if (ActiveLobbyWidget)
	{
		ActiveLobbyWidget->ShowNoServerPopup();
		return;
	}

	ActiveNoServerWidget = CreateAndShowPopup(NoServerWidgetClass);
}

void URaidSessionSubsystem::ShowLoadFailed()
{
	HideEntryPopups();
	if (ActiveLobbyWidget)
	{
		ActiveLobbyWidget->ShowNoServerPopup();
		return;
	}

	ActiveLoadFailedWidget = CreateAndShowPopup(LoadFailedWidgetClass);
}

void URaidSessionSubsystem::HideEntryPopups()
{
	if (ActiveMatchmakingWidget)
	{
		ActiveMatchmakingWidget->RemoveFromParent();
		ActiveMatchmakingWidget = nullptr;
	}

	if (ActiveNoServerWidget)
	{
		ActiveNoServerWidget->RemoveFromParent();
		ActiveNoServerWidget = nullptr;
	}

	if (ActiveLoadFailedWidget)
	{
		ActiveLoadFailedWidget->RemoveFromParent();
		ActiveLoadFailedWidget = nullptr;
	}
}

void URaidSessionSubsystem::CancelMatchmaking()
{
	StopMatchmakingRetry();
	LastAssignmentResult = FRaidAssignmentResult::Canceled(TEXT("CancelMatchmaking"));

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] RaidEntryCancel Slot=%s"),
		*PendingRaidEntrySlotId);

	HideEntryPopups();
	if (ActiveLobbyWidget)
	{
		ActiveLobbyWidget->ShowMainLobby();
	}
}

#if WITH_DEV_AUTOMATION_TESTS
void URaidSessionSubsystem::SetAssignmentForTest(URaidAssignmentBase* InAssignment)
{
	Assignment = InAssignment;
}

void URaidSessionSubsystem::SetSuppressTravelForTest(bool bInSuppressTravel)
{
	bSuppressTravelForTest = bInSuppressTravel;
}

void URaidSessionSubsystem::ResetTravelRequestedForTest()
{
	bTravelRequestedForTest = false;
	TravelRequestCountForTest = 0;
}

void URaidSessionSubsystem::RetryRaidEntryForTest()
{
	HandleMatchmakingRetry();
}

void URaidSessionSubsystem::ExpireMatchmakingWaitForTest()
{
	const FRaidAssignmentResult TimeoutResult = FRaidAssignmentResult::Failed(
		ERaidEntryFailReason::NoServerAvailable,
		TEXT("MatchmakingTimeout"));
	RecordAssignmentResult(TimeoutResult);
	StopMatchmakingRetry();
	HandleRaidEntryFailure(TimeoutResult);
}
#endif
