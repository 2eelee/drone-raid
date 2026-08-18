#include "RaidSessionSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "LocalAssignment.h"
#include "RaidLobbyWidget.h"
#include "RemoteRaidAssignment.h"
#include "UObject/UObjectGlobals.h"

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

	URemoteRaidAssignment* RemoteAssignment = NewObject<URemoteRaidAssignment>(this);
	RemoteAssignment->InitializeFromSettings();
	Assignment = RemoteAssignment;
	LoadLocalProfile();

	PostLoadMapDelegateHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
		this,
		&URaidSessionSubsystem::HandlePostLoadMap);
	if (GEngine)
	{
		TravelFailureDelegateHandle = GEngine->OnTravelFailure().AddWeakLambda(
			this,
			[this](UWorld* FailedWorld, ETravelFailure::Type FailureType, const FString& ErrorString)
			{
				if (bRaidLoadWatchdogActive
					&& (!FailedWorld || FailedWorld == RaidLoadSourceWorld.Get() || FailedWorld->GetGameInstance() == GetGameInstance()))
				{
					HandlePendingRaidLoadFailure(FString::Printf(
						TEXT("TravelFailure:%s:%s"),
						ETravelFailure::ToString(FailureType),
						*ErrorString));
				}
			});
		NetworkFailureDelegateHandle = GEngine->OnNetworkFailure().AddWeakLambda(
			this,
			[this](UWorld* FailedWorld, UNetDriver*, ENetworkFailure::Type FailureType, const FString& ErrorString)
			{
				if (bRaidLoadWatchdogActive
					&& (!FailedWorld || FailedWorld == RaidLoadSourceWorld.Get() || FailedWorld->GetGameInstance() == GetGameInstance()))
				{
					HandlePendingRaidLoadFailure(FString::Printf(
						TEXT("NetworkFailure:%s:%s"),
						ENetworkFailure::ToString(FailureType),
						*ErrorString));
				}
			});
	}
}

void URaidSessionSubsystem::Deinitialize()
{
	StopMatchmakingRetry();
	StopRaidLoadWatchdog();
	if (PostLoadMapDelegateHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapDelegateHandle);
		PostLoadMapDelegateHandle.Reset();
	}
	if (GEngine)
	{
		if (TravelFailureDelegateHandle.IsValid())
		{
			GEngine->OnTravelFailure().Remove(TravelFailureDelegateHandle);
			TravelFailureDelegateHandle.Reset();
		}
		if (NetworkFailureDelegateHandle.IsValid())
		{
			GEngine->OnNetworkFailure().Remove(NetworkFailureDelegateHandle);
			NetworkFailureDelegateHandle.Reset();
		}
	}

	Super::Deinitialize();
}

bool URaidSessionSubsystem::TryLoginWithCallsign(const FString& RawCallsign)
{
	FString NormalizedCallsign;
	if (!TryNormalizeCallsign(RawCallsign, NormalizedCallsign))
	{
		return false;
	}

	Callsign = MoveTemp(NormalizedCallsign);
	const bool bSaved = SaveLocalProfile();
	bCallsignIdentified = bSaved;
	return bSaved;
}

bool URaidSessionSubsystem::TryLoginWithCallsignAndTravel(const FString& RawCallsign)
{
	if (!TryLoginWithCallsign(RawCallsign))
	{
		return false;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}

	UGameplayStatics::OpenLevel(
		World,
		GetPostLoginMapName(),
		true,
		GetPostLoginTravelOptions());
	return true;
}

FName URaidSessionSubsystem::GetPostLoginMapName() const
{
	return bHasCompletedTutorial ? FName(TEXT("LobbyMap")) : FName(TEXT("TestMap"));
}

FString URaidSessionSubsystem::GetPostLoginTravelOptions() const
{
	return bHasCompletedTutorial
		? FString()
		: TEXT("game=/Script/DroneProto.TutorialGameMode");
}

bool URaidSessionSubsystem::MarkTutorialCompleted()
{
	if (bHasCompletedTutorial)
	{
		return true;
	}

	bHasCompletedTutorial = true;
	return SaveLocalProfile();
}

#if WITH_DEV_AUTOMATION_TESTS
bool URaidSessionSubsystem::ReloadLocalProfileForTest()
{
	return LoadLocalProfile();
}
#endif

bool URaidSessionSubsystem::LoadLocalProfile()
{
	Callsign = TEXT("AAA");
	bHasCompletedTutorial = false;

	if (!UGameplayStatics::DoesSaveGameExist(ProfileSaveSlotName, ProfileSaveUserIndex))
	{
		return false;
	}

	const UDroneLocalProfileSaveGame* Profile = Cast<UDroneLocalProfileSaveGame>(
		UGameplayStatics::LoadGameFromSlot(ProfileSaveSlotName, ProfileSaveUserIndex));
	if (!Profile)
	{
		return false;
	}

	FString NormalizedCallsign;
	if (TryNormalizeCallsign(Profile->Callsign, NormalizedCallsign))
	{
		Callsign = MoveTemp(NormalizedCallsign);
	}
	bHasCompletedTutorial = Profile->bHasCompletedTutorial;
	return true;
}

bool URaidSessionSubsystem::SaveLocalProfile() const
{
	UDroneLocalProfileSaveGame* Profile = Cast<UDroneLocalProfileSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UDroneLocalProfileSaveGame::StaticClass()));
	if (!Profile)
	{
		return false;
	}

	Profile->Callsign = Callsign;
	Profile->bHasCompletedTutorial = bHasCompletedTutorial;
	return UGameplayStatics::SaveGameToSlot(Profile, ProfileSaveSlotName, ProfileSaveUserIndex);
}

bool URaidSessionSubsystem::TryNormalizeCallsign(const FString& RawCallsign, FString& OutCallsign)
{
	if (RawCallsign.Len() != 3)
	{
		return false;
	}

	OutCallsign.Reset(3);
	for (const TCHAR Character : RawCallsign)
	{
		if ((Character < TEXT('A') || Character > TEXT('Z'))
			&& (Character < TEXT('a') || Character > TEXT('z')))
		{
			OutCallsign.Reset();
			return false;
		}

		OutCallsign.AppendChar(FChar::ToUpper(Character));
	}

	return true;
}

void URaidSessionSubsystem::SetActiveLobbyWidget(URaidLobbyWidget* InWidget)
{
	ActiveLobbyWidget = InWidget;
	if (ActiveLobbyWidget && bPendingLoadFailedPopupAfterLobbyReturn)
	{
		bPendingLoadFailedPopupAfterLobbyReturn = false;
		ShowLoadFailed();
	}
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
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] RaidEntryRequest Slot=%s AuthorityModel=DedicatedReservation"), *SlotId);

	StopMatchmakingRetry();
	StopRaidLoadWatchdog();
	bRaidLoadFailureHandled = false;
	bPendingLoadFailedPopupAfterLobbyReturn = false;
	bLobbyReturnRequestedForLoadFailure = false;
	++AssignmentRequestGeneration;
	bAssignmentRequestInFlight = false;
	PendingRaidEntrySlotId = SlotId;
	EvaluateRaidEntry(false);
}

bool URaidSessionSubsystem::IsSlotEnabled(const FString& SlotId) const
{
	return Assignment && Assignment->IsSlotEnabled(SlotId);
}

void URaidSessionSubsystem::EvaluateRaidEntry(bool bIsRetry)
{
	if (bAssignmentRequestInFlight)
	{
		return;
	}

	double RemainingSeconds = MatchmakingTimeoutSeconds;
	if (bIsRetry)
	{
		const double ElapsedSeconds = GetMatchmakingNowSeconds() - MatchmakingWaitStartTimeSeconds;
		RemainingSeconds = MatchmakingTimeoutSeconds - ElapsedSeconds;
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

	bAssignmentRequestInFlight = true;
	const uint64 RequestGeneration = ++AssignmentRequestGeneration;
	Assignment->ResolveRaidAssignmentAsync(
		PendingRaidEntrySlotId,
		RemainingSeconds,
		FRaidAssignmentComplete::CreateUObject(this, &URaidSessionSubsystem::HandleAssignmentResolved, RequestGeneration));
}

void URaidSessionSubsystem::HandleAssignmentResolved(const FRaidAssignmentResult& Result, uint64 RequestGeneration)
{
	if (RequestGeneration != AssignmentRequestGeneration)
	{
		return;
	}
	bAssignmentRequestInFlight = false;
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

	FString TravelTarget = Result.Endpoint.TravelTarget;
	if (!Result.Endpoint.bIsLevelName)
	{
		if (Result.ReservationToken.IsEmpty())
		{
			const FRaidAssignmentResult MissingTokenResult = FRaidAssignmentResult::Failed(
				ERaidEntryFailReason::MapLoadFailed,
				TEXT("MissingReservationToken"),
				Result.Endpoint,
				Result.Availability);
			RecordAssignmentResult(MissingTokenResult);
			HandleRaidEntryFailure(MissingTokenResult);
			return;
		}
		TravelTarget = FString::Printf(TEXT("%s?RaidSlot=%s?RaidReservation=%s"),
			*Result.Endpoint.TravelTarget,
			*Result.Endpoint.SlotId,
			*Result.ReservationToken);
	}

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] RaidEntryTravel Slot=%s Target=%s bIsLevelName=%d"),
		*Result.Endpoint.SlotId,
		*Result.Endpoint.TravelTarget,
		Result.Endpoint.bIsLevelName ? 1 : 0);

#if WITH_DEV_AUTOMATION_TESTS
	bTravelRequestedForTest = true;
	++TravelRequestCountForTest;
	LastTravelTargetForTest = TravelTarget;
	if (bSuppressTravelForTest)
	{
		StartRaidLoadWatchdog(nullptr, Result.Endpoint);
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
		StartRaidLoadWatchdog(World, Result.Endpoint);
		UGameplayStatics::OpenLevel(World, FName(*Result.Endpoint.TravelTarget));
	}
	else if (APlayerController* PC = World->GetFirstPlayerController())
	{
		StartRaidLoadWatchdog(World, Result.Endpoint);
		PC->ClientTravel(TravelTarget, TRAVEL_Absolute);
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

void URaidSessionSubsystem::StartRaidLoadWatchdog(UWorld* SourceWorld, const FServerEndpoint& Endpoint)
{
	StopRaidLoadWatchdog();
	RaidLoadSourceWorld = SourceWorld;
	PendingRaidLoadEndpoint = Endpoint;
	RaidLoadWatchdogStartTimeSeconds = FPlatformTime::Seconds();
	bRaidLoadWatchdogActive = true;
	bRaidLoadFailureHandled = false;
	bPendingLoadFailedPopupAfterLobbyReturn = false;
	bLobbyReturnRequestedForLoadFailure = false;
	RaidLoadWatchdogTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &URaidSessionSubsystem::HandleRaidLoadWatchdogTick),
		RaidLoadWatchdogTickIntervalSeconds);

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] RaidLoadWatchdog Started Slot=%s Target=%s TimeoutSeconds=%.2f"),
		*Endpoint.SlotId,
		*Endpoint.TravelTarget,
		RaidLoadTimeoutSeconds);
}

void URaidSessionSubsystem::StopRaidLoadWatchdog()
{
	if (RaidLoadWatchdogTickerHandle.IsValid())
	{
		FTSTicker::RemoveTicker(RaidLoadWatchdogTickerHandle);
		RaidLoadWatchdogTickerHandle.Reset();
	}

	bRaidLoadWatchdogActive = false;
	RaidLoadSourceWorld.Reset();
}

bool URaidSessionSubsystem::HandleRaidLoadWatchdogTick(float DeltaSeconds)
{
	(void)DeltaSeconds;
	if (!bRaidLoadWatchdogActive)
	{
		RaidLoadWatchdogTickerHandle.Reset();
		return false;
	}

	const double ElapsedSeconds = FPlatformTime::Seconds() - RaidLoadWatchdogStartTimeSeconds;
	if (ElapsedSeconds < RaidLoadTimeoutSeconds)
	{
		return true;
	}

	RaidLoadWatchdogTickerHandle.Reset();
	HandlePendingRaidLoadFailure(TEXT("MapLoadTimeout"));
	return false;
}

void URaidSessionSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (bPendingLoadFailedPopupAfterLobbyReturn
		&& IsLobbyWorld(LoadedWorld)
		&& ActiveLobbyWidget
		&& ActiveLobbyWidget->GetWorld() == LoadedWorld)
	{
		bPendingLoadFailedPopupAfterLobbyReturn = false;
		ShowLoadFailed();
	}

	if (!bRaidLoadWatchdogActive
		|| !LoadedWorld
		|| LoadedWorld == RaidLoadSourceWorld.Get()
		|| LoadedWorld->GetGameInstance() != GetGameInstance())
	{
		return;
	}

	const double ElapsedSeconds = FPlatformTime::Seconds() - RaidLoadWatchdogStartTimeSeconds;
	if (ElapsedSeconds > RaidLoadTimeoutSeconds)
	{
		HandlePendingRaidLoadFailure(TEXT("MapLoadTimeoutAfterPostLoad"));
		return;
	}

	CompletePendingRaidLoad();
}

void URaidSessionSubsystem::CompletePendingRaidLoad()
{
	if (!bRaidLoadWatchdogActive)
	{
		return;
	}

	const FServerEndpoint CompletedEndpoint = PendingRaidLoadEndpoint;
	const double ElapsedSeconds = FPlatformTime::Seconds() - RaidLoadWatchdogStartTimeSeconds;
	StopRaidLoadWatchdog();
	PendingRaidLoadEndpoint = FServerEndpoint{};

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] RaidLoadWatchdog Completed Slot=%s Target=%s ElapsedSeconds=%.2f"),
		*CompletedEndpoint.SlotId,
		*CompletedEndpoint.TravelTarget,
		ElapsedSeconds);
}

void URaidSessionSubsystem::HandlePendingRaidLoadFailure(const FString& DebugReason)
{
	if (!bRaidLoadWatchdogActive || bRaidLoadFailureHandled)
	{
		return;
	}

	const FServerEndpoint FailedEndpoint = PendingRaidLoadEndpoint;
	bRaidLoadFailureHandled = true;
	StopRaidLoadWatchdog();
	PendingRaidLoadEndpoint = FServerEndpoint{};

	const FRaidAssignmentResult LoadFailedResult = FRaidAssignmentResult::Failed(
		ERaidEntryFailReason::MapLoadFailed,
		DebugReason,
		FailedEndpoint);
	RecordAssignmentResult(LoadFailedResult);
#if WITH_DEV_AUTOMATION_TESTS
	++RaidLoadFailureHandleCountForTest;
#endif

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] RaidLoadWatchdog Failed Slot=%s Target=%s DebugReason=%s"),
		*FailedEndpoint.SlotId,
		*FailedEndpoint.TravelTarget,
		*DebugReason);
	PresentPendingRaidLoadFailure();
}

void URaidSessionSubsystem::PresentPendingRaidLoadFailure()
{
	UGameInstance* GameInstance = GetGameInstance();
	UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	if ((!World || IsLobbyWorld(World)) && ActiveLobbyWidget)
	{
		bPendingLoadFailedPopupAfterLobbyReturn = false;
		ShowLoadFailed();
		return;
	}

	bPendingLoadFailedPopupAfterLobbyReturn = true;
	if (!World || bLobbyReturnRequestedForLoadFailure)
	{
		return;
	}

	bLobbyReturnRequestedForLoadFailure = true;
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] RaidLoadFailed ReturnToLobby Target=LobbyMap"));
	UGameplayStatics::OpenLevel(World, FName(TEXT("LobbyMap")));
}

bool URaidSessionSubsystem::IsLobbyWorld(const UWorld* World) const
{
	return World && World->GetMapName().EndsWith(TEXT("LobbyMap"));
}

void URaidSessionSubsystem::RecordAssignmentResult(const FRaidAssignmentResult& Result)
{
	LastAssignmentResult = Result;

	const FString SelectedSlot = Result.SelectedSlotId.ToString();
	const TCHAR* AuthorityModel = Assignment && Assignment->IsA<URemoteRaidAssignment>()
		? TEXT("DedicatedReservation")
		: TEXT("LocalPrototype");
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] RaidAssignmentResult Result=%s Slot=%s ServerState=%s AuthorityModel=%s SelectedSlot=%s FailReason=%s DebugReason=%s"),
		ToRaidAssignmentResultText(Result.Result),
		*SelectedSlot,
		ToRaidServerStateText(Result.Availability.ServerState),
		AuthorityModel,
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
	++AssignmentRequestGeneration;
	bAssignmentRequestInFlight = false;
	StopMatchmakingRetry();
	StopRaidLoadWatchdog();
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
	LastTravelTargetForTest.Reset();
}

void URaidSessionSubsystem::RetryRaidEntryForTest()
{
	HandleMatchmakingRetry();
}

void URaidSessionSubsystem::ExpireMatchmakingWaitForTest()
{
	++AssignmentRequestGeneration;
	bAssignmentRequestInFlight = false;
	const FRaidAssignmentResult TimeoutResult = FRaidAssignmentResult::Failed(
		ERaidEntryFailReason::NoServerAvailable,
		TEXT("MatchmakingTimeout"));
	RecordAssignmentResult(TimeoutResult);
	StopMatchmakingRetry();
	HandleRaidEntryFailure(TimeoutResult);
}

void URaidSessionSubsystem::CompleteRaidLoadForTest()
{
	CompletePendingRaidLoad();
}

void URaidSessionSubsystem::ExpireRaidLoadWatchdogForTest()
{
	HandlePendingRaidLoadFailure(TEXT("MapLoadTimeout"));
}

void URaidSessionSubsystem::NotifyRaidTravelFailureForTest()
{
	HandlePendingRaidLoadFailure(TEXT("TravelFailureForTest"));
}
#endif
