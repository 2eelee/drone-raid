#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "CoreGlobals.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "Drone.h"
#include "DronePart.h"
#include "Raid/DronePartInventory.h"
#include "Raid/DroneDataTableRows.h"
#include "Raid/DroneCombatTypes.h"
#include "Raid/DronePartReturnManager.h"
#include "Raid/BossHUDWidget.h"
#include "Raid/DroneReportWidget.h"
#include "Raid/RaidBoss.h"
#include "Raid/RaidBossAttackTelegraph.h"
#include "Raid/RaidGameMode.h"
#include "Raid/RaidGameState.h"
#include "Raid/RaidPlayerController.h"
#include "Lobby/LocalAssignment.h"
#include "Lobby/RaidLobbyWidget.h"
#include "Lobby/RaidSessionSubsystem.h"
#include "Lobby/ServerEndpoint.h"

#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/DefaultPawn.h"
#include "TimerManager.h"

namespace
{
struct FDroneSelectionTestContext
{
	UWorld* World = nullptr;
	ARaidGameState* GameState = nullptr;
	ADronePartInventory* Inventory = nullptr;
	ARaidPlayerController* PC = nullptr;
	ADrone* Drone = nullptr;
};

FDroneSelectionTestContext CreateDroneSelectionTestContext(const TCHAR* WorldName)
{
	FDroneSelectionTestContext Context;
	Context.World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
	if (!Context.World)
	{
		return Context;
	}

	Context.GameState = Context.World->SpawnActor<ARaidGameState>();
	Context.Inventory = Context.World->SpawnActor<ADronePartInventory>();
	Context.PC = Context.World->SpawnActor<ARaidPlayerController>();
	Context.Drone = Context.World->SpawnActor<ADrone>();

	if (Context.GameState)
	{
		Context.World->SetGameState(Context.GameState);
		if (Context.Inventory)
		{
			Context.GameState->SetDronePartInventory(Context.Inventory);
		}
	}

	if (Context.PC && Context.Drone)
	{
		Context.PC->Possess(Context.Drone);
	}

	return Context;
}

void DestroyDroneSelectionTestContext(FDroneSelectionTestContext& Context)
{
	if (Context.World)
	{
		Context.World->DestroyWorld(false);
	}
	Context = FDroneSelectionTestContext();
}

FDroneSelectionTestContext CreateDroneReturnTestContext(
	const TCHAR* WorldName,
	UDronePartReturnManager*& OutReturnManager)
{
	FDroneSelectionTestContext Context = CreateDroneSelectionTestContext(WorldName);
	OutReturnManager = NewObject<UDronePartReturnManager>();

	if (OutReturnManager && Context.Inventory)
	{
		OutReturnManager->Initialize(Context.Inventory);
	}

	if (Context.PC)
	{
		Context.PC->SetDronePartReturnManagerForTest(OutReturnManager);
	}

	return Context;
}

bool PrepareBattleAttackTest(
	FAutomationTestBase& Test,
	FDroneSelectionTestContext& Context,
	ARaidBoss*& OutBoss,
	const TCHAR* ContextLabel)
{
	Test.TestNotNull(FString::Printf(TEXT("%s world is created"), ContextLabel), Context.World);
	Test.TestNotNull(FString::Printf(TEXT("%s game state is spawned"), ContextLabel), Context.GameState);
	Test.TestNotNull(FString::Printf(TEXT("%s player controller is spawned"), ContextLabel), Context.PC);
	Test.TestNotNull(FString::Printf(TEXT("%s drone is spawned"), ContextLabel), Context.Drone);
	if (!Context.World || !Context.GameState || !Context.PC || !Context.Drone)
	{
		return false;
	}

	OutBoss = Context.World->SpawnActor<ARaidBoss>();
	Test.TestNotNull(FString::Printf(TEXT("%s boss is spawned"), ContextLabel), OutBoss);
	if (!OutBoss)
	{
		return false;
	}

	Context.GameState->SetRaidBossForServer(OutBoss);
	Context.PC->Server_RequestReadyForRaid_Implementation();
	Test.TestEqual(FString::Printf(TEXT("%s player is InBattle"), ContextLabel),
		Context.PC->GetCurrentSelectionState(),
		EPlayerSelectionState::InBattle);
	return true;
}

float AttackBossAndMeasureDamage(ADrone* Drone, const ARaidBoss* Boss)
{
	if (!Drone || !Boss)
	{
		return 0.0f;
	}

	const float HPBeforeAttack = Boss->GetCurrentHP();
	Drone->RequestAttackBoss();
	return HPBeforeAttack - Boss->GetCurrentHP();
}

bool SetFloatPropertyForAutomationTest(UObject* Object, FName PropertyName, float Value)
{
	if (!Object)
	{
		return false;
	}

	FFloatProperty* FloatProperty = FindFProperty<FFloatProperty>(Object->GetClass(), PropertyName);
	if (!FloatProperty)
	{
		return false;
	}

	FloatProperty->SetPropertyValue_InContainer(Object, Value);
	return true;
}

bool SetPawnControllerForAutomationTest(APawn* Pawn, AController* Controller)
{
	if (!Pawn)
	{
		return false;
	}

	FObjectProperty* ControllerProperty = FindFProperty<FObjectProperty>(APawn::StaticClass(), TEXT("Controller"));
	if (!ControllerProperty)
	{
		return false;
	}

	ControllerProperty->SetObjectPropertyValue_InContainer(Pawn, Controller);
	return true;
}

void TickWorldForAutomationTest(UWorld* World, float DurationSeconds)
{
	if (!World || DurationSeconds <= 0.0f)
	{
		return;
	}

	++GFrameCounter;
	World->GetTimerManager().Tick(0.0f);
	++GFrameCounter;
	World->GetTimerManager().Tick(DurationSeconds + 0.001f);
}

int32 CountBossTelegraphsForAutomationTest(UWorld* World)
{
	if (!World)
	{
		return 0;
	}

	int32 TelegraphCount = 0;
	for (TActorIterator<ARaidBossAttackTelegraph> It(World); It; ++It)
	{
		if (*It && !It->IsActorBeingDestroyed())
		{
			TelegraphCount++;
		}
	}
	return TelegraphCount;
}

ARaidBossAttackTelegraph* FindBossTelegraphForAutomationTest(UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<ARaidBossAttackTelegraph> It(World); It; ++It)
	{
		if (*It && !It->IsActorBeingDestroyed())
		{
			return *It;
		}
	}
	return nullptr;
}

FRaidServerCandidate MakeRaidServerCandidate(
	const TCHAR* SlotId,
	int32 CurrentPlayers,
	int32 MaxPlayers = 16,
	bool bIsOnline = true,
	bool bAcceptsPlayers = true,
	const TCHAR* TravelTarget = TEXT("TestMap"),
	ERaidServerState ServerState = ERaidServerState::Unknown)
{
	FRaidServerCandidate Candidate;
	Candidate.Endpoint.SlotId = SlotId;
	Candidate.Endpoint.TravelTarget = TravelTarget;
	Candidate.Endpoint.bIsLevelName = true;
	Candidate.CurrentPlayers = CurrentPlayers;
	Candidate.MaxPlayers = MaxPlayers;
	Candidate.bIsOnline = bIsOnline;
	Candidate.bAcceptsPlayers = bAcceptsPlayers;
	Candidate.Availability.SlotId = SlotId;
	Candidate.Availability.CurrentPlayers = CurrentPlayers;
	Candidate.Availability.MaxPlayers = MaxPlayers;
	Candidate.Availability.bAcceptsPlayers = bAcceptsPlayers;
	Candidate.Availability.ServerState = ServerState != ERaidServerState::Unknown
		? ServerState
		: (!bIsOnline
			? ERaidServerState::Offline
			: (!bAcceptsPlayers
				? ERaidServerState::Unavailable
				: (CurrentPlayers >= MaxPlayers ? ERaidServerState::Full : ERaidServerState::Online)));
	Candidate.Availability.DebugReason = FString::Printf(TEXT("TestAvailability Slot=%s"), SlotId);
	return Candidate;
}

}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRaidEntryLocalAssignmentDefaultsTest,
	"DroneProto.RaidEntry.Assignment.DefaultCandidates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRaidEntryLocalAssignmentDefaultsTest::RunTest(const FString& Parameters)
{
	ULocalAssignment* Assignment = NewObject<ULocalAssignment>();
	TestNotNull(TEXT("local assignment is created"), Assignment);
	if (!Assignment)
	{
		return false;
	}

	const TArray<FRaidServerCandidate>& Candidates = Assignment->GetCandidatesForTest();
	TestEqual(TEXT("default local assignment creates three candidates"), Candidates.Num(), 3);
	if (Candidates.Num() != 3)
	{
		return false;
	}

	TestEqual(TEXT("default candidate A slot"), Candidates[0].Endpoint.SlotId, FString(TEXT("A")));
	TestEqual(TEXT("default candidate B slot"), Candidates[1].Endpoint.SlotId, FString(TEXT("B")));
	TestEqual(TEXT("default candidate C slot"), Candidates[2].Endpoint.SlotId, FString(TEXT("C")));
	for (const FRaidServerCandidate& Candidate : Candidates)
	{
		TestEqual(TEXT("default max players is 16"), Candidate.MaxPlayers, 16);
		TestEqual(TEXT("default availability max players is 16"), Candidate.Availability.MaxPlayers, 16);
		TestEqual(TEXT("default travel target points at the current raid test map"), Candidate.Endpoint.TravelTarget, FString(TEXT("TestMap")));
		TestTrue(TEXT("default candidate is online"), Candidate.bIsOnline);
		TestEqual(TEXT("default availability state is online"), Candidate.Availability.ServerState, ERaidServerState::Online);
		TestTrue(TEXT("default candidate accepts players"), Candidate.bAcceptsPlayers);
		TestTrue(TEXT("default availability accepts players"), Candidate.Availability.bAcceptsPlayers);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRaidEntryLocalAssignmentPriorityTest,
	"DroneProto.RaidEntry.Assignment.PriorityAndFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRaidEntryLocalAssignmentPriorityTest::RunTest(const FString& Parameters)
{
	ULocalAssignment* Assignment = NewObject<ULocalAssignment>();
	TestNotNull(TEXT("local assignment is created"), Assignment);
	if (!Assignment)
	{
		return false;
	}

	auto ResolveWithCandidates = [Assignment](const TArray<FRaidServerCandidate>& Candidates)
	{
		Assignment->SetCandidatesForTest(Candidates);
		return Assignment->ResolveRaidAssignment(TEXT("A"));
	};

	FRaidAssignmentResult Result = ResolveWithCandidates({
		MakeRaidServerCandidate(TEXT("A"), 0),
		MakeRaidServerCandidate(TEXT("B"), 0),
		MakeRaidServerCandidate(TEXT("C"), 0),
	});
	TestEqual(TEXT("available A succeeds"), Result.Result, ERaidAssignmentResultType::Success);
	TestEqual(TEXT("available A is selected"), Result.SelectedSlotId, FName(TEXT("A")));
	TestEqual(TEXT("available A records selected server state"), Result.Availability.ServerState, ERaidServerState::Online);
	TestEqual(TEXT("successful assignment has neutral fail reason"), Result.FailReason, ERaidEntryFailReason::None);
	TestEqual(TEXT("successful assignment points at TestMap"), Result.Endpoint.TravelTarget, FString(TEXT("TestMap")));

	Result = ResolveWithCandidates({
		MakeRaidServerCandidate(TEXT("A"), 16),
		MakeRaidServerCandidate(TEXT("B"), 0),
		MakeRaidServerCandidate(TEXT("C"), 0),
	});
	TestEqual(TEXT("full A skips to B"), Result.Result, ERaidAssignmentResultType::Success);
	TestEqual(TEXT("B is selected when A is full"), Result.SelectedSlotId, FName(TEXT("B")));
	TestEqual(TEXT("B selection records online server state"), Result.Availability.ServerState, ERaidServerState::Online);
	TestEqual(TEXT("successful fallback assignment has neutral fail reason"), Result.FailReason, ERaidEntryFailReason::None);

	Result = ResolveWithCandidates({
		MakeRaidServerCandidate(TEXT("A"), 16),
		MakeRaidServerCandidate(TEXT("B"), 16),
		MakeRaidServerCandidate(TEXT("C"), 0),
	});
	TestEqual(TEXT("full A and B skip to C"), Result.Result, ERaidAssignmentResultType::Success);
	TestEqual(TEXT("C is selected when A and B are full"), Result.SelectedSlotId, FName(TEXT("C")));

	Result = ResolveWithCandidates({
		MakeRaidServerCandidate(TEXT("A"), 16),
		MakeRaidServerCandidate(TEXT("B"), 16),
		MakeRaidServerCandidate(TEXT("C"), 16),
	});
	TestEqual(TEXT("all full returns waiting"), Result.Result, ERaidAssignmentResultType::Waiting);
	TestEqual(TEXT("all full preserves no-server reason"), Result.FailReason, ERaidEntryFailReason::NoServerAvailable);
	TestTrue(TEXT("full slot remains enabled so request can enter waiting"), Assignment->IsSlotEnabled(TEXT("A")));

	Result = ResolveWithCandidates({
		MakeRaidServerCandidate(TEXT("A"), 0, 16, false),
		MakeRaidServerCandidate(TEXT("B"), 0, 16, true, false),
		MakeRaidServerCandidate(TEXT("C"), 0),
	});
	TestEqual(TEXT("offline and unavailable candidates are skipped"), Result.Result, ERaidAssignmentResultType::Success);
	TestEqual(TEXT("C is selected after offline and unavailable candidates"), Result.SelectedSlotId, FName(TEXT("C")));
	TestEqual(TEXT("C selection records online server state"), Result.Availability.ServerState, ERaidServerState::Online);
	TestFalse(TEXT("offline slot is disabled for direct entry"), Assignment->IsSlotEnabled(TEXT("A")));

	Result = ResolveWithCandidates({
		MakeRaidServerCandidate(TEXT("A"), 0, 16, false, true, TEXT("TestMap"), ERaidServerState::Offline),
		MakeRaidServerCandidate(TEXT("B"), 0, 16, true, false, TEXT("TestMap"), ERaidServerState::Unavailable),
		MakeRaidServerCandidate(TEXT("C"), 0, 16, true, true, TEXT("TestMap"), ERaidServerState::Error),
	});
	TestEqual(TEXT("all offline unavailable or error returns waiting"), Result.Result, ERaidAssignmentResultType::Waiting);
	TestEqual(TEXT("all unavailable preserves no-server reason"), Result.FailReason, ERaidEntryFailReason::NoServerAvailable);

	Result = ResolveWithCandidates({
		MakeRaidServerCandidate(TEXT("A"), 0, 16, true, true, TEXT("")),
		MakeRaidServerCandidate(TEXT("B"), 0),
		MakeRaidServerCandidate(TEXT("C"), 0),
	});
	TestEqual(TEXT("invalid travel target fails immediately"), Result.Result, ERaidAssignmentResultType::Failed);
	TestEqual(TEXT("invalid travel target preserves map-load fail reason"), Result.FailReason, ERaidEntryFailReason::MapLoadFailed);
	TestEqual(TEXT("invalid travel target records candidate server state"), Result.Availability.ServerState, ERaidServerState::Online);

	Assignment->SetCandidatesForTest({
		MakeRaidServerCandidate(TEXT("A"), 0),
		MakeRaidServerCandidate(TEXT("B"), 0),
		MakeRaidServerCandidate(TEXT("C"), 0),
	});
	const FServerEndpoint CompatEndpoint = Assignment->ResolveServer(TEXT("A"));
	TestEqual(TEXT("legacy ResolveServer wrapper returns selected endpoint"), CompatEndpoint.SlotId, FString(TEXT("A")));
	TestEqual(TEXT("legacy ResolveServer wrapper returns TestMap target"), CompatEndpoint.TravelTarget, FString(TEXT("TestMap")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRaidEntrySessionWaitRetryCancelTest,
	"DroneProto.RaidEntry.Session.WaitRetryTimeoutCancel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRaidEntrySessionWaitRetryCancelTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	URaidSessionSubsystem* Session = NewObject<URaidSessionSubsystem>(GameInstance);
	ULocalAssignment* Assignment = NewObject<ULocalAssignment>(Session);
	TestNotNull(TEXT("game instance test outer is created"), GameInstance);
	TestNotNull(TEXT("raid session subsystem is created"), Session);
	TestNotNull(TEXT("local assignment is created"), Assignment);
	if (!GameInstance || !Session || !Assignment)
	{
		return false;
	}

	Session->SetAssignmentForTest(Assignment);
	Session->SetSuppressTravelForTest(true);
	Assignment->SetCandidatesForTest({
		MakeRaidServerCandidate(TEXT("A"), 16),
		MakeRaidServerCandidate(TEXT("B"), 16),
		MakeRaidServerCandidate(TEXT("C"), 16),
	});

	Session->RequestRaidEntry(TEXT("A"));
	TestEqual(TEXT("all full enters waiting state"), Session->GetLastAssignmentResultForTest().Result, ERaidAssignmentResultType::Waiting);
	TestTrue(TEXT("waiting starts retry state"), Session->IsMatchmakingRetryActiveForTest());
	TestFalse(TEXT("waiting does not travel"), Session->WasTravelRequestedForTest());

	Assignment->SetCandidateCurrentPlayersForTest(TEXT("B"), 15);
	Session->RetryRaidEntryForTest();
	TestEqual(TEXT("retry succeeds when B opens"), Session->GetLastAssignmentResultForTest().Result, ERaidAssignmentResultType::Success);
	TestEqual(TEXT("retry selects opened B"), Session->GetLastAssignmentResultForTest().SelectedSlotId, FName(TEXT("B")));
	TestEqual(TEXT("retry success has neutral fail reason"), Session->GetLastAssignmentResultForTest().FailReason, ERaidEntryFailReason::None);
	TestTrue(TEXT("success requests travel"), Session->WasTravelRequestedForTest());
	TestFalse(TEXT("success stops retry state"), Session->IsMatchmakingRetryActiveForTest());

	Session->ResetTravelRequestedForTest();
	Assignment->SetCandidatesForTest({
		MakeRaidServerCandidate(TEXT("A"), 16),
		MakeRaidServerCandidate(TEXT("B"), 16),
		MakeRaidServerCandidate(TEXT("C"), 16),
	});
	Session->RequestRaidEntry(TEXT("A"));
	Session->ExpireMatchmakingWaitForTest();
	TestEqual(TEXT("timeout fails with no server"), Session->GetLastAssignmentResultForTest().Result, ERaidAssignmentResultType::Failed);
	TestEqual(TEXT("timeout preserves no-server reason"), Session->GetLastAssignmentResultForTest().FailReason, ERaidEntryFailReason::NoServerAvailable);
	TestFalse(TEXT("timeout does not travel"), Session->WasTravelRequestedForTest());

	Session->RequestRaidEntry(TEXT("A"));
	Session->CancelMatchmaking();
	TestEqual(TEXT("cancel marks canceled result"), Session->GetLastAssignmentResultForTest().Result, ERaidAssignmentResultType::Canceled);
	TestEqual(TEXT("cancel preserves cancel reason"), Session->GetLastAssignmentResultForTest().FailReason, ERaidEntryFailReason::Cancelled);
	TestFalse(TEXT("cancel stops retry state"), Session->IsMatchmakingRetryActiveForTest());
	TestFalse(TEXT("cancel does not request raid travel"), Session->WasTravelRequestedForTest());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRaidLobbyWidgetDebugStateTransitionsTest,
	"DroneProto.RaidEntry.LobbyWidget.DebugStateTransitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRaidLobbyWidgetDebugStateTransitionsTest::RunTest(const FString& Parameters)
{
	URaidLobbyWidget* Widget = NewObject<URaidLobbyWidget>();
	TestNotNull(TEXT("raid lobby widget is created"), Widget);
	if (!Widget)
	{
		return false;
	}

	TestEqual(TEXT("initial lobby UI state is main"), Widget->GetCurrentLobbyUIState(), ERaidLobbyUIState::Main);

	Widget->ShowDebugWaitingPopup();
	TestEqual(TEXT("debug waiting hook switches to waiting"), Widget->GetCurrentLobbyUIState(), ERaidLobbyUIState::Waiting);

	Widget->CancelMatchmakingFromLobby();
	TestEqual(TEXT("cancel returns waiting state to main"), Widget->GetCurrentLobbyUIState(), ERaidLobbyUIState::Main);

	Widget->ShowDebugNoServerPopup();
	TestEqual(TEXT("debug no-server hook switches to no-server"), Widget->GetCurrentLobbyUIState(), ERaidLobbyUIState::NoServer);

	Widget->ConfirmNoServerFromLobby();
	TestEqual(TEXT("confirm returns no-server state to main"), Widget->GetCurrentLobbyUIState(), ERaidLobbyUIState::Main);

	Widget->ShowLoading();
	TestEqual(TEXT("loading helper switches to loading"), Widget->GetCurrentLobbyUIState(), ERaidLobbyUIState::Loading);

	Widget->ResetDebugMainLobby();
	TestEqual(TEXT("debug reset returns loading state to main"), Widget->GetCurrentLobbyUIState(), ERaidLobbyUIState::Main);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRaidLobbyWidgetSessionStateTransitionsTest,
	"DroneProto.RaidEntry.LobbyWidget.SessionStateTransitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRaidLobbyWidgetSessionStateTransitionsTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	URaidSessionSubsystem* Session = NewObject<URaidSessionSubsystem>(GameInstance);
	ULocalAssignment* Assignment = NewObject<ULocalAssignment>(Session);
	URaidLobbyWidget* Widget = NewObject<URaidLobbyWidget>();
	TestNotNull(TEXT("game instance test outer is created"), GameInstance);
	TestNotNull(TEXT("raid session subsystem is created"), Session);
	TestNotNull(TEXT("local assignment is created"), Assignment);
	TestNotNull(TEXT("raid lobby widget is created"), Widget);
	if (!GameInstance || !Session || !Assignment || !Widget)
	{
		return false;
	}

	Session->SetAssignmentForTest(Assignment);
	Session->SetSuppressTravelForTest(true);
	Session->SetActiveLobbyWidget(Widget);
	Widget->SetRaidSubsystemForTest(Session);

	Assignment->SetCandidatesForTest({
		MakeRaidServerCandidate(TEXT("A"), 16),
		MakeRaidServerCandidate(TEXT("B"), 16),
		MakeRaidServerCandidate(TEXT("C"), 16),
	});
	Session->RequestRaidEntry(TEXT("A"));
	TestEqual(TEXT("all full session result switches lobby to waiting"), Widget->GetCurrentLobbyUIState(), ERaidLobbyUIState::Waiting);
	TestFalse(TEXT("waiting does not request travel"), Session->WasTravelRequestedForTest());

	Session->CancelMatchmaking();
	TestEqual(TEXT("cancel switches lobby back to main"), Widget->GetCurrentLobbyUIState(), ERaidLobbyUIState::Main);

	Session->ResetTravelRequestedForTest();
	Session->RequestRaidEntry(TEXT("A"));
	Session->ExpireMatchmakingWaitForTest();
	TestEqual(TEXT("timeout switches lobby to no-server"), Widget->GetCurrentLobbyUIState(), ERaidLobbyUIState::NoServer);
	TestFalse(TEXT("timeout does not request travel"), Session->WasTravelRequestedForTest());

	Widget->ConfirmNoServerFromLobby();
	TestEqual(TEXT("no-server confirm switches lobby back to main"), Widget->GetCurrentLobbyUIState(), ERaidLobbyUIState::Main);

	Session->ResetTravelRequestedForTest();
	Assignment->SetCandidatesForTest({
		MakeRaidServerCandidate(TEXT("A"), 0),
		MakeRaidServerCandidate(TEXT("B"), 0),
		MakeRaidServerCandidate(TEXT("C"), 0),
	});
	Widget->RequestEntry(TEXT("A"));
	TestEqual(TEXT("success switches lobby to loading before travel"), Widget->GetCurrentLobbyUIState(), ERaidLobbyUIState::Loading);
	TestTrue(TEXT("success requests travel once"), Session->WasTravelRequestedForTest());
	TestEqual(TEXT("success records one travel request"), Session->GetTravelRequestCountForTest(), 1);

	Widget->RequestEntry(TEXT("A"));
	TestEqual(TEXT("duplicate join while loading keeps lobby in loading"), Widget->GetCurrentLobbyUIState(), ERaidLobbyUIState::Loading);
	TestEqual(TEXT("duplicate join while loading does not request a second travel"), Session->GetTravelRequestCountForTest(), 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneCombatFormulaTest,
	"DroneProto.D9.DroneCombat.Formulas",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneCombatFormulaTest::RunTest(const FString& Parameters)
{
	FDroneWeaponCalculationInput PulseInput;
	PulseInput.WeaponType = EDroneCombatWeaponType::PulseLaser;
	PulseInput.PulseAttackCount = 2;
	const FDroneWeaponCalculationResult PulseResult = FDroneCombatRules::CalculateWeaponDamage(PulseInput);
	TestTrue(TEXT("third Pulse hit uses strong damage"),
		FMath::IsNearlyEqual(PulseResult.WeaponDamage, 18.0f, 0.01f));
	TestEqual(TEXT("third Pulse hit resets the slot-local counter"), PulseResult.PulseAttackCount, 0);

	FDroneWeaponCalculationInput FractureInput;
	FractureInput.WeaponType = EDroneCombatWeaponType::FractureBurst;
	const FDroneWeaponCalculationResult FractureResult = FDroneCombatRules::CalculateWeaponDamage(FractureInput);
	TestTrue(TEXT("Fracture Burst calculates 5 + 3 * 2 damage"),
		FMath::IsNearlyEqual(FractureResult.WeaponDamage, 11.0f, 0.01f));
	TestEqual(TEXT("Fracture Burst exposes four total hit events for reporting"), FractureResult.HitCount, 4);

	FDroneWeaponCalculationInput VectorInput;
	VectorInput.WeaponType = EDroneCombatWeaponType::VectorCannon;
	VectorInput.VectorAccumulatedMoveDistanceMeters = 43.0f;
	const FDroneWeaponCalculationResult VectorResult = FDroneCombatRules::CalculateWeaponDamage(VectorInput);
	TestTrue(TEXT("Vector Cannon caps movement bonus at 8"),
		FMath::IsNearlyEqual(VectorResult.WeaponDamage, 15.0f, 0.01f));
	TestTrue(TEXT("Vector Cannon requests attack-time vector distance reset"), VectorResult.bResetVectorDistance);

	FDroneCoreCalculationInput ZenithInput;
	ZenithInput.CoreType = EDroneCombatCoreType::Zenith;
	ZenithInput.CurrentHP = 50.0f;
	ZenithInput.MaxHP = 100.0f;
	const FDroneCoreCalculationResult ZenithResult = FDroneCombatRules::CalculateCoreBonus(ZenithInput);
	TestTrue(TEXT("Zenith base attack modifier stays 1.0"),
		FMath::IsNearlyEqual(ZenithResult.CoreAttackModifier, 1.0f, 0.01f));
	TestTrue(TEXT("Zenith base move speed modifier stays 1.0"),
		FMath::IsNearlyEqual(ZenithResult.CoreMoveSpeedModifier, 1.0f, 0.01f));
	TestTrue(TEXT("Zenith at 50 percent HP gives 1.10 bonus modifier"),
		FMath::IsNearlyEqual(ZenithResult.CoreBonusAttackModifier, 1.10f, 0.01f));

	FDroneCoreCalculationInput BoosterInput;
	BoosterInput.CoreType = EDroneCombatCoreType::Booster;
	BoosterInput.AccumulatedMoveDistanceMeters = 210.0f;
	const FDroneCoreCalculationResult BoosterResult = FDroneCombatRules::CalculateCoreBonus(BoosterInput);
	TestTrue(TEXT("Booster base attack modifier applies 0.95"),
		FMath::IsNearlyEqual(BoosterResult.CoreAttackModifier, 0.95f, 0.01f));
	TestTrue(TEXT("Booster base move speed modifier stays 1.0"),
		FMath::IsNearlyEqual(BoosterResult.CoreMoveSpeedModifier, 1.0f, 0.01f));
	TestTrue(TEXT("Booster speed bonus caps at 0.30"),
		FMath::IsNearlyEqual(BoosterResult.MoveSpeedBonus, 0.30f, 0.01f));
	TestTrue(TEXT("Booster attack bonus is half of speed bonus"),
		FMath::IsNearlyEqual(BoosterResult.CoreBonusAttackModifier, 1.15f, 0.01f));

	FDroneCoreCalculationInput DrainInput;
	DrainInput.CoreType = EDroneCombatCoreType::Drain;
	const FDroneCoreCalculationResult DrainResult = FDroneCombatRules::CalculateCoreBonus(DrainInput);
	TestTrue(TEXT("Drain base attack modifier applies 0.85"),
		FMath::IsNearlyEqual(DrainResult.CoreAttackModifier, 0.85f, 0.01f));
	TestTrue(TEXT("Drain base move speed modifier applies 0.9"),
		FMath::IsNearlyEqual(DrainResult.CoreMoveSpeedModifier, 0.9f, 0.01f));

	TestTrue(TEXT("Drain heal is 12 percent of dealt damage"),
		FMath::IsNearlyEqual(FDroneCombatRules::CalculateDrainHeal(11.0f), 1.32f, 0.01f));
	TestTrue(TEXT("Drain heal caps once per attack input"),
		FMath::IsNearlyEqual(FDroneCombatRules::CalculateDrainHeal(100.0f), 3.0f, 0.01f));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneReportFormulaTest,
	"DroneProto.D10.DroneReport.Formulas",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneCombatSpecAlignmentTest,
	"DroneProto.D13.DroneCombat.SpecAlignment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneCombatSpecAlignmentTest::RunTest(const FString& Parameters)
{
	const auto ExpectCore = [this](EDroneCombatCoreType CoreType, float CurrentHP, float MaxHP, float MoveMeters, float ExpectedCoreModifier, float ExpectedBonusModifier, float ExpectedMoveSpeedBonus, const TCHAR* Label)
	{
		FDroneCoreCalculationInput Input;
		Input.CoreType = CoreType;
		Input.CurrentHP = CurrentHP;
		Input.MaxHP = MaxHP;
		Input.AccumulatedMoveDistanceMeters = MoveMeters;
		const FDroneCoreCalculationResult Result = FDroneCombatRules::CalculateCoreBonus(Input);
		TestTrue(FString::Printf(TEXT("%s core attack modifier"), Label),
			FMath::IsNearlyEqual(Result.CoreAttackModifier, ExpectedCoreModifier, 0.001f));
		TestTrue(FString::Printf(TEXT("%s core bonus attack modifier"), Label),
			FMath::IsNearlyEqual(Result.CoreBonusAttackModifier, ExpectedBonusModifier, 0.001f));
		TestTrue(FString::Printf(TEXT("%s move speed bonus"), Label),
			FMath::IsNearlyEqual(Result.MoveSpeedBonus, ExpectedMoveSpeedBonus, 0.001f));
	};

	ExpectCore(EDroneCombatCoreType::None, 100.0f, 100.0f, 0.0f, 1.0f, 1.0f, 0.0f, TEXT("empty"));
	ExpectCore(EDroneCombatCoreType::Zenith, 0.0f, 100.0f, 0.0f, 1.0f, 1.0f, 0.0f, TEXT("Zenith 0 percent HP"));
	ExpectCore(EDroneCombatCoreType::Zenith, 10.0f, 100.0f, 0.0f, 1.0f, 1.02f, 0.0f, TEXT("Zenith 10 percent HP"));
	ExpectCore(EDroneCombatCoreType::Zenith, 50.0f, 100.0f, 0.0f, 1.0f, 1.10f, 0.0f, TEXT("Zenith 50 percent HP"));
	ExpectCore(EDroneCombatCoreType::Zenith, 100.0f, 100.0f, 0.0f, 1.0f, 1.20f, 0.0f, TEXT("Zenith 100 percent HP"));
	ExpectCore(EDroneCombatCoreType::Booster, 100.0f, 100.0f, 0.0f, 0.95f, 1.0f, 0.0f, TEXT("Booster 0m"));
	ExpectCore(EDroneCombatCoreType::Booster, 100.0f, 100.0f, 19.99f, 0.95f, 1.0f, 0.0f, TEXT("Booster 19.99m"));
	ExpectCore(EDroneCombatCoreType::Booster, 100.0f, 100.0f, 20.0f, 0.95f, 1.015f, 0.03f, TEXT("Booster 20m"));
	ExpectCore(EDroneCombatCoreType::Booster, 100.0f, 100.0f, 40.0f, 0.95f, 1.03f, 0.06f, TEXT("Booster 40m"));
	ExpectCore(EDroneCombatCoreType::Booster, 100.0f, 100.0f, 199.99f, 0.95f, 1.135f, 0.27f, TEXT("Booster just before cap"));
	ExpectCore(EDroneCombatCoreType::Booster, 100.0f, 100.0f, 200.0f, 0.95f, 1.15f, 0.30f, TEXT("Booster cap"));
	ExpectCore(EDroneCombatCoreType::Booster, 100.0f, 100.0f, 1000.0f, 0.95f, 1.15f, 0.30f, TEXT("Booster over cap"));
	ExpectCore(EDroneCombatCoreType::Drain, 100.0f, 100.0f, 200.0f, 0.85f, 1.0f, 0.0f, TEXT("Drain base penalty"));

	const auto ExpectWeapon = [this](EDroneCombatWeaponType WeaponType, int32 PulseCount, float VectorMeters, float ExpectedDamage, float ExpectedBaseDamage, float ExpectedBonusDamage, int32 ExpectedHitCount, int32 ExpectedAdditionalHitCount, bool bExpectedResetVector, int32 ExpectedPulseCount, const TCHAR* Label)
	{
		FDroneWeaponCalculationInput Input;
		Input.WeaponType = WeaponType;
		Input.PulseAttackCount = PulseCount;
		Input.VectorAccumulatedMoveDistanceMeters = VectorMeters;
		const FDroneWeaponCalculationResult Result = FDroneCombatRules::CalculateWeaponDamage(Input);
		TestTrue(FString::Printf(TEXT("%s weapon damage"), Label),
			FMath::IsNearlyEqual(Result.WeaponDamage, ExpectedDamage, 0.001f));
		TestTrue(FString::Printf(TEXT("%s base damage"), Label),
			FMath::IsNearlyEqual(Result.BaseDamage, ExpectedBaseDamage, 0.001f));
		TestTrue(FString::Printf(TEXT("%s bonus damage"), Label),
			FMath::IsNearlyEqual(Result.BonusDamage, ExpectedBonusDamage, 0.001f));
		TestEqual(FString::Printf(TEXT("%s total hit count"), Label), Result.HitCount, ExpectedHitCount);
		TestEqual(FString::Printf(TEXT("%s additional hit count"), Label), Result.AdditionalHitCount, ExpectedAdditionalHitCount);
		TestEqual(FString::Printf(TEXT("%s vector reset flag"), Label), Result.bResetVectorDistance, bExpectedResetVector);
		TestEqual(FString::Printf(TEXT("%s pulse counter"), Label), Result.PulseAttackCount, ExpectedPulseCount);
	};

	ExpectWeapon(EDroneCombatWeaponType::None, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0, false, 0, TEXT("empty weapon"));
	ExpectWeapon(EDroneCombatWeaponType::PulseLaser, 0, 0.0f, 8.0f, 8.0f, 0.0f, 1, 0, false, 1, TEXT("Pulse first"));
	ExpectWeapon(EDroneCombatWeaponType::PulseLaser, 1, 0.0f, 8.0f, 8.0f, 0.0f, 1, 0, false, 2, TEXT("Pulse second"));
	ExpectWeapon(EDroneCombatWeaponType::PulseLaser, 2, 0.0f, 18.0f, 8.0f, 10.0f, 1, 0, false, 0, TEXT("Pulse third"));
	ExpectWeapon(EDroneCombatWeaponType::FractureBurst, 0, 0.0f, 11.0f, 5.0f, 6.0f, 4, 3, false, 0, TEXT("Fracture"));
	ExpectWeapon(EDroneCombatWeaponType::VectorCannon, 0, 0.0f, 7.0f, 7.0f, 0.0f, 1, 0, true, 0, TEXT("Vector 0m"));
	ExpectWeapon(EDroneCombatWeaponType::VectorCannon, 0, 4.9f, 7.0f, 7.0f, 0.0f, 1, 0, true, 0, TEXT("Vector 4.9m"));
	ExpectWeapon(EDroneCombatWeaponType::VectorCannon, 0, 5.0f, 8.0f, 7.0f, 1.0f, 1, 0, true, 0, TEXT("Vector 5m"));
	ExpectWeapon(EDroneCombatWeaponType::VectorCannon, 0, 10.0f, 9.0f, 7.0f, 2.0f, 1, 0, true, 0, TEXT("Vector 10m"));
	ExpectWeapon(EDroneCombatWeaponType::VectorCannon, 0, 40.0f, 15.0f, 7.0f, 8.0f, 1, 0, true, 0, TEXT("Vector cap"));
	ExpectWeapon(EDroneCombatWeaponType::VectorCannon, 0, 100.0f, 15.0f, 7.0f, 8.0f, 1, 0, true, 0, TEXT("Vector over cap"));
	TestTrue(TEXT("Drain heal is 12 percent before cap"),
		FMath::IsNearlyEqual(FDroneCombatRules::CalculateDrainHeal(10.0f), 1.2f, 0.001f));
	TestTrue(TEXT("Drain heal is clamped to zero for negative damage"),
		FMath::IsNearlyZero(FDroneCombatRules::CalculateDrainHeal(-10.0f), 0.001f));
	TestTrue(TEXT("Drain heal caps at 3 per attack"),
		FMath::IsNearlyEqual(FDroneCombatRules::CalculateDrainHeal(100.0f), 3.0f, 0.001f));

	FDroneSelectionTestContext Context = CreateDroneSelectionTestContext(TEXT("DroneD13DrainActualDamageWorld"));
	ARaidBoss* Boss = nullptr;
	if (!PrepareBattleAttackTest(*this, Context, Boss, TEXT("D13 drain actual damage")))
	{
		DestroyDroneSelectionTestContext(Context);
		return false;
	}

	const FName DrainCore = ADronePartInventory::GetCoreDrainPartID();
	const FName FractureBurst = ADronePartInventory::GetFractureBurstPartID();
	TestTrue(TEXT("D13 Drain double Fracture loadout applies"),
		Context.Drone->ApplyLoadout(DrainCore, FractureBurst, FractureBurst));
	Context.Drone->ApplyDamageForServer(20, FName(TEXT("Automation")));
	Boss->ApplyDamageForServer(Boss->GetMaxHP() - 5.0f, Context.PC, Context.Drone);
	const float HPBeforeClampedDrain = Context.Drone->GetHealthValueForTest();
	TestTrue(TEXT("Drain uses actual boss damage after HP clamp"),
		FMath::IsNearlyEqual(AttackBossAndMeasureDamage(Context.Drone, Boss), 5.0f, 0.001f));
	TestTrue(TEXT("Drain heals 12 percent of actual boss damage"),
		FMath::IsNearlyEqual(Context.Drone->GetHealthValueForTest(), HPBeforeClampedDrain + 0.6f, 0.001f));
	TestTrue(TEXT("CombatRecord stores actual boss damage after HP clamp"),
		FMath::IsNearlyEqual(Context.Drone->GetCombatRecordForTest().BossDamage, 5.0f, 0.001f));

	DestroyDroneSelectionTestContext(Context);

	FDroneSelectionTestContext MoveSpeedContext = CreateDroneSelectionTestContext(TEXT("DroneQ2CoreMoveSpeedWorld"));
	if (!MoveSpeedContext.Drone)
	{
		DestroyDroneSelectionTestContext(MoveSpeedContext);
		return false;
	}
	TestTrue(TEXT("Drain move speed loadout applies"),
		MoveSpeedContext.Drone->ApplyLoadout(DrainCore, NAME_None, NAME_None));
	TestTrue(TEXT("Drain core applies 0.9 base move speed on the server path"),
		FMath::IsNearlyEqual(MoveSpeedContext.Drone->GetCurrentMoveSpeed(), 4.05f, 0.001f));
	TestTrue(TEXT("Booster core keeps base move speed before movement stacks"),
		MoveSpeedContext.Drone->ApplyLoadout(ADronePartInventory::GetCoreBoosterPartID(), NAME_None, NAME_None));
	TestTrue(TEXT("Booster zero stacks keeps 4.5 m/s base speed"),
		FMath::IsNearlyEqual(MoveSpeedContext.Drone->GetCurrentMoveSpeed(), 4.5f, 0.001f));
	DestroyDroneSelectionTestContext(MoveSpeedContext);
	return true;
}

bool FDroneReportFormulaTest::RunTest(const FString& Parameters)
{
	FDroneCombatRecord CappedBaseRecord;
	CappedBaseRecord.SurvivalTime = 240.0f;
	CappedBaseRecord.BossDamage = 4800.0f;
	CappedBaseRecord.BossMaxHP = 60000.0f;
	CappedBaseRecord.BossHPOnJoin = 60000.0f;
	CappedBaseRecord.MoveDistance = 600.0f;
	CappedBaseRecord.HealAmount = 60.0f;
	CappedBaseRecord.DamageTakenCount = 1;
	CappedBaseRecord.CombatStartTime = 0.0f;
	CappedBaseRecord.CombatEndTime = 20.0f;
	CappedBaseRecord.bIsAliveAtReport = true;
	const FDroneReportData CappedBaseReport = FDroneReportRules::BuildReportData(CappedBaseRecord, false);
	TestTrue(TEXT("Survival, boss damage, move, and heal base scores cap at 750 total"),
		FMath::IsNearlyEqual(CappedBaseReport.ReportScore, 750.0f, 0.01f));
	TestTrue(TEXT("BossDamageRatio is stored for UI"),
		FMath::IsNearlyEqual(CappedBaseReport.BossDamageRatio, 0.08f, 0.001f));
	TestEqual(TEXT("750 score maps to grade A"), CappedBaseReport.Grade, EDroneReportGrade::A);

	FDroneCombatRecord LowDamageRecord = CappedBaseRecord;
	LowDamageRecord.BossDamage = 300.0f;
	LowDamageRecord.MoveDistance = 600.0f;
	LowDamageRecord.HealAmount = 60.0f;
	const FDroneReportData LowDamageReport = FDroneReportRules::BuildReportData(LowDamageRecord, false);
	TestTrue(TEXT("BossDamageRatio under 1 percent zeros move and heal score"),
		FMath::IsNearlyEqual(LowDamageReport.ReportScore, 221.875f, 0.01f));
	TestEqual(TEXT("Low contribution report has no bonuses"), LowDamageReport.BonusScore, 0);

	FDroneCombatRecord AllBonusRecord = CappedBaseRecord;
	AllBonusRecord.BossDamage = 7200.0f;
	AllBonusRecord.MoveDistance = 900.0f;
	AllBonusRecord.HealAmount = 70.0f;
	AllBonusRecord.DamageTakenCount = 0;
	AllBonusRecord.CombatEndTime = 180.0f;
	const FDroneReportData AllBonusReport = FDroneReportRules::BuildReportData(AllBonusRecord, true);
	TestEqual(TEXT("BonusScore caps at 250"), AllBonusReport.BonusScore, 250);
	TestEqual(TEXT("All five bonus types are listed before cap"), AllBonusReport.AchievedBonusList.Num(), 5);

	FDroneCombatRecord BossSlayerRecord = CappedBaseRecord;
	BossSlayerRecord.BossDamage = 1800.0f;
	BossSlayerRecord.MoveDistance = 0.0f;
	BossSlayerRecord.HealAmount = 0.0f;
	BossSlayerRecord.DamageTakenCount = 1;
	BossSlayerRecord.CombatEndTime = 180.0f;
	const FDroneReportData BossSlayerReport = FDroneReportRules::BuildReportData(BossSlayerRecord, true);
	TestEqual(TEXT("BossSlayer basic bonus grants 80"), BossSlayerReport.BonusScore, 80);
	TestTrue(TEXT("BossSlayer bonus type is listed"),
		BossSlayerReport.AchievedBonusList.Contains(EDroneReportBonusType::BossSlayer));

	FDroneCombatRecord LateJoinRecord = BossSlayerRecord;
	LateJoinRecord.BossDamage = 600.0f;
	LateJoinRecord.BossHPOnJoin = 15000.0f;
	LateJoinRecord.CombatEndTime = 30.0f;
	const FDroneReportData LateJoinReport = FDroneReportRules::BuildReportData(LateJoinRecord, true);
	TestEqual(TEXT("Late join BossSlayer bonus is limited to 40"), LateJoinReport.BonusScore, 40);

	FDroneCombatRecord TooShortRecord = AllBonusRecord;
	TooShortRecord.CombatEndTime = 29.0f;
	const FDroneReportData TooShortReport = FDroneReportRules::BuildReportData(TooShortRecord, true);
	TestEqual(TEXT("CombatDuration under 30 seconds blocks bonuses"), TooShortReport.BonusScore, 0);

	TestEqual(TEXT("850 score maps to S"), FDroneReportRules::CalculateGrade(850.0f), EDroneReportGrade::S);
	TestEqual(TEXT("650 score maps to A"), FDroneReportRules::CalculateGrade(650.0f), EDroneReportGrade::A);
	TestEqual(TEXT("400 score maps to B"), FDroneReportRules::CalculateGrade(400.0f), EDroneReportGrade::B);
	TestEqual(TEXT("399 score maps to C"), FDroneReportRules::CalculateGrade(399.0f), EDroneReportGrade::C);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneReportDuplicateGenerationTest,
	"DroneProto.D10.DroneReport.DuplicateGeneration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneReportDuplicateGenerationTest::RunTest(const FString& Parameters)
{
	FDroneSelectionTestContext Context = CreateDroneSelectionTestContext(TEXT("DroneReportDuplicateWorld"));
	TestNotNull(TEXT("test player controller is spawned"), Context.PC);
	TestNotNull(TEXT("test drone is spawned"), Context.Drone);
	if (!Context.PC || !Context.Drone)
	{
		DestroyDroneSelectionTestContext(Context);
		return false;
	}

	Context.Drone->ApplyDamageForServer(Context.Drone->GetMaxHealth() + 10, FName(TEXT("Automation")));
	TestTrue(TEXT("death creates exactly one server-held report"), Context.PC->HasDroneReportGeneratedForTest());
	const FDroneReportData FirstReport = Context.PC->GetLastDroneReportDataForTest();
	TestTrue(TEXT("created report is marked generated"), FirstReport.bIsReportGenerated);

	TestFalse(TEXT("second report request is ignored"),
		Context.PC->TryCreateDroneReportForServer(EDroneReportTrigger::RaidTimeLimit, false));
	const FDroneReportData DuplicateAttemptReport = Context.PC->GetLastDroneReportDataForTest();
	TestTrue(TEXT("duplicate attempt does not clear the existing report"),
		DuplicateAttemptReport.bIsReportGenerated);
	TestTrue(TEXT("duplicate attempt preserves the first report score"),
		FMath::IsNearlyEqual(DuplicateAttemptReport.ReportScore, FirstReport.ReportScore, 0.01f));

	DestroyDroneSelectionTestContext(Context);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneReportWidgetTextTest,
	"DroneProto.D11.DroneReport.WidgetTextHelpers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneReportWidgetTextTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("BossSlayer bonus display name"),
		UDroneReportWidget::GetBonusTypeDisplayText(EDroneReportBonusType::BossSlayer).ToString(),
		FString(TEXT("Boss Slayer")));
	TestEqual(TEXT("HighDPS bonus display name"),
		UDroneReportWidget::GetBonusTypeDisplayText(EDroneReportBonusType::HighDPS).ToString(),
		FString(TEXT("High DPS")));
	TestEqual(TEXT("NoDamage bonus display name"),
		UDroneReportWidget::GetBonusTypeDisplayText(EDroneReportBonusType::NoDamage).ToString(),
		FString(TEXT("NO DAMAGE")));
	TestEqual(TEXT("KeepMoving bonus display name"),
		UDroneReportWidget::GetBonusTypeDisplayText(EDroneReportBonusType::KeepMoving).ToString(),
		FString(TEXT("Keep Moving")));
	TestEqual(TEXT("HighRecovery bonus display name"),
		UDroneReportWidget::GetBonusTypeDisplayText(EDroneReportBonusType::HighRecovery).ToString(),
		FString(TEXT("High Recovery")));

	TestEqual(TEXT("S grade display text"), UDroneReportWidget::GetGradeDisplayText(EDroneReportGrade::S).ToString(), FString(TEXT("S")));
	TestEqual(TEXT("A grade display text"), UDroneReportWidget::GetGradeDisplayText(EDroneReportGrade::A).ToString(), FString(TEXT("A")));
	TestEqual(TEXT("B grade display text"), UDroneReportWidget::GetGradeDisplayText(EDroneReportGrade::B).ToString(), FString(TEXT("B")));
	TestEqual(TEXT("C grade display text"), UDroneReportWidget::GetGradeDisplayText(EDroneReportGrade::C).ToString(), FString(TEXT("C")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneReportWidgetReturnToLobbyTest,
	"DroneProto.D11.DroneReport.ReturnToLobbyGuard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneReportWidgetReturnToLobbyTest::RunTest(const FString& Parameters)
{
	UDroneReportWidget* Widget = NewObject<UDroneReportWidget>();
	TestNotNull(TEXT("drone report widget is created"), Widget);
	if (!Widget)
	{
		return false;
	}

	Widget->SetSuppressReturnToLobbyTravelForTest(true);

	Widget->RequestReturnToLobby();
	TestEqual(TEXT("first return-to-lobby request records one travel"), Widget->GetReturnToLobbyTravelRequestCountForTest(), 1);

	Widget->RequestReturnToLobby();
	TestEqual(TEXT("duplicate return-to-lobby request is ignored"), Widget->GetReturnToLobbyTravelRequestCountForTest(), 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDronePartInventoryStockTest,
	"DroneProto.D4.DronePartInventory.StockConsumeReturn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDronePartInventoryStockTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("DronePartInventoryTestWorld")));
	TestNotNull(TEXT("test world is created"), World);
	if (!World)
	{
		return false;
	}

	ADronePartInventory* Inventory = World->SpawnActor<ADronePartInventory>();
	TestNotNull(TEXT("inventory actor is spawned"), Inventory);
	if (!Inventory)
	{
		World->DestroyWorld(false);
		return false;
	}

	TestTrue(TEXT("inventory actor replicates for GameState pointer delivery"), Inventory->GetIsReplicated());
	TestTrue(TEXT("inventory actor is always relevant so clients receive the referenced actor"), Inventory->bAlwaysRelevant);
	TestEqual(TEXT("inventory actor does not start dormant"), static_cast<uint8>(Inventory->NetDormancy), static_cast<uint8>(DORM_Never));

	const FName CoreZenith = ADronePartInventory::GetCoreZenithPartID();
	const FName PulseLaser = ADronePartInventory::GetPulseLaserPartID();
	const FName FractureBurst = ADronePartInventory::GetFractureBurstPartID();

	TestEqual(TEXT("Zenith Core uses the design PartID"), CoreZenith, FName(TEXT("CORE_001")));
	TestEqual(TEXT("Pulse Laser uses the design PartID"), PulseLaser, FName(TEXT("WEAPON_001")));

	EDronePartType PartType = EDronePartType::Weapon;
	TestTrue(TEXT("Zenith Core has a registered part type"), Inventory->GetPartType(CoreZenith, PartType));
	TestEqual(TEXT("Zenith Core is a Core part"), static_cast<uint8>(PartType), static_cast<uint8>(EDronePartType::Core));
	TestEqual(TEXT("Zenith Core starts with design max/current stock"), Inventory->GetCurrentCount(CoreZenith), 5);
	TestEqual(TEXT("Pulse Laser starts with design max/current stock"), Inventory->GetCurrentCount(PulseLaser), 11);
	TestEqual(TEXT("Fracture Burst starts with design max/current stock"), Inventory->GetCurrentCount(FractureBurst), 10);

	TestTrue(TEXT("Zenith Core can be consumed while available"), Inventory->TryConsumePart(CoreZenith));
	TestEqual(TEXT("Zenith Core count decreases after consume"), Inventory->GetCurrentCount(CoreZenith), 4);

	Inventory->ReturnPart(CoreZenith);
	TestEqual(TEXT("Zenith Core count returns after cancellation"), Inventory->GetCurrentCount(CoreZenith), 5);

	Inventory->ReturnPart(CoreZenith);
	TestEqual(TEXT("Zenith Core count does not exceed max"), Inventory->GetCurrentCount(CoreZenith), 5);

	TestFalse(TEXT("unknown part cannot be consumed"), Inventory->TryConsumePart(TEXT("UNKNOWN_PART")));
	TestEqual(TEXT("unknown part count is zero"), Inventory->GetCurrentCount(TEXT("UNKNOWN_PART")), 0);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneQ5DataTableSchemaRowsTest,
	"DroneProto.Q5.DataTable.SchemaRows",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneQ5DataTableSchemaRowsTest::RunTest(const FString& Parameters)
{
	FDronePartCountRow PartRow;
	PartRow.PartID = ADronePartInventory::GetCoreZenithPartID();
	PartRow.Name = FText::FromString(TEXT("Zenith Core"));
	PartRow.Type = EDronePartType::Core;
	PartRow.MaxCount = 5;
	PartRow.IsSelectable = true;
	TestEqual(TEXT("part row exposes PartID"), PartRow.PartID, ADronePartInventory::GetCoreZenithPartID());
	TestEqual(TEXT("part row exposes Name"), PartRow.Name.ToString(), FString(TEXT("Zenith Core")));
	TestEqual(TEXT("part row exposes Type"), static_cast<uint8>(PartRow.Type), static_cast<uint8>(EDronePartType::Core));
	TestEqual(TEXT("part row exposes MaxCount"), PartRow.MaxCount, 5);
	TestTrue(TEXT("part row exposes IsSelectable"), PartRow.IsSelectable);

	FDroneCoreRow CoreRow;
	CoreRow.CoreID = ADronePartInventory::GetCoreDrainPartID();
	CoreRow.AttackModifier = 0.85f;
	CoreRow.MoveSpeedModifier = 0.9f;
	CoreRow.EffectType = FName(TEXT("DAMAGE_TO_HEAL"));
	CoreRow.EffectValue01 = 0.2f;
	CoreRow.EffectValue02 = 0.0f;
	CoreRow.EffectMaxValue = 3.0f;
	TestEqual(TEXT("core row exposes CoreID"), CoreRow.CoreID, ADronePartInventory::GetCoreDrainPartID());
	TestEqual(TEXT("core row exposes AttackModifier"), CoreRow.AttackModifier, 0.85f);
	TestEqual(TEXT("core row exposes MoveSpeedModifier"), CoreRow.MoveSpeedModifier, 0.9f);
	TestEqual(TEXT("core row exposes EffectType"), CoreRow.EffectType, FName(TEXT("DAMAGE_TO_HEAL")));
	TestEqual(TEXT("core row exposes EffectValue01"), CoreRow.EffectValue01, 0.2f);
	TestEqual(TEXT("core row exposes EffectValue02"), CoreRow.EffectValue02, 0.0f);
	TestEqual(TEXT("core row exposes EffectMaxValue"), CoreRow.EffectMaxValue, 3.0f);

	FDroneWeaponRow WeaponRow;
	WeaponRow.WeaponID = ADronePartInventory::GetFractureBurstPartID();
	WeaponRow.BaseDamage = 5.0f;
	WeaponRow.SpecialEffectType = FName(TEXT("FRACTURE_MULTI_HIT"));
	WeaponRow.SpecialValue01 = 3.0f;
	WeaponRow.SpecialValue02 = 2.0f;
	WeaponRow.SpecialMaxValue = 0.0f;
	WeaponRow.HitCount = 4;
	TestEqual(TEXT("weapon row exposes WeaponID"), WeaponRow.WeaponID, ADronePartInventory::GetFractureBurstPartID());
	TestEqual(TEXT("weapon row exposes BaseDamage"), WeaponRow.BaseDamage, 5.0f);
	TestEqual(TEXT("weapon row exposes SpecialEffectType"), WeaponRow.SpecialEffectType, FName(TEXT("FRACTURE_MULTI_HIT")));
	TestEqual(TEXT("weapon row exposes SpecialValue01"), WeaponRow.SpecialValue01, 3.0f);
	TestEqual(TEXT("weapon row exposes SpecialValue02"), WeaponRow.SpecialValue02, 2.0f);
	TestEqual(TEXT("weapon row exposes SpecialMaxValue"), WeaponRow.SpecialMaxValue, 0.0f);
	TestEqual(TEXT("weapon row exposes HitCount"), WeaponRow.HitCount, 4);

	FDroneBonusRow BonusRow;
	BonusRow.BonusID = FName(TEXT("BONUS_001"));
	BonusRow.BonusName = FName(TEXT("BossSlayer"));
	BonusRow.BonusDisplayName = FText::FromString(TEXT("Boss Slayer"));
	BonusRow.BonusScore = 80;
	BonusRow.MinCombatDuration = 60.0f;
	BonusRow.MinBossDamageRatio = 0.03f;
	BonusRow.MaxScore = 80;
	TestEqual(TEXT("bonus row exposes BonusID"), BonusRow.BonusID, FName(TEXT("BONUS_001")));
	TestEqual(TEXT("bonus row exposes BonusName"), BonusRow.BonusName, FName(TEXT("BossSlayer")));
	TestEqual(TEXT("bonus row exposes BonusDisplayName"), BonusRow.BonusDisplayName.ToString(), FString(TEXT("Boss Slayer")));
	TestEqual(TEXT("bonus row exposes BonusScore"), BonusRow.BonusScore, 80);
	TestEqual(TEXT("bonus row exposes MinCombatDuration"), BonusRow.MinCombatDuration, 60.0f);
	TestEqual(TEXT("bonus row exposes MinBossDamageRatio"), BonusRow.MinBossDamageRatio, 0.03f);
	TestEqual(TEXT("bonus row exposes MaxScore"), BonusRow.MaxScore, 80);

	FDroneGradeRow GradeRow;
	GradeRow.Grade = EDroneReportGrade::S;
	GradeRow.MinScore = 850.0f;
	GradeRow.MaxScore = 1000.0f;
	TestEqual(TEXT("grade row exposes Grade"), static_cast<uint8>(GradeRow.Grade), static_cast<uint8>(EDroneReportGrade::S));
	TestEqual(TEXT("grade row exposes MinScore"), GradeRow.MinScore, 850.0f);
	TestEqual(TEXT("grade row exposes MaxScore"), GradeRow.MaxScore, 1000.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneQ5DataTableFallbackStockTest,
	"DroneProto.Q5.DataTable.FallbackStock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneQ5DataTableFallbackStockTest::RunTest(const FString& Parameters)
{
	const FProperty* PartCountTableProperty = ADronePartInventory::StaticClass()->FindPropertyByName(TEXT("PartCountDataTable"));
	TestNotNull(TEXT("inventory exposes a PartCountDataTable candidate"), PartCountTableProperty);
	const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(PartCountTableProperty);
	TestNotNull(TEXT("PartCountDataTable is an object property"), ObjectProperty);
	if (ObjectProperty)
	{
		TestTrue(TEXT("PartCountDataTable accepts UDataTable assets"), ObjectProperty->PropertyClass->IsChildOf(UDataTable::StaticClass()));
	}

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("Q5DataTableFallbackStockWorld")));
	TestNotNull(TEXT("fallback stock world is created"), World);
	if (!World)
	{
		return false;
	}

	ADronePartInventory* Inventory = World->SpawnActor<ADronePartInventory>();
	TestNotNull(TEXT("fallback inventory actor is spawned"), Inventory);
	if (!Inventory)
	{
		World->DestroyWorld(false);
		return false;
	}

	TestEqual(TEXT("fallback keeps six selectable stock rows"), Inventory->GetPartStocks().Num(), 6);
	TestEqual(TEXT("fallback Zenith count remains 5"), Inventory->GetCurrentCount(ADronePartInventory::GetCoreZenithPartID()), 5);
	TestEqual(TEXT("fallback Booster count remains 6"), Inventory->GetCurrentCount(ADronePartInventory::GetCoreBoosterPartID()), 6);
	TestEqual(TEXT("fallback Drain count remains 5"), Inventory->GetCurrentCount(ADronePartInventory::GetCoreDrainPartID()), 5);
	TestEqual(TEXT("fallback Pulse count remains 11"), Inventory->GetCurrentCount(ADronePartInventory::GetPulseLaserPartID()), 11);
	TestEqual(TEXT("fallback Fracture count remains 10"), Inventory->GetCurrentCount(ADronePartInventory::GetFractureBurstPartID()), 10);
	TestEqual(TEXT("fallback Vector count remains 11"), Inventory->GetCurrentCount(ADronePartInventory::GetVectorCannonPartID()), 11);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneQ5DataTablePartCountLoadTest,
	"DroneProto.Q5.DataTable.PartCountLoad",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneQ5DataTablePartCountLoadTest::RunTest(const FString& Parameters)
{
	UDataTable* PartCountTable = NewObject<UDataTable>();
	TestNotNull(TEXT("transient part count data table is created"), PartCountTable);
	if (!PartCountTable)
	{
		return false;
	}
	PartCountTable->RowStruct = FDronePartCountRow::StaticStruct();

	FDronePartCountRow SelectableRow;
	SelectableRow.PartID = FName(TEXT("CORE_TEST"));
	SelectableRow.Name = FText::FromString(TEXT("Test Core"));
	SelectableRow.Type = EDronePartType::Core;
	SelectableRow.MaxCount = 2;
	SelectableRow.IsSelectable = true;
	PartCountTable->AddRow(FName(TEXT("PART_CORE_TEST")), SelectableRow);

	FDronePartCountRow NonSelectableRow;
	NonSelectableRow.PartID = FName(TEXT("WEAPON_HIDDEN"));
	NonSelectableRow.Name = FText::FromString(TEXT("Hidden Weapon"));
	NonSelectableRow.Type = EDronePartType::Weapon;
	NonSelectableRow.MaxCount = 99;
	NonSelectableRow.IsSelectable = false;
	PartCountTable->AddRow(FName(TEXT("PART_WEAPON_HIDDEN")), NonSelectableRow);

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("Q5DataTablePartCountLoadWorld")));
	TestNotNull(TEXT("part count load world is created"), World);
	if (!World)
	{
		return false;
	}

	ADronePartInventory* Inventory = World->SpawnActor<ADronePartInventory>();
	TestNotNull(TEXT("part count load inventory is spawned"), Inventory);
	if (!Inventory)
	{
		World->DestroyWorld(false);
		return false;
	}

	FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(
		ADronePartInventory::StaticClass()->FindPropertyByName(TEXT("PartCountDataTable")));
	TestNotNull(TEXT("PartCountDataTable property can be assigned for tests"), ObjectProperty);
	if (!ObjectProperty)
	{
		World->DestroyWorld(false);
		return false;
	}

	ObjectProperty->SetObjectPropertyValue_InContainer(Inventory, PartCountTable);
	Inventory->DispatchBeginPlay();

	TestEqual(TEXT("data table load replaces fallback row count"), Inventory->GetPartStocks().Num(), 1);
	TestEqual(TEXT("data table selectable row sets current count from max"), Inventory->GetCurrentCount(FName(TEXT("CORE_TEST"))), 2);
	TestEqual(TEXT("data table selectable row sets max count"), Inventory->GetMaxCount(FName(TEXT("CORE_TEST"))), 2);
	TestEqual(TEXT("data table excludes non-selectable rows"), Inventory->GetCurrentCount(FName(TEXT("WEAPON_HIDDEN"))), 0);

	EDronePartType LoadedType = EDronePartType::Weapon;
	TestTrue(TEXT("data table row registers part type"), Inventory->GetPartType(FName(TEXT("CORE_TEST")), LoadedType));
	TestEqual(TEXT("data table row preserves part type"), static_cast<uint8>(LoadedType), static_cast<uint8>(EDronePartType::Core));

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneQ6RaidTimerReplicationTest,
	"DroneProto.Q6.RaidHUD.RaidTimerReplication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneQ6RaidTimerReplicationTest::RunTest(const FString& Parameters)
{
	FDroneSelectionTestContext Context = CreateDroneSelectionTestContext(TEXT("Q6RaidTimerReplicationWorld"));
	ARaidGameMode* GameMode = Context.World ? Context.World->SpawnActor<ARaidGameMode>() : nullptr;
	TestNotNull(TEXT("raid timer world is created"), Context.World);
	TestNotNull(TEXT("raid timer game state is spawned"), Context.GameState);
	TestNotNull(TEXT("raid timer game mode is spawned"), GameMode);
	if (!Context.World || !Context.GameState || !GameMode)
	{
		DestroyDroneSelectionTestContext(Context);
		return false;
	}

	Context.GameState->SetRaidStateForServer(ERaidState::Battle);
	TestTrue(TEXT("raid timer duration is test-configurable"),
		SetFloatPropertyForAutomationTest(GameMode, FName(TEXT("RaidTimeLimitSeconds")), 180.0f));
	GameMode->StartRaidTimeLimitTimerForServer();

	TestTrue(TEXT("raid timer end server time is set"),
		Context.GameState->GetRaidTimeEndServerTime() > Context.World->GetTimeSeconds());
	TestTrue(TEXT("raid timer remaining seconds clamps to 180 max"),
		Context.GameState->GetRaidRemainingSeconds() > 0.0f
		&& Context.GameState->GetRaidRemainingSeconds() <= 180.0f);

	Context.GameState->SetRaidTimeEndServerTimeForServer(Context.World->GetTimeSeconds() + 240.0f);
	TestEqual(TEXT("manual future end time clamps display remaining to 180"),
		Context.GameState->GetRaidRemainingSeconds(),
		180.0f);

	Context.GameState->SetRaidTimeEndServerTimeForServer(Context.World->GetTimeSeconds() - 1.0f);
	TestEqual(TEXT("past end time clamps remaining to zero"),
		Context.GameState->GetRaidRemainingSeconds(),
		0.0f);

	Context.GameState->SetRaidTimeEndServerTimeForServer(0.0f);
	TestEqual(TEXT("cleared end time has zero remaining"),
		Context.GameState->GetRaidRemainingSeconds(),
		0.0f);

	DestroyDroneSelectionTestContext(Context);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneQ6BossHUDWidgetTest,
	"DroneProto.Q6.RaidHUD.BossHUDWidget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneQ6BossHUDWidgetTest::RunTest(const FString& Parameters)
{
	UBossHUDWidget* EmptyWidget = NewObject<UBossHUDWidget>();
	TestNotNull(TEXT("empty boss HUD widget is created"), EmptyWidget);
	if (!EmptyWidget)
	{
		return false;
	}

	TestEqual(TEXT("boss HUD without world has zero HP percent"), EmptyWidget->GetBossHPPercent(), 0.0f);
	TestEqual(TEXT("boss HUD without world has empty HP text"), EmptyWidget->GetBossHPText().ToString(), FString(TEXT("0 / 0")));
	TestEqual(TEXT("boss HUD without world has zero remaining seconds"), EmptyWidget->GetRaidRemainingSeconds(), 0.0f);
	TestEqual(TEXT("boss HUD without world has zero timer text"), EmptyWidget->GetRaidTimerText().ToString(), FString(TEXT("00:00")));

	FDroneSelectionTestContext Context = CreateDroneSelectionTestContext(TEXT("Q6BossHUDWidgetWorld"));
	ARaidBoss* Boss = Context.World ? Context.World->SpawnActor<ARaidBoss>() : nullptr;
	UBossHUDWidget* Widget = Context.World ? NewObject<UBossHUDWidget>(Context.World) : nullptr;
	TestNotNull(TEXT("boss HUD world is created"), Context.World);
	TestNotNull(TEXT("boss HUD game state is spawned"), Context.GameState);
	TestNotNull(TEXT("boss HUD boss is spawned"), Boss);
	TestNotNull(TEXT("boss HUD widget is created"), Widget);
	if (!Context.World || !Context.GameState || !Boss || !Widget)
	{
		DestroyDroneSelectionTestContext(Context);
		return false;
	}

	Context.GameState->SetRaidBossForServer(Boss);
	Boss->ApplyDamageForServer(Boss->GetMaxHP() * 0.25f, Context.PC, Context.Drone);
	TestTrue(TEXT("boss HUD reads replicated boss HP percent"),
		FMath::IsNearlyEqual(Widget->GetBossHPPercent(), 0.75f, 0.001f));
	TestEqual(TEXT("boss HUD builds HP text"),
		Widget->GetBossHPText().ToString(),
		FString(TEXT("45000 / 60000")));

	Context.GameState->SetRaidTimeEndServerTimeForServer(Context.World->GetTimeSeconds() + 125.0f);
	TestTrue(TEXT("boss HUD reads raid remaining seconds"),
		FMath::IsNearlyEqual(Widget->GetRaidRemainingSeconds(), 125.0f, 0.001f));
	TestEqual(TEXT("boss HUD builds mm:ss timer text"),
		Widget->GetRaidTimerText().ToString(),
		FString(TEXT("02:05")));

	TestTrue(TEXT("boss MaxHP is adjustable for safety test"),
		SetFloatPropertyForAutomationTest(Boss, FName(TEXT("MaxHP")), 0.0f));
	Widget->RefreshBossHUD();
	TestEqual(TEXT("boss HUD clamps zero MaxHP percent safely"), Widget->GetBossHPPercent(), 0.0f);
	TestEqual(TEXT("boss HUD clamps zero MaxHP text safely"),
		Widget->GetBossHPText().ToString(),
		FString(TEXT("0 / 0")));

	DestroyDroneSelectionTestContext(Context);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDronePartReturnManagerTest,
	"DroneProto.D5.DronePartReturnManager.ReturnAndReplace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDronePartReturnManagerTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("DronePartReturnManagerTestWorld")));
	TestNotNull(TEXT("test world is created"), World);
	if (!World)
	{
		return false;
	}

	ADronePartInventory* Inventory = World->SpawnActor<ADronePartInventory>();
	ARaidPlayerController* PC = World->SpawnActor<ARaidPlayerController>();
	UDronePartReturnManager* ReturnManager = NewObject<UDronePartReturnManager>();

	TestNotNull(TEXT("inventory actor is spawned"), Inventory);
	TestNotNull(TEXT("player controller is spawned"), PC);
	TestNotNull(TEXT("return manager is created"), ReturnManager);
	if (!Inventory || !PC || !ReturnManager)
	{
		World->DestroyWorld(false);
		return false;
	}

	ReturnManager->Initialize(Inventory);

	const FName CoreZenith = ADronePartInventory::GetCoreZenithPartID();
	const FName CoreBooster = ADronePartInventory::GetCoreBoosterPartID();

	TestTrue(TEXT("initial selection consumes Zenith Core"), Inventory->TryConsumePart(CoreZenith));
	PC->SetSelectedPartIDForSlotForServer(EPartSlot::Core, CoreZenith);
	TestEqual(TEXT("Zenith Core count decreases after selection"), Inventory->GetCurrentCount(CoreZenith), 4);

	TestTrue(TEXT("cancel return succeeds"), ReturnManager->ReturnSingleSelectedPart(PC, EPartSlot::Core, EDronePartReturnReason::Cancel));
	TestEqual(TEXT("Zenith Core returns to stock after cancel"), Inventory->GetCurrentCount(CoreZenith), 5);
	TestEqual(TEXT("selected slot is cleared after cancel"), PC->GetSelectedPartIDBySlot(EPartSlot::Core), NAME_None);

	TestFalse(TEXT("second cancel is skipped because slot is already empty"),
		ReturnManager->ReturnSingleSelectedPart(PC, EPartSlot::Core, EDronePartReturnReason::Cancel));
	TestEqual(TEXT("Zenith Core does not exceed max after duplicate cancel"), Inventory->GetCurrentCount(CoreZenith), 5);

	TestTrue(TEXT("Zenith Core can be selected again"), Inventory->TryConsumePart(CoreZenith));
	PC->SetSelectedPartIDForSlotForServer(EPartSlot::Core, CoreZenith);
	TestEqual(TEXT("Zenith Core count decreases before replace"), Inventory->GetCurrentCount(CoreZenith), 4);

	TestTrue(TEXT("new part is available for replace"), Inventory->IsPartAvailable(CoreBooster));
	TestTrue(TEXT("successful replace first consumes new part"), Inventory->TryConsumePart(CoreBooster));
	TestTrue(TEXT("successful replace returns old part"), ReturnManager->ReturnSingleSelectedPart(PC, EPartSlot::Core, EDronePartReturnReason::Replace));
	PC->SetSelectedPartIDForSlotForServer(EPartSlot::Core, CoreBooster);
	TestEqual(TEXT("old part stock is restored after replace"), Inventory->GetCurrentCount(CoreZenith), 5);
	TestEqual(TEXT("new part stock is consumed after replace"), Inventory->GetCurrentCount(CoreBooster), 5);
	TestEqual(TEXT("slot now points at new part"), PC->GetSelectedPartIDBySlot(EPartSlot::Core), CoreBooster);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDronePartSelectUIGlueTest,
	"DroneProto.D5.DronePartSelectUI.BlueprintGlue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDronePartSelectUIGlueTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("DronePartSelectUIGlueTestWorld")));
	TestNotNull(TEXT("test world is created"), World);
	if (!World)
	{
		return false;
	}

	ARaidPlayerController* PC = World->SpawnActor<ARaidPlayerController>();
	TestNotNull(TEXT("player controller is spawned"), PC);
	if (!PC)
	{
		World->DestroyWorld(false);
		return false;
	}

	const TArray<FName> CorePartIDs = PC->GetCorePartIDs();
	const TArray<FName> WeaponPartIDs = PC->GetWeaponPartIDs();

	TestEqual(TEXT("core candidates are exposed for the UMG arrows"), CorePartIDs.Num(), 3);
	TestEqual(TEXT("weapon candidates are exposed for both weapon slots"), WeaponPartIDs.Num(), 3);
	TestTrue(TEXT("core slot returns core candidates"), PC->GetAvailablePartIDsForSlot(EDronePartSlot::Core).Contains(ADronePartInventory::GetCoreZenithPartID()));
	TestTrue(TEXT("left weapon slot returns weapon candidates"), PC->GetAvailablePartIDsForSlot(EDronePartSlot::LeftWeapon).Contains(ADronePartInventory::GetPulseLaserPartID()));
	TestTrue(TEXT("right weapon slot returns weapon candidates"), PC->GetAvailablePartIDsForSlot(EDronePartSlot::RightWeapon).Contains(ADronePartInventory::GetVectorCannonPartID()));

	TestFalse(TEXT("empty slot reports no selected part"), PC->HasSelectedPartForSlot(EDronePartSlot::Core));
	PC->SetSelectedPartIDForSlotForServer(EPartSlot::Core, ADronePartInventory::GetCoreZenithPartID());
	TestTrue(TEXT("selected slot reports a selected part"), PC->HasSelectedPartForSlot(EDronePartSlot::Core));
	TestEqual(TEXT("UMG getter returns selected part"), PC->GetSelectedPartForSlot(EDronePartSlot::Core), ADronePartInventory::GetCoreZenithPartID());

	TestFalse(TEXT("known part display name is not empty"), PC->GetPartDisplayName(ADronePartInventory::GetCoreZenithPartID()).IsEmpty());
	TestFalse(TEXT("known part description is not empty"), PC->GetPartDescription(ADronePartInventory::GetPulseLaserPartID()).IsEmpty());
	TestEqual(TEXT("unknown part current count is zero without inventory"), PC->GetPartCurrentCount(TEXT("UNKNOWN_PART")), 0);
	TestEqual(TEXT("unknown part max count is zero without inventory"), PC->GetPartMaxCount(TEXT("UNKNOWN_PART")), 0);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRaidGameModeDroneSpawnCollisionTest,
	"DroneProto.D5.RaidGameMode.SpawnsDroneWhenPlayerStartIsOccupied",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRaidGameModeDroneSpawnCollisionTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("RaidGameModeDroneSpawnCollisionTestWorld")));
	TestNotNull(TEXT("test world is created"), World);
	if (!World)
	{
		return false;
	}

	ARaidGameMode* GameMode = World->SpawnActor<ARaidGameMode>();
	ARaidPlayerController* FirstPC = World->SpawnActor<ARaidPlayerController>();
	ARaidPlayerController* SecondPC = World->SpawnActor<ARaidPlayerController>();
	UClass* DronePawnClass = LoadClass<APawn>(nullptr, TEXT("/Game/BP_Drone.BP_Drone_C"));
	ADefaultPawn* DefaultPawnCDO = GetMutableDefault<ADefaultPawn>();

	TestNotNull(TEXT("raid game mode is spawned"), GameMode);
	TestNotNull(TEXT("first player controller is spawned"), FirstPC);
	TestNotNull(TEXT("second player controller is spawned"), SecondPC);
	TestNotNull(TEXT("BP_Drone pawn class is loaded"), DronePawnClass);
	TestNotNull(TEXT("default pawn CDO is available"), DefaultPawnCDO);
	if (!GameMode || !FirstPC || !SecondPC || !DronePawnClass || !DefaultPawnCDO)
	{
		World->DestroyWorld(false);
		return false;
	}

	TestTrue(TEXT("BP_Drone remains an ADrone class for TestMap PIE"), DronePawnClass->IsChildOf(ADrone::StaticClass()));

	const ESpawnActorCollisionHandlingMethod OriginalSpawnCollisionHandling = DefaultPawnCDO->SpawnCollisionHandlingMethod;
	DefaultPawnCDO->SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
	GameMode->DefaultPawnClass = ADefaultPawn::StaticClass();

	const FTransform SharedSpawnTransform(
		FRotator::ZeroRotator,
		FVector(-720.0f, 170.0f, 192.0f),
		FVector::OneVector);

	APawn* FirstPawn = GameMode->SpawnDefaultPawnAtTransform(FirstPC, SharedSpawnTransform);
	TestNotNull(TEXT("first pawn spawn succeeds"), FirstPawn);

	APawn* SecondPawn = GameMode->SpawnDefaultPawnAtTransform(SecondPC, SharedSpawnTransform);
	TestNotNull(TEXT("second pawn spawn succeeds even when the spawn transform is occupied"), SecondPawn);

	DefaultPawnCDO->SpawnCollisionHandlingMethod = OriginalSpawnCollisionHandling;

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDronePlayerSelectionStateTest,
	"DroneProto.D5.RaidPlayerController.PlayerSelectionStateSeparatesRaidState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDronePlayerSelectionStateTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("DronePlayerSelectionStateTestWorld")));
	TestNotNull(TEXT("test world is created"), World);
	if (!World)
	{
		return false;
	}

	ARaidGameState* GameState = World->SpawnActor<ARaidGameState>();
	ADronePartInventory* Inventory = World->SpawnActor<ADronePartInventory>();
	ARaidPlayerController* ReadyPC = World->SpawnActor<ARaidPlayerController>();
	ADrone* ReadyDrone = World->SpawnActor<ADrone>();
	ARaidPlayerController* LateJoinPC = World->SpawnActor<ARaidPlayerController>();
	ADrone* LateJoinDrone = World->SpawnActor<ADrone>();

	TestNotNull(TEXT("game state is spawned"), GameState);
	TestNotNull(TEXT("inventory actor is spawned"), Inventory);
	TestNotNull(TEXT("ready player controller is spawned"), ReadyPC);
	TestNotNull(TEXT("ready drone is spawned"), ReadyDrone);
	TestNotNull(TEXT("late join player controller is spawned"), LateJoinPC);
	TestNotNull(TEXT("late join drone is spawned"), LateJoinDrone);
	if (!GameState || !Inventory || !ReadyPC || !ReadyDrone || !LateJoinPC || !LateJoinDrone)
	{
		World->DestroyWorld(false);
		return false;
	}

	World->SetGameState(GameState);
	GameState->SetDronePartInventory(Inventory);
	GameState->SetRaidStateForServer(ERaidState::Battle);

	ReadyPC->Possess(ReadyDrone);
	LateJoinPC->Possess(LateJoinDrone);

	const FName PulseLaser = ADronePartInventory::GetPulseLaserPartID();
	const FName VectorCannon = ADronePartInventory::GetVectorCannonPartID();

	TestEqual(TEXT("ready player starts in Selecting"), ReadyPC->GetPlayerSelectionState(), EPlayerSelectionState::Selecting);
	TestTrue(TEXT("ready player's Pulse selection consumes stock"), Inventory->TryConsumePart(PulseLaser));
	ReadyPC->SetSelectedPartIDForSlotForServer(EPartSlot::LeftWeapon, PulseLaser);
	ReadyPC->Server_RequestReadyForRaid_Implementation();

	TestEqual(TEXT("ready player locks after Ready"), ReadyPC->GetPlayerSelectionState(), EPlayerSelectionState::InBattle);
	TestEqual(TEXT("ready selected weapon moved out of selected slot"), ReadyPC->GetSelectedPartIDBySlot(EPartSlot::LeftWeapon), NAME_None);
	TestEqual(TEXT("ready selected weapon copied to equipped slot"), ReadyPC->GetEquippedPartIDBySlot(EPartSlot::LeftWeapon), PulseLaser);
	TestEqual(TEXT("Ready keeps the global raid in Battle"), GameState->RaidState, ERaidState::Battle);

	const int32 PulseCountAfterReady = Inventory->GetCurrentCount(PulseLaser);
	ReadyPC->Server_RequestCancelPart_Implementation(EPartSlot::LeftWeapon);
	TestEqual(TEXT("locked player cancel does not return equipped stock"), Inventory->GetCurrentCount(PulseLaser), PulseCountAfterReady);
	TestEqual(TEXT("locked player cancel keeps equipped weapon"), ReadyPC->GetEquippedPartIDBySlot(EPartSlot::LeftWeapon), PulseLaser);

	ReadyPC->Server_RequestSelectPart_Implementation(EPartSlot::RightWeapon, VectorCannon);
	TestEqual(TEXT("locked player select does not change selected slot"), ReadyPC->GetSelectedPartIDBySlot(EPartSlot::RightWeapon), NAME_None);
	TestEqual(TEXT("locked player select does not consume new stock"), Inventory->GetCurrentCount(VectorCannon), 11);

	TestEqual(TEXT("late join player remains Selecting while RaidState is Battle"), LateJoinPC->GetPlayerSelectionState(), EPlayerSelectionState::Selecting);
	TestTrue(TEXT("late join player's Vector selection consumes stock"), Inventory->TryConsumePart(VectorCannon));
	LateJoinPC->SetSelectedPartIDForSlotForServer(EPartSlot::RightWeapon, VectorCannon);
	LateJoinPC->Server_RequestReadyForRaid_Implementation();

	TestEqual(TEXT("late join player can Ready while RaidState is Battle"), LateJoinPC->GetPlayerSelectionState(), EPlayerSelectionState::InBattle);
	TestEqual(TEXT("late join selected weapon copied to equipped slot"), LateJoinPC->GetEquippedPartIDBySlot(EPartSlot::RightWeapon), VectorCannon);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRaidStateDraftingFlowTest,
	"DroneProto.Q3.RaidState.DraftingFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRaidStateDraftingFlowTest::RunTest(const FString& Parameters)
{
	UDronePartReturnManager* ReturnManager = nullptr;
	FDroneSelectionTestContext Context = CreateDroneReturnTestContext(TEXT("RaidStateDraftingFlowWorld"), ReturnManager);
	ARaidGameMode* GameMode = Context.World ? Context.World->SpawnActor<ARaidGameMode>() : nullptr;

	TestNotNull(TEXT("drafting flow world is created"), Context.World);
	TestNotNull(TEXT("drafting flow game state is spawned"), Context.GameState);
	TestNotNull(TEXT("drafting flow inventory is spawned"), Context.Inventory);
	TestNotNull(TEXT("drafting flow player controller is spawned"), Context.PC);
	TestNotNull(TEXT("drafting flow drone is spawned"), Context.Drone);
	TestNotNull(TEXT("drafting flow return manager is created"), ReturnManager);
	TestNotNull(TEXT("drafting flow game mode is spawned"), GameMode);
	if (!Context.World || !Context.GameState || !Context.Inventory || !Context.PC || !Context.Drone || !ReturnManager || !GameMode)
	{
		DestroyDroneSelectionTestContext(Context);
		return false;
	}

	Context.World->AddController(Context.PC);
	const FName PulseLaser = ADronePartInventory::GetPulseLaserPartID();
	TestEqual(TEXT("raid starts Waiting before the first selection entry"), Context.GameState->RaidState, ERaidState::Waiting);

	Context.PC->Server_RequestSelectPart_Implementation(EPartSlot::LeftWeapon, PulseLaser);
	TestEqual(TEXT("first player selection entry selects pulse"), Context.PC->GetSelectedPartIDBySlot(EPartSlot::LeftWeapon), PulseLaser);
	TestEqual(TEXT("first player selection entry moves the global raid to Drafting"),
		Context.GameState->RaidState,
		ERaidState::Drafting);

	Context.PC->Server_RequestReadyForRaid_Implementation();

	TestEqual(TEXT("Ready success moves player into battle"),
		Context.PC->GetPlayerSelectionState(),
		EPlayerSelectionState::InBattle);
	TestEqual(TEXT("Ready success moves the global raid from Drafting to Battle"),
		Context.GameState->RaidState,
		ERaidState::Battle);

	GameMode->ReturnAllEquippedPartsForRaidEnd(FName(TEXT("Automation")));
	TestEqual(TEXT("RaidEnd keeps the global End transition"),
		Context.GameState->RaidState,
		ERaidState::End);

	ARaidPlayerController* LateJoinPC = Context.World->SpawnActor<ARaidPlayerController>();
	ADrone* LateJoinDrone = Context.World->SpawnActor<ADrone>();
	TestNotNull(TEXT("late join player controller is spawned after End"), LateJoinPC);
	TestNotNull(TEXT("late join drone is spawned after End"), LateJoinDrone);
	if (!LateJoinPC || !LateJoinDrone)
	{
		DestroyDroneSelectionTestContext(Context);
		return false;
	}

	Context.World->AddController(LateJoinPC);
	LateJoinPC->Possess(LateJoinDrone);
	LateJoinPC->SetDronePartReturnManagerForTest(ReturnManager);

	const FName VectorCannon = ADronePartInventory::GetVectorCannonPartID();
	LateJoinPC->Server_RequestSelectPart_Implementation(EPartSlot::RightWeapon, VectorCannon);
	TestEqual(TEXT("late join selection after End does not move the raid back to Drafting"),
		Context.GameState->RaidState,
		ERaidState::End);
	TestEqual(TEXT("late join selection after End can remain pending"),
		LateJoinPC->GetSelectedPartIDBySlot(EPartSlot::RightWeapon),
		VectorCannon);
	LateJoinPC->Server_RequestReadyForRaid_Implementation();

	TestEqual(TEXT("late join Ready after End does not enter InBattle"),
		LateJoinPC->GetPlayerSelectionState(),
		EPlayerSelectionState::Selecting);
	TestEqual(TEXT("late join Ready after End keeps selected weapon pending"),
		LateJoinPC->GetSelectedPartIDBySlot(EPartSlot::RightWeapon),
		VectorCannon);
	TestEqual(TEXT("late join Ready after End does not equip weapon"),
		LateJoinPC->GetEquippedPartIDBySlot(EPartSlot::RightWeapon),
		NAME_None);
	TestEqual(TEXT("late join Ready after End does not move the global raid back to Battle"),
		Context.GameState->RaidState,
		ERaidState::End);

	DestroyDroneSelectionTestContext(Context);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneSelectionTimerAutoReadyTest,
	"DroneProto.D5.RaidPlayerController.SelectionTimerAutoReady",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneSelectionTimerAutoReadyTest::RunTest(const FString& Parameters)
{
	FDroneSelectionTestContext EmptyLoadout = CreateDroneSelectionTestContext(TEXT("DroneSelectionTimerEmptyWorld"));
	TestNotNull(TEXT("empty loadout world is created"), EmptyLoadout.World);
	TestNotNull(TEXT("empty loadout player controller is spawned"), EmptyLoadout.PC);
	TestNotNull(TEXT("empty loadout drone is spawned"), EmptyLoadout.Drone);
	if (!EmptyLoadout.World || !EmptyLoadout.PC || !EmptyLoadout.Drone)
	{
		if (EmptyLoadout.World)
		{
			EmptyLoadout.World->DestroyWorld(false);
		}
		return false;
	}

	TestEqual(TEXT("empty loadout starts Selecting"),
		EmptyLoadout.PC->GetCurrentSelectionState(),
		EPlayerSelectionState::Selecting);
	EmptyLoadout.PC->Server_RequestStartSelectionTimer_Implementation();
	TestTrue(TEXT("server selection timer exposes remaining time"),
		EmptyLoadout.PC->GetSelectionRemainingTime() > 0.0f);
	EmptyLoadout.PC->HandleSelectionTimerExpiredForServer();

	TestEqual(TEXT("empty loadout auto ready moves player to InBattle"),
		EmptyLoadout.PC->GetCurrentSelectionState(),
		EPlayerSelectionState::InBattle);
	TestTrue(TEXT("empty loadout reports selection locked after auto ready"),
		EmptyLoadout.PC->IsSelectionLocked());
	TestEqual(TEXT("empty auto ready equips no core"),
		EmptyLoadout.PC->GetEquippedPartIDBySlot(EPartSlot::Core),
		NAME_None);
	TestEqual(TEXT("empty auto ready keeps default attack power"),
		EmptyLoadout.Drone->GetAttackPower(),
		0);
	EmptyLoadout.World->DestroyWorld(false);

	FDroneSelectionTestContext PartialLoadout = CreateDroneSelectionTestContext(TEXT("DroneSelectionTimerPartialWorld"));
	TestNotNull(TEXT("partial loadout world is created"), PartialLoadout.World);
	TestNotNull(TEXT("partial loadout inventory is spawned"), PartialLoadout.Inventory);
	TestNotNull(TEXT("partial loadout player controller is spawned"), PartialLoadout.PC);
	TestNotNull(TEXT("partial loadout drone is spawned"), PartialLoadout.Drone);
	if (!PartialLoadout.World || !PartialLoadout.Inventory || !PartialLoadout.PC || !PartialLoadout.Drone)
	{
		if (PartialLoadout.World)
		{
			PartialLoadout.World->DestroyWorld(false);
		}
		return false;
	}

	const FName PulseLaser = ADronePartInventory::GetPulseLaserPartID();
	TestTrue(TEXT("partial loadout consumes selected weapon before timer"),
		PartialLoadout.Inventory->TryConsumePart(PulseLaser));
	PartialLoadout.PC->SetSelectedPartIDForSlotForServer(EPartSlot::LeftWeapon, PulseLaser);
	PartialLoadout.PC->Server_RequestStartSelectionTimer_Implementation();
	PartialLoadout.PC->HandleSelectionTimerExpiredForServer();

	TestEqual(TEXT("partial loadout auto ready moves player to InBattle"),
		PartialLoadout.PC->GetCurrentSelectionState(),
		EPlayerSelectionState::InBattle);
	TestEqual(TEXT("partial auto ready copies selected weapon to equipped slot"),
		PartialLoadout.PC->GetEquippedPartIDBySlot(EPartSlot::LeftWeapon),
		PulseLaser);
	TestEqual(TEXT("partial auto ready clears selected weapon slot"),
		PartialLoadout.PC->GetSelectedPartIDBySlot(EPartSlot::LeftWeapon),
		NAME_None);
	TestEqual(TEXT("partial auto ready applies weapon attack power"),
		PartialLoadout.Drone->GetAttackPower(),
		8);

	PartialLoadout.World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneSelectionTimerDuplicateReadyTest,
	"DroneProto.D5.RaidPlayerController.SelectionTimerManualReadyPreventsDuplicate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneSelectionTimerDuplicateReadyTest::RunTest(const FString& Parameters)
{
	FDroneSelectionTestContext Context = CreateDroneSelectionTestContext(TEXT("DroneSelectionTimerDuplicateWorld"));
	TestNotNull(TEXT("test world is created"), Context.World);
	TestNotNull(TEXT("inventory actor is spawned"), Context.Inventory);
	TestNotNull(TEXT("player controller is spawned"), Context.PC);
	TestNotNull(TEXT("drone is spawned"), Context.Drone);
	if (!Context.World || !Context.Inventory || !Context.PC || !Context.Drone)
	{
		if (Context.World)
		{
			Context.World->DestroyWorld(false);
		}
		return false;
	}

	const FName PulseLaser = ADronePartInventory::GetPulseLaserPartID();
	TestTrue(TEXT("manual ready test consumes selected weapon"),
		Context.Inventory->TryConsumePart(PulseLaser));
	Context.PC->SetSelectedPartIDForSlotForServer(EPartSlot::LeftWeapon, PulseLaser);
	Context.PC->Server_RequestStartSelectionTimer_Implementation();
	Context.PC->Server_RequestReadyForRaid_Implementation();

	const int32 PulseCountAfterManualReady = Context.Inventory->GetCurrentCount(PulseLaser);
	TestEqual(TEXT("manual ready locks selection"),
		Context.PC->GetCurrentSelectionState(),
		EPlayerSelectionState::InBattle);
	TestEqual(TEXT("manual ready equips selected weapon"),
		Context.PC->GetEquippedPartIDBySlot(EPartSlot::LeftWeapon),
		PulseLaser);
	TestEqual(TEXT("manual ready stops remaining timer display"),
		Context.PC->GetSelectionRemainingTime(),
		0.0f);

	Context.PC->HandleSelectionTimerExpiredForServer();
	TestEqual(TEXT("stale timer expiry does not change consumed stock"),
		Context.Inventory->GetCurrentCount(PulseLaser),
		PulseCountAfterManualReady);
	TestEqual(TEXT("stale timer expiry does not clear equipped slot"),
		Context.PC->GetEquippedPartIDBySlot(EPartSlot::LeftWeapon),
		PulseLaser);

	Context.PC->Server_RequestReadyForRaid_Implementation();
	TestEqual(TEXT("locked manual ready retry does not change stock"),
		Context.Inventory->GetCurrentCount(PulseLaser),
		PulseCountAfterManualReady);
	TestEqual(TEXT("locked manual ready retry keeps equipped slot"),
		Context.PC->GetEquippedPartIDBySlot(EPartSlot::LeftWeapon),
		PulseLaser);

	Context.World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneDeadReadyGuardTest,
	"DroneProto.D6.RaidPlayerController.DeadPawnReadyAndAutoReadyIgnored",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneDeadReadyGuardTest::RunTest(const FString& Parameters)
{
	const FName PulseLaser = ADronePartInventory::GetPulseLaserPartID();
	const FName VectorCannon = ADronePartInventory::GetVectorCannonPartID();

	FDroneSelectionTestContext ManualReady = CreateDroneSelectionTestContext(TEXT("DroneDeadManualReadyGuardWorld"));
	TestNotNull(TEXT("manual ready world is created"), ManualReady.World);
	TestNotNull(TEXT("manual ready inventory actor is spawned"), ManualReady.Inventory);
	TestNotNull(TEXT("manual ready player controller is spawned"), ManualReady.PC);
	TestNotNull(TEXT("manual ready drone is spawned"), ManualReady.Drone);
	if (!ManualReady.World || !ManualReady.Inventory || !ManualReady.PC || !ManualReady.Drone)
	{
		DestroyDroneSelectionTestContext(ManualReady);
		return false;
	}

	TestTrue(TEXT("manual ready dead guard consumes selected weapon"),
		ManualReady.Inventory->TryConsumePart(PulseLaser));
	ManualReady.PC->SetSelectedPartIDForSlotForServer(EPartSlot::LeftWeapon, PulseLaser);
	const int32 ManualPulseCountBeforeReady = ManualReady.Inventory->GetCurrentCount(PulseLaser);

	ManualReady.Drone->ApplyDamageForServer(ManualReady.Drone->GetMaxHealth() + 1, FName(TEXT("Automation")));
	TestTrue(TEXT("manual ready guard starts from a dead drone"), ManualReady.Drone->IsDead());
	ManualReady.PC->Server_RequestReadyForRaid_Implementation();

	TestEqual(TEXT("dead manual ready keeps player Selecting"),
		ManualReady.PC->GetCurrentSelectionState(),
		EPlayerSelectionState::Selecting);
	TestEqual(TEXT("dead manual ready keeps selected weapon"),
		ManualReady.PC->GetSelectedPartIDBySlot(EPartSlot::LeftWeapon),
		PulseLaser);
	TestEqual(TEXT("dead manual ready does not equip selected weapon"),
		ManualReady.PC->GetEquippedPartIDBySlot(EPartSlot::LeftWeapon),
		NAME_None);
	TestEqual(TEXT("dead manual ready does not change selected weapon stock"),
		ManualReady.Inventory->GetCurrentCount(PulseLaser),
		ManualPulseCountBeforeReady);
	DestroyDroneSelectionTestContext(ManualReady);

	FDroneSelectionTestContext AutoReady = CreateDroneSelectionTestContext(TEXT("DroneDeadAutoReadyGuardWorld"));
	TestNotNull(TEXT("auto ready world is created"), AutoReady.World);
	TestNotNull(TEXT("auto ready inventory actor is spawned"), AutoReady.Inventory);
	TestNotNull(TEXT("auto ready player controller is spawned"), AutoReady.PC);
	TestNotNull(TEXT("auto ready drone is spawned"), AutoReady.Drone);
	if (!AutoReady.World || !AutoReady.Inventory || !AutoReady.PC || !AutoReady.Drone)
	{
		DestroyDroneSelectionTestContext(AutoReady);
		return false;
	}

	TestTrue(TEXT("auto ready dead guard consumes selected weapon"),
		AutoReady.Inventory->TryConsumePart(VectorCannon));
	AutoReady.PC->SetSelectedPartIDForSlotForServer(EPartSlot::RightWeapon, VectorCannon);
	AutoReady.PC->Server_RequestStartSelectionTimer_Implementation();
	const int32 AutoVectorCountBeforeReady = AutoReady.Inventory->GetCurrentCount(VectorCannon);

	AutoReady.Drone->ApplyDamageForServer(AutoReady.Drone->GetMaxHealth() + 1, FName(TEXT("Automation")));
	TestTrue(TEXT("auto ready guard starts from a dead drone"), AutoReady.Drone->IsDead());
	AutoReady.PC->HandleSelectionTimerExpiredForServer();

	TestEqual(TEXT("dead auto ready keeps player Selecting"),
		AutoReady.PC->GetCurrentSelectionState(),
		EPlayerSelectionState::Selecting);
	TestEqual(TEXT("dead auto ready keeps selected weapon"),
		AutoReady.PC->GetSelectedPartIDBySlot(EPartSlot::RightWeapon),
		VectorCannon);
	TestEqual(TEXT("dead auto ready does not equip selected weapon"),
		AutoReady.PC->GetEquippedPartIDBySlot(EPartSlot::RightWeapon),
		NAME_None);
	TestEqual(TEXT("dead auto ready does not change selected weapon stock"),
		AutoReady.Inventory->GetCurrentCount(VectorCannon),
		AutoVectorCountBeforeReady);
	DestroyDroneSelectionTestContext(AutoReady);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneSelectionLockedRequestRegressionTest,
	"DroneProto.D5.RaidPlayerController.LockedRequestsDoNotChangeStock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneSelectionLockedRequestRegressionTest::RunTest(const FString& Parameters)
{
	FDroneSelectionTestContext Context = CreateDroneSelectionTestContext(TEXT("DroneSelectionLockedRegressionWorld"));
	TestNotNull(TEXT("test world is created"), Context.World);
	TestNotNull(TEXT("inventory actor is spawned"), Context.Inventory);
	TestNotNull(TEXT("player controller is spawned"), Context.PC);
	TestNotNull(TEXT("drone is spawned"), Context.Drone);
	if (!Context.World || !Context.Inventory || !Context.PC || !Context.Drone)
	{
		if (Context.World)
		{
			Context.World->DestroyWorld(false);
		}
		return false;
	}

	const FName PulseLaser = ADronePartInventory::GetPulseLaserPartID();
	const FName VectorCannon = ADronePartInventory::GetVectorCannonPartID();
	TestTrue(TEXT("locked request test consumes selected weapon"),
		Context.Inventory->TryConsumePart(PulseLaser));
	Context.PC->SetSelectedPartIDForSlotForServer(EPartSlot::LeftWeapon, PulseLaser);
	Context.PC->Server_RequestReadyForRaid_Implementation();

	const int32 PulseCountAfterReady = Context.Inventory->GetCurrentCount(PulseLaser);
	const int32 VectorCountBeforeLockedSelect = Context.Inventory->GetCurrentCount(VectorCannon);

	Context.PC->Server_RequestSelectPart_Implementation(EPartSlot::RightWeapon, VectorCannon);
	Context.PC->Server_RequestCancelPart_Implementation(EPartSlot::LeftWeapon);
	Context.PC->Server_RequestReadyForRaid_Implementation();
	Context.PC->Server_RequestStartSelectionTimer_Implementation();

	TestEqual(TEXT("locked select does not consume another weapon"),
		Context.Inventory->GetCurrentCount(VectorCannon),
		VectorCountBeforeLockedSelect);
	TestEqual(TEXT("locked cancel does not return equipped weapon"),
		Context.Inventory->GetCurrentCount(PulseLaser),
		PulseCountAfterReady);
	TestEqual(TEXT("locked requests keep equipped weapon"),
		Context.PC->GetEquippedPartIDBySlot(EPartSlot::LeftWeapon),
		PulseLaser);
	TestEqual(TEXT("locked requests leave selected right weapon empty"),
		Context.PC->GetSelectedPartIDBySlot(EPartSlot::RightWeapon),
		NAME_None);

	Context.World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneSelectionReturnPathRegressionTest,
	"DroneProto.D5.DronePartReturnManager.SelectionAndBattleReturnPaths",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneSelectionReturnPathRegressionTest::RunTest(const FString& Parameters)
{
	FDroneSelectionTestContext Context = CreateDroneSelectionTestContext(TEXT("DroneSelectionReturnRegressionWorld"));
	UDronePartReturnManager* ReturnManager = NewObject<UDronePartReturnManager>();
	TestNotNull(TEXT("test world is created"), Context.World);
	TestNotNull(TEXT("inventory actor is spawned"), Context.Inventory);
	TestNotNull(TEXT("player controller is spawned"), Context.PC);
	TestNotNull(TEXT("return manager is created"), ReturnManager);
	if (!Context.World || !Context.Inventory || !Context.PC || !ReturnManager)
	{
		if (Context.World)
		{
			Context.World->DestroyWorld(false);
		}
		return false;
	}

	ReturnManager->Initialize(Context.Inventory);

	const FName PulseLaser = ADronePartInventory::GetPulseLaserPartID();
	TestTrue(TEXT("selecting logout path consumes selected part"),
		Context.Inventory->TryConsumePart(PulseLaser));
	Context.PC->SetSelectedPartIDForSlotForServer(EPartSlot::LeftWeapon, PulseLaser);
	TestTrue(TEXT("selecting logout path returns selected parts"),
		ReturnManager->ReturnSelectedParts(Context.PC, EDronePartReturnReason::Disconnect));
	TestEqual(TEXT("selecting logout path restores selected part count"),
		Context.Inventory->GetCurrentCount(PulseLaser),
		Context.Inventory->GetMaxCount(PulseLaser));
	TestEqual(TEXT("selecting logout path clears selected slot"),
		Context.PC->GetSelectedPartIDBySlot(EPartSlot::LeftWeapon),
		NAME_None);

	TestTrue(TEXT("battle logout path consumes equipped part"),
		Context.Inventory->TryConsumePart(PulseLaser));
	Context.PC->SetEquippedPartIDForSlotForServer(EPartSlot::LeftWeapon, PulseLaser);
	TestTrue(TEXT("battle logout path returns equipped parts"),
		ReturnManager->ReturnEquippedParts(Context.PC, EDronePartReturnReason::Disconnect));
	TestEqual(TEXT("battle logout path restores equipped part count"),
		Context.Inventory->GetCurrentCount(PulseLaser),
		Context.Inventory->GetMaxCount(PulseLaser));
	TestEqual(TEXT("battle logout path clears equipped slot"),
		Context.PC->GetEquippedPartIDBySlot(EPartSlot::LeftWeapon),
		NAME_None);
	TestFalse(TEXT("second battle logout return is skipped after slot clear"),
		ReturnManager->ReturnEquippedParts(Context.PC, EDronePartReturnReason::Disconnect));
	TestEqual(TEXT("duplicate equipped return does not exceed max count"),
		Context.Inventory->GetCurrentCount(PulseLaser),
		Context.Inventory->GetMaxCount(PulseLaser));

	Context.World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneApplyLoadoutTest,
	"DroneProto.D5.Drone.ApplySelectedLoadout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneApplyLoadoutTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("DroneApplyLoadoutTestWorld")));
	TestNotNull(TEXT("test world is created"), World);
	if (!World)
	{
		return false;
	}

	ADrone* Drone = World->SpawnActor<ADrone>();
	TestNotNull(TEXT("drone is spawned"), Drone);
	if (!Drone)
	{
		World->DestroyWorld(false);
		return false;
	}

	TestTrue(TEXT("loadout accepts the default drone with no parts"),
		Drone->ApplyLoadout(NAME_None, NAME_None, NAME_None));
	TestEqual(TEXT("default drone keeps base max health"), Drone->GetMaxHealth(), 100);
	TestEqual(TEXT("default drone has zero displayed attack power"), Drone->GetAttackPower(), 0);

	TestTrue(TEXT("loadout accepts core-only selection"),
		Drone->ApplyLoadout(ADronePartInventory::GetCoreZenithPartID(), NAME_None, NAME_None));

	TestTrue(TEXT("loadout accepts one weapon without a core"),
		Drone->ApplyLoadout(NAME_None, ADronePartInventory::GetPulseLaserPartID(), NAME_None));

	TestTrue(TEXT("loadout accepts selected core and weapons"),
		Drone->ApplyLoadout(
			ADronePartInventory::GetCoreZenithPartID(),
			ADronePartInventory::GetPulseLaserPartID(),
			ADronePartInventory::GetVectorCannonPartID()));
	TestEqual(TEXT("loadout keeps base max health when no health part stat exists"), Drone->GetMaxHealth(), 100);
	TestEqual(TEXT("loadout displays total base weapon damage"), Drone->GetAttackPower(), 15);
	TestEqual(TEXT("loadout fills health after drafting"), Drone->GetHealth(), 100);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneAttackBossTest,
	"DroneProto.D5.Drone.ServerAttackBossVerticalSlice",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneAttackBossTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("DroneAttackBossTestWorld")));
	TestNotNull(TEXT("test world is created"), World);
	if (!World)
	{
		return false;
	}

	ADrone* Drone = World->SpawnActor<ADrone>();
	ARaidBoss* Boss = World->SpawnActor<ARaidBoss>();
	ARaidPlayerController* PC = World->SpawnActor<ARaidPlayerController>();
	ARaidGameState* GameState = World->SpawnActor<ARaidGameState>();
	TestNotNull(TEXT("drone is spawned"), Drone);
	TestNotNull(TEXT("boss is spawned"), Boss);
	TestNotNull(TEXT("player controller is spawned"), PC);
	TestNotNull(TEXT("game state is spawned"), GameState);
	if (!Drone || !Boss || !PC || !GameState)
	{
		World->DestroyWorld(false);
		return false;
	}

	// POR-16: 공격은 서버 타겟이 필요하므로 GameState에 보스를 등록해야 Ready 시 타겟이 배정된다.
	// POR-17: BossMinApproachDistanceCm 클램프를 피하도록 보스를 드론에서 떨어뜨린다.
	World->SetGameState(GameState);
	Boss->SetActorLocation(FVector(-2000.0f, 0.0f, 0.0f));
	GameState->SetRaidBossForServer(Boss);

	PC->Possess(Drone);
	TestEqual(TEXT("attack test starts in Selecting"),
		PC->GetCurrentSelectionState(),
		EPlayerSelectionState::Selecting);

	TestTrue(TEXT("selecting drone can have a loadout but still cannot attack"),
		Drone->ApplyLoadout(NAME_None, ADronePartInventory::GetPulseLaserPartID(), NAME_None));
	const float HPBeforeSelectingAttack = Boss->GetCurrentHP();
	Drone->RequestAttackBoss();
	TestTrue(TEXT("Selecting player attack is ignored before boss damage"),
		FMath::IsNearlyEqual(Boss->GetCurrentHP(), HPBeforeSelectingAttack, 0.01f));

	PC->Server_RequestReadyForRaid_Implementation();
	TestEqual(TEXT("ready player moves to InBattle before attacks"),
		PC->GetCurrentSelectionState(),
		EPlayerSelectionState::InBattle);

	TestTrue(TEXT("empty loadout can attack for zero damage"), Drone->ApplyLoadout(NAME_None, NAME_None, NAME_None));
	const float InitialHP = Boss->GetCurrentHP();
	Drone->RequestAttackBoss();
	TestTrue(TEXT("empty weapon slots leave boss HP unchanged"),
		FMath::IsNearlyEqual(Boss->GetCurrentHP(), InitialHP, 0.01f));

	TestTrue(TEXT("Pulse plus Fracture loadout applies both weapon damages"),
		Drone->ApplyLoadout(NAME_None, ADronePartInventory::GetPulseLaserPartID(), ADronePartInventory::GetFractureBurstPartID()));
	Drone->RequestAttackBoss();
	TestTrue(TEXT("first attack deals Pulse 8 plus Fracture 11"),
		FMath::IsNearlyEqual(Boss->GetCurrentHP(), InitialHP - 19.0f, 0.01f));
	Drone->RequestAttackBoss();
	TestTrue(TEXT("second attack deals another 19 damage"),
		FMath::IsNearlyEqual(Boss->GetCurrentHP(), InitialHP - 38.0f, 0.01f));
	Drone->RequestAttackBoss();
	TestTrue(TEXT("third Pulse attack uses strong 18 plus Fracture 11"),
		FMath::IsNearlyEqual(Boss->GetCurrentHP(), InitialHP - 67.0f, 0.01f));

	TestTrue(TEXT("Zenith core applies HP-ratio attack bonus"),
		Drone->ApplyLoadout(ADronePartInventory::GetCoreZenithPartID(), ADronePartInventory::GetPulseLaserPartID(), NAME_None));
	const float HPBeforeZenithAttack = Boss->GetCurrentHP();
	Drone->RequestAttackBoss();
	TestTrue(TEXT("full HP Zenith Pulse attack deals 8 * 1.2 damage"),
		FMath::IsNearlyEqual(Boss->GetCurrentHP(), HPBeforeZenithAttack - 9.6f, 0.01f));

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDronePulseLaserCombatTest,
	"DroneProto.D7.Drone.PulseLaserCombat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDronePulseLaserCombatTest::RunTest(const FString& Parameters)
{
	const FName PulseLaser = ADronePartInventory::GetPulseLaserPartID();

	FDroneSelectionTestContext SinglePulse = CreateDroneSelectionTestContext(TEXT("DronePulseSingleWorld"));
	ARaidBoss* SingleBoss = nullptr;
	if (!PrepareBattleAttackTest(*this, SinglePulse, SingleBoss, TEXT("single pulse")))
	{
		DestroyDroneSelectionTestContext(SinglePulse);
		return false;
	}

	TestTrue(TEXT("single Pulse loadout applies"), SinglePulse.Drone->ApplyLoadout(NAME_None, PulseLaser, NAME_None));
	TestTrue(TEXT("first Pulse attack deals base damage"),
		FMath::IsNearlyEqual(AttackBossAndMeasureDamage(SinglePulse.Drone, SingleBoss), 8.0f, 0.01f));
	TestEqual(TEXT("left Pulse count advances to 1"), SinglePulse.Drone->GetPulseAttackCountForTest(true), 1);
	TestTrue(TEXT("second Pulse attack deals base damage"),
		FMath::IsNearlyEqual(AttackBossAndMeasureDamage(SinglePulse.Drone, SingleBoss), 8.0f, 0.01f));
	TestEqual(TEXT("left Pulse count advances to 2"), SinglePulse.Drone->GetPulseAttackCountForTest(true), 2);
	TestTrue(TEXT("third Pulse attack deals strong damage"),
		FMath::IsNearlyEqual(AttackBossAndMeasureDamage(SinglePulse.Drone, SingleBoss), 18.0f, 0.01f));
	TestEqual(TEXT("third Pulse attack resets left count"), SinglePulse.Drone->GetPulseAttackCountForTest(true), 0);

	TestTrue(TEXT("new loadout resets Pulse count after partial chain"),
		SinglePulse.Drone->ApplyLoadout(NAME_None, PulseLaser, NAME_None));
	AttackBossAndMeasureDamage(SinglePulse.Drone, SingleBoss);
	AttackBossAndMeasureDamage(SinglePulse.Drone, SingleBoss);
	TestEqual(TEXT("left Pulse count reaches 2 before reset"), SinglePulse.Drone->GetPulseAttackCountForTest(true), 2);
	TestTrue(TEXT("reapplying loadout succeeds"), SinglePulse.Drone->ApplyLoadout(NAME_None, PulseLaser, NAME_None));
	TestEqual(TEXT("new loadout clears left Pulse count"), SinglePulse.Drone->GetPulseAttackCountForTest(true), 0);

	AttackBossAndMeasureDamage(SinglePulse.Drone, SingleBoss);
	AttackBossAndMeasureDamage(SinglePulse.Drone, SingleBoss);
	TestEqual(TEXT("left Pulse count reaches 2 before death"), SinglePulse.Drone->GetPulseAttackCountForTest(true), 2);
	SinglePulse.Drone->ApplyDamageForServer(SinglePulse.Drone->GetMaxHealth() + 10, FName(TEXT("Automation")));
	TestTrue(TEXT("lethal damage marks Pulse test drone dead"), SinglePulse.Drone->IsDead());
	TestEqual(TEXT("death clears left Pulse count"), SinglePulse.Drone->GetPulseAttackCountForTest(true), 0);
	DestroyDroneSelectionTestContext(SinglePulse);

	FDroneSelectionTestContext DoublePulse = CreateDroneSelectionTestContext(TEXT("DronePulseDoubleWorld"));
	ARaidBoss* DoubleBoss = nullptr;
	if (!PrepareBattleAttackTest(*this, DoublePulse, DoubleBoss, TEXT("double pulse")))
	{
		DestroyDroneSelectionTestContext(DoublePulse);
		return false;
	}

	TestTrue(TEXT("double Pulse loadout applies"),
		DoublePulse.Drone->ApplyLoadout(NAME_None, PulseLaser, PulseLaser));
	TestTrue(TEXT("double Pulse first attack deals two base hits"),
		FMath::IsNearlyEqual(AttackBossAndMeasureDamage(DoublePulse.Drone, DoubleBoss), 16.0f, 0.01f));
	TestEqual(TEXT("left Pulse count advances independently to 1"),
		DoublePulse.Drone->GetPulseAttackCountForTest(true),
		1);
	TestEqual(TEXT("right Pulse count advances independently to 1"),
		DoublePulse.Drone->GetPulseAttackCountForTest(false),
		1);
	TestTrue(TEXT("double Pulse second attack deals two base hits"),
		FMath::IsNearlyEqual(AttackBossAndMeasureDamage(DoublePulse.Drone, DoubleBoss), 16.0f, 0.01f));
	TestEqual(TEXT("left Pulse count advances independently to 2"),
		DoublePulse.Drone->GetPulseAttackCountForTest(true),
		2);
	TestEqual(TEXT("right Pulse count advances independently to 2"),
		DoublePulse.Drone->GetPulseAttackCountForTest(false),
		2);
	TestTrue(TEXT("double Pulse third attack deals two strong hits"),
		FMath::IsNearlyEqual(AttackBossAndMeasureDamage(DoublePulse.Drone, DoubleBoss), 36.0f, 0.01f));
	TestEqual(TEXT("left Pulse strong hit resets only left count"),
		DoublePulse.Drone->GetPulseAttackCountForTest(true),
		0);
	TestEqual(TEXT("right Pulse strong hit resets only right count"),
		DoublePulse.Drone->GetPulseAttackCountForTest(false),
		0);

	AttackBossAndMeasureDamage(DoublePulse.Drone, DoubleBoss);
	AttackBossAndMeasureDamage(DoublePulse.Drone, DoubleBoss);
	TestEqual(TEXT("left Pulse count reaches 2 before RaidEnd"), DoublePulse.Drone->GetPulseAttackCountForTest(true), 2);
	ARaidGameMode* GameMode = DoublePulse.World->SpawnActor<ARaidGameMode>();
	TestNotNull(TEXT("raid end game mode is spawned"), GameMode);
	if (GameMode)
	{
		DoublePulse.World->AddController(DoublePulse.PC);
		GameMode->ReturnAllEquippedPartsForRaidEnd(FName(TEXT("Automation")));
		TestEqual(TEXT("RaidEnd clears left Pulse count"), DoublePulse.Drone->GetPulseAttackCountForTest(true), 0);
		TestEqual(TEXT("RaidEnd clears right Pulse count"), DoublePulse.Drone->GetPulseAttackCountForTest(false), 0);
	}

	DestroyDroneSelectionTestContext(DoublePulse);

	FDroneSelectionTestContext DeadBossPulse = CreateDroneSelectionTestContext(TEXT("DronePulseDeadBossWorld"));
	ARaidBoss* DeadBoss = nullptr;
	if (!PrepareBattleAttackTest(*this, DeadBossPulse, DeadBoss, TEXT("dead boss pulse")))
	{
		DestroyDroneSelectionTestContext(DeadBossPulse);
		return false;
	}

	TestTrue(TEXT("dead boss Pulse loadout applies"),
		DeadBossPulse.Drone->ApplyLoadout(NAME_None, PulseLaser, NAME_None));
	DeadBoss->ApplyDamageForServer(DeadBoss->GetMaxHP() + 100.0f, DeadBossPulse.PC, DeadBossPulse.Drone);
	TestTrue(TEXT("dead boss setup reaches zero HP"),
		FMath::IsNearlyZero(DeadBoss->GetCurrentHP(), 0.01f));
	DeadBossPulse.Drone->RequestAttackBoss();
	TestEqual(TEXT("BossDead attack does not advance Pulse counter"),
		DeadBossPulse.Drone->GetPulseAttackCountForTest(true),
		0);
	TestTrue(TEXT("BossDead attack keeps boss HP at zero"),
		FMath::IsNearlyZero(DeadBoss->GetCurrentHP(), 0.01f));
	DestroyDroneSelectionTestContext(DeadBossPulse);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneFractureBurstCombatTest,
	"DroneProto.D7.Drone.FractureBurstCombat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneFractureBurstCombatTest::RunTest(const FString& Parameters)
{
	FDroneSelectionTestContext Context = CreateDroneSelectionTestContext(TEXT("DroneFractureBurstWorld"));
	ARaidBoss* Boss = nullptr;
	if (!PrepareBattleAttackTest(*this, Context, Boss, TEXT("fracture burst")))
	{
		DestroyDroneSelectionTestContext(Context);
		return false;
	}

	const FName FractureBurst = ADronePartInventory::GetFractureBurstPartID();
	TestTrue(TEXT("single Fracture loadout applies"),
		Context.Drone->ApplyLoadout(NAME_None, FractureBurst, NAME_None));
	TestTrue(TEXT("single Fracture attack deals base plus shards"),
		FMath::IsNearlyEqual(AttackBossAndMeasureDamage(Context.Drone, Boss), 11.0f, 0.01f));

	TestTrue(TEXT("Fracture plus Fracture with CORE_002 loadout applies"),
		Context.Drone->ApplyLoadout(ADronePartInventory::GetCoreBoosterPartID(), FractureBurst, FractureBurst));
	TestTrue(TEXT("Fracture plus Fracture with CORE_002 applies 0.95 base modifier"),
		FMath::IsNearlyEqual(AttackBossAndMeasureDamage(Context.Drone, Boss), 20.9f, 0.01f));

	DestroyDroneSelectionTestContext(Context);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneZenithCoreCombatTest,
	"DroneProto.D7.Drone.ZenithCoreCombat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneZenithCoreCombatTest::RunTest(const FString& Parameters)
{
	FDroneSelectionTestContext Context = CreateDroneSelectionTestContext(TEXT("DroneZenithCoreWorld"));
	ARaidBoss* Boss = nullptr;
	if (!PrepareBattleAttackTest(*this, Context, Boss, TEXT("zenith core")))
	{
		DestroyDroneSelectionTestContext(Context);
		return false;
	}

	const FName ZenithCore = ADronePartInventory::GetCoreZenithPartID();
	const FName PulseLaser = ADronePartInventory::GetPulseLaserPartID();
	TestTrue(TEXT("Zenith Pulse loadout applies"),
		Context.Drone->ApplyLoadout(ZenithCore, PulseLaser, NAME_None));
	TestTrue(TEXT("full HP Zenith applies 1.20 modifier"),
		FMath::IsNearlyEqual(AttackBossAndMeasureDamage(Context.Drone, Boss), 9.60f, 0.01f));

	TestTrue(TEXT("Zenith Pulse loadout reapplies for half HP case"),
		Context.Drone->ApplyLoadout(ZenithCore, PulseLaser, NAME_None));
	Context.Drone->ApplyDamageForServer(50, FName(TEXT("Automation")));
	TestEqual(TEXT("Zenith half HP setup reaches 50 HP"), Context.Drone->GetHealth(), 50);
	TestTrue(TEXT("50 percent HP Zenith applies 1.10 modifier"),
		FMath::IsNearlyEqual(AttackBossAndMeasureDamage(Context.Drone, Boss), 8.80f, 0.01f));

	DestroyDroneSelectionTestContext(Context);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneDrainCoreCombatTest,
	"DroneProto.D7.Drone.DrainCoreCombat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneDrainCoreCombatTest::RunTest(const FString& Parameters)
{
	FDroneSelectionTestContext Context = CreateDroneSelectionTestContext(TEXT("DroneDrainCoreWorld"));
	ARaidBoss* Boss = nullptr;
	if (!PrepareBattleAttackTest(*this, Context, Boss, TEXT("drain core")))
	{
		DestroyDroneSelectionTestContext(Context);
		return false;
	}

	const FName DrainCore = ADronePartInventory::GetCoreDrainPartID();
	const FName FractureBurst = ADronePartInventory::GetFractureBurstPartID();
	const FName PulseLaser = ADronePartInventory::GetPulseLaserPartID();

	TestTrue(TEXT("Drain Fracture plus Fracture loadout applies"),
		Context.Drone->ApplyLoadout(DrainCore, FractureBurst, FractureBurst));
	Context.Drone->ApplyDamageForServer(20, FName(TEXT("Automation")));
	const float HPBeforeFractureDrain = Context.Drone->GetHealthValueForTest();
	TestTrue(TEXT("Drain Fracture plus Fracture applies 0.85 base modifier"),
		FMath::IsNearlyEqual(AttackBossAndMeasureDamage(Context.Drone, Boss), 18.7f, 0.01f));
	TestTrue(TEXT("Drain heals from total Fracture damage once per input"),
		FMath::IsNearlyEqual(Context.Drone->GetHealthValueForTest(), HPBeforeFractureDrain + 2.244f, 0.01f));

	TestTrue(TEXT("Drain with empty weapons loadout applies"),
		Context.Drone->ApplyLoadout(DrainCore, NAME_None, NAME_None));
	Context.Drone->ApplyDamageForServer(10, FName(TEXT("Automation")));
	const float HPBeforeZeroDamageDrain = Context.Drone->GetHealthValueForTest();
	TestTrue(TEXT("Drain empty weapon attack deals zero damage"),
		FMath::IsNearlyEqual(AttackBossAndMeasureDamage(Context.Drone, Boss), 0.0f, 0.01f));
	TestTrue(TEXT("zero damage Drain attack heals zero"),
		FMath::IsNearlyEqual(Context.Drone->GetHealthValueForTest(), HPBeforeZeroDamageDrain, 0.01f));

	TestTrue(TEXT("Drain Pulse plus Pulse loadout applies"),
		Context.Drone->ApplyLoadout(DrainCore, PulseLaser, PulseLaser));
	Context.Drone->ApplyDamageForServer(50, FName(TEXT("Automation")));
	AttackBossAndMeasureDamage(Context.Drone, Boss);
	AttackBossAndMeasureDamage(Context.Drone, Boss);
	const float HPBeforeStrongDrain = Context.Drone->GetHealthValueForTest();
	TestTrue(TEXT("third Drain double Pulse attack applies base modifier before capped heal"),
		FMath::IsNearlyEqual(AttackBossAndMeasureDamage(Context.Drone, Boss), 30.6f, 0.01f));
	TestTrue(TEXT("Drain heal is capped at 3 per attack input"),
		FMath::IsNearlyEqual(Context.Drone->GetHealthValueForTest(), HPBeforeStrongDrain + 3.0f, 0.01f));

	TestTrue(TEXT("Drain max health cap loadout applies"),
		Context.Drone->ApplyLoadout(DrainCore, FractureBurst, FractureBurst));
	Context.Drone->ApplyDamageForServer(1, FName(TEXT("Automation")));
	AttackBossAndMeasureDamage(Context.Drone, Boss);
	TestTrue(TEXT("Drain never heals beyond MaxHealth"),
		Context.Drone->GetHealthValueForTest() <= static_cast<float>(Context.Drone->GetMaxHealth()) + 0.01f);

	DestroyDroneSelectionTestContext(Context);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneVectorBoosterCombatRecordTest,
	"DroneProto.D9.Drone.VectorBoosterCombatRecord",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneVectorBoosterCombatRecordTest::RunTest(const FString& Parameters)
{
	FDroneSelectionTestContext Context = CreateDroneSelectionTestContext(TEXT("DroneVectorBoosterCombatRecordWorld"));
	ARaidBoss* Boss = nullptr;
	if (!PrepareBattleAttackTest(*this, Context, Boss, TEXT("vector booster record")))
	{
		DestroyDroneSelectionTestContext(Context);
		return false;
	}

	const FName BoosterCore = ADronePartInventory::GetCoreBoosterPartID();
	const FName VectorCannon = ADronePartInventory::GetVectorCannonPartID();
	TestTrue(TEXT("Booster double Vector loadout applies"),
		Context.Drone->ApplyLoadout(BoosterCore, VectorCannon, VectorCannon));

	Context.Drone->SetActorLocation(FVector::ZeroVector);
	Context.Drone->ResetMoveDistanceForServerForTest(FName(TEXT("Automation")));
	Context.Drone->UpdateMoveDistanceForServerForTest(0.016f);
	for (int32 Step = 1; Step <= 20; ++Step)
	{
		Context.Drone->SetActorLocation(FVector(static_cast<float>(Step) * 200.0f, 0.0f, 0.0f));
		Context.Drone->UpdateMoveDistanceForServerForTest(0.25f);
	}

	TestTrue(TEXT("movement setup accumulates 40 meters for Vector"),
		FMath::IsNearlyEqual(Context.Drone->GetVectorAccumulatedMoveDistanceForTest(), 40.0f, 0.01f));
	TestTrue(TEXT("movement setup accumulates 40 meters for Booster"),
		FMath::IsNearlyEqual(Context.Drone->GetBoosterAccumulatedMoveDistanceForTest(), 40.0f, 0.01f));
	TestTrue(TEXT("double Vector uses capped damage and Booster attack bonus"),
		FMath::IsNearlyEqual(AttackBossAndMeasureDamage(Context.Drone, Boss), 29.355f, 0.01f));
	TestTrue(TEXT("Vector attack resets Vector distance only"),
		FMath::IsNearlyZero(Context.Drone->GetVectorAccumulatedMoveDistanceForTest(), 0.001f));
	TestTrue(TEXT("Vector attack keeps Booster distance"),
		FMath::IsNearlyEqual(Context.Drone->GetBoosterAccumulatedMoveDistanceForTest(), 40.0f, 0.01f));

	const FDroneCombatRecord Record = Context.Drone->GetCombatRecordForTest();
	TestTrue(TEXT("CombatRecord accumulates boss damage"),
		FMath::IsNearlyEqual(Record.BossDamage, 29.355f, 0.01f));
	TestTrue(TEXT("CombatRecord accumulates move distance"),
		FMath::IsNearlyEqual(Record.MoveDistance, 40.0f, 0.01f));
	TestTrue(TEXT("CombatRecord stores boss max HP for ratio calculation"),
		FMath::IsNearlyEqual(Record.BossMaxHP, Boss->GetMaxHP(), 0.01f));
	TestTrue(TEXT("CombatRecord stores boss HP on join"),
		Record.BossHPOnJoin > 0.0f);

	Context.Drone->ApplyDamageForServer(10, FName(TEXT("Automation")));
	const FDroneCombatRecord DamagedRecord = Context.Drone->GetCombatRecordForTest();
	TestEqual(TEXT("CombatRecord increments damage taken count"), DamagedRecord.DamageTakenCount, 1);
	TestTrue(TEXT("server report generation succeeds for Vector/Booster record"),
		Context.PC->TryCreateDroneReportForServer(EDroneReportTrigger::RaidTimeLimit, false));
	const FDroneReportData Report = Context.PC->GetLastDroneReportDataForTest();
	TestTrue(TEXT("DroneReport copies server CombatRecord move distance"),
		FMath::IsNearlyEqual(Report.MoveDistance, DamagedRecord.MoveDistance, 0.01f));
	TestTrue(TEXT("DroneReport copies server CombatRecord boss damage"),
		FMath::IsNearlyEqual(Report.BossDamage, DamagedRecord.BossDamage, 0.01f));
	TestEqual(TEXT("DroneReport copies server CombatRecord damage taken count"),
		Report.DamageTakenCount,
		DamagedRecord.DamageTakenCount);

	DestroyDroneSelectionTestContext(Context);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneServerMoveInputAuthorityTest,
	"DroneProto.D8.Drone.ServerMoveInputAuthority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneServerMoveInputAuthorityTest::RunTest(const FString& Parameters)
{
	FDroneSelectionTestContext Context = CreateDroneSelectionTestContext(TEXT("DroneServerMoveInputAuthorityWorld"));
	TestNotNull(TEXT("move input world is created"), Context.World);
	TestNotNull(TEXT("move input player controller is spawned"), Context.PC);
	TestNotNull(TEXT("move input drone is spawned"), Context.Drone);
	if (!Context.World || !Context.PC || !Context.Drone)
	{
		DestroyDroneSelectionTestContext(Context);
		return false;
	}

	TestEqual(TEXT("move input test starts Selecting"),
		Context.PC->GetCurrentSelectionState(),
		EPlayerSelectionState::Selecting);
	TestFalse(TEXT("Selecting pawn move input is ignored by server"),
		Context.Drone->ApplyMoveInputForServerForTest(FVector2D(1.0f, 0.0f)));
	TestTrue(TEXT("ignored move input keeps last accepted axis zero"),
		Context.Drone->GetLastServerMoveInputForTest().IsNearlyZero());

	Context.PC->Server_RequestReadyForRaid_Implementation();
	TestEqual(TEXT("ready player is InBattle before server move input"),
		Context.PC->GetCurrentSelectionState(),
		EPlayerSelectionState::InBattle);

	TestTrue(TEXT("InBattle alive pawn move input is accepted"),
		Context.Drone->ApplyMoveInputForServerForTest(FVector2D(3.0f, 4.0f)));
	TestTrue(TEXT("server clamps move input vector length to one"),
		FMath::IsNearlyEqual(Context.Drone->GetLastServerMoveInputForTest().Size(), 1.0f, 0.001f));
	const FVector LocationBeforeServerMove = Context.Drone->GetActorLocation();
	TestTrue(TEXT("pending accepted server move input is applied"),
		Context.Drone->ApplyPendingServerMoveInputForTest(0.10f));
	const FVector LocationAfterServerMove = Context.Drone->GetActorLocation();
	TestTrue(TEXT("accepted server move input changes drone location on server tick"),
		!LocationAfterServerMove.Equals(LocationBeforeServerMove, 0.1f));
	TestTrue(TEXT("accepted server move input is consumed after server tick"),
		Context.Drone->GetLastServerMoveInputForTest().IsNearlyZero());

	Context.PC->UnPossess();
	TestFalse(TEXT("unpossessed drone move input is ignored"),
		Context.Drone->ApplyMoveInputForServerForTest(FVector2D(1.0f, 0.0f)));
	TestTrue(TEXT("unpossessed ignored input clears last server axis"),
		Context.Drone->GetLastServerMoveInputForTest().IsNearlyZero());

	Context.PC->Possess(Context.Drone);
	Context.Drone->ApplyDamageForServer(Context.Drone->GetMaxHealth() + 10, FName(TEXT("Automation")));
	TestTrue(TEXT("move input test drone is dead"), Context.Drone->IsDead());
	TestFalse(TEXT("Dead pawn move input is ignored by server"),
		Context.Drone->ApplyMoveInputForServerForTest(FVector2D(1.0f, 0.0f)));

	DestroyDroneSelectionTestContext(Context);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneServerDodgeAuthorityTest,
	"DroneProto.D14.Drone.ServerDodgeAuthority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneServerDodgeAuthorityTest::RunTest(const FString& Parameters)
{
	FDroneSelectionTestContext Valid = CreateDroneSelectionTestContext(TEXT("DroneServerDodgeValidWorld"));
	TestNotNull(TEXT("valid dodge world is created"), Valid.World);
	TestNotNull(TEXT("valid dodge game state is spawned"), Valid.GameState);
	TestNotNull(TEXT("valid dodge player controller is spawned"), Valid.PC);
	TestNotNull(TEXT("valid dodge drone is spawned"), Valid.Drone);
	if (!Valid.World || !Valid.GameState || !Valid.PC || !Valid.Drone)
	{
		DestroyDroneSelectionTestContext(Valid);
		return false;
	}

	ARaidBoss* Boss = Valid.World->SpawnActor<ARaidBoss>();
	TestNotNull(TEXT("valid dodge boss is spawned"), Boss);
	if (Boss)
	{
		// POR-17: 보스가 드론과 같은 위치면 BossMinDistance 클램프가 dodge 시작 위치를 밀어낸다.
		Boss->SetActorLocation(FVector(-3000.0f, 0.0f, 0.0f));
		Valid.GameState->SetRaidBossForServer(Boss);
	}
	Valid.PC->Server_RequestReadyForRaid_Implementation();
	TestEqual(TEXT("valid dodge player is InBattle"),
		Valid.PC->GetCurrentSelectionState(),
		EPlayerSelectionState::InBattle);
	TestTrue(TEXT("valid dodge test loadout applies"),
		Valid.Drone->ApplyLoadout(NAME_None, ADronePartInventory::GetPulseLaserPartID(), NAME_None));
	Valid.PC->SetControlRotation(FRotator::ZeroRotator);
	Valid.Drone->SetActorLocation(FVector::ZeroVector);
	Valid.Drone->ResetMoveDistanceForServerForTest(FName(TEXT("Automation")));

	const FVector ValidLocationBefore = Valid.Drone->GetActorLocation();
	TestTrue(TEXT("InBattle valid direction dodge succeeds"),
		Valid.Drone->RequestDodgeForServer(FVector2D(1.0f, 0.0f)));
	// D19 시간보간: 위치/이동거리는 Drone Tick이 진행돼야 반영된다 (0.10s < DodgeDuration 0.25s).
	Valid.Drone->TickForTest(0.10f);
	const FVector ValidLocationAfter = Valid.Drone->GetActorLocation();
	TestTrue(TEXT("successful dodge changes server location"),
		!ValidLocationAfter.Equals(ValidLocationBefore, 0.1f));
	TestTrue(TEXT("dodge does not enqueue normal server move input"),
		Valid.Drone->GetLastServerMoveInputForTest().IsNearlyZero());
	TestTrue(TEXT("dodge adds actual distance to Vector move distance"),
		Valid.Drone->GetVectorAccumulatedMoveDistanceForTest() > 0.0f);
	TestTrue(TEXT("dodge adds actual distance to Booster move distance"),
		Valid.Drone->GetBoosterAccumulatedMoveDistanceForTest() > 0.0f);
	const float BossHPDuringDodgeAttack = Boss ? Boss->GetCurrentHP() : 0.0f;
	Valid.Drone->RequestAttackBoss();
	if (Boss)
	{
		TestEqual(TEXT("attack is blocked while dodge state is active"),
			Boss->GetCurrentHP(),
			BossHPDuringDodgeAttack);
		TickWorldForAutomationTest(Valid.World, 0.30f);
		const float BossHPBeforeAttack = Boss->GetCurrentHP();
		Valid.Drone->RequestAttackBoss();
		TestTrue(TEXT("attack becomes available again after dodge ends"),
			Boss->GetCurrentHP() < BossHPBeforeAttack);
	}
	DestroyDroneSelectionTestContext(Valid);

	FDroneSelectionTestContext ZeroDirection = CreateDroneSelectionTestContext(TEXT("DroneServerDodgeZeroDirectionWorld"));
	TestNotNull(TEXT("zero direction dodge world is created"), ZeroDirection.World);
	TestNotNull(TEXT("zero direction dodge player controller is spawned"), ZeroDirection.PC);
	TestNotNull(TEXT("zero direction dodge drone is spawned"), ZeroDirection.Drone);
	if (!ZeroDirection.World || !ZeroDirection.PC || !ZeroDirection.Drone)
	{
		DestroyDroneSelectionTestContext(ZeroDirection);
		return false;
	}

	ZeroDirection.PC->Server_RequestReadyForRaid_Implementation();
	const FVector ZeroDirectionLocationBefore = ZeroDirection.Drone->GetActorLocation();
	TestFalse(TEXT("zero direction dodge is ignored"),
		ZeroDirection.Drone->RequestDodgeForServer(FVector2D::ZeroVector));
	TestTrue(TEXT("zero direction dodge keeps server location"),
		ZeroDirection.Drone->GetActorLocation().Equals(ZeroDirectionLocationBefore, 0.1f));
	DestroyDroneSelectionTestContext(ZeroDirection);

	FDroneSelectionTestContext Selecting = CreateDroneSelectionTestContext(TEXT("DroneServerDodgeSelectingWorld"));
	TestNotNull(TEXT("selecting dodge world is created"), Selecting.World);
	TestNotNull(TEXT("selecting dodge player controller is spawned"), Selecting.PC);
	TestNotNull(TEXT("selecting dodge drone is spawned"), Selecting.Drone);
	if (!Selecting.World || !Selecting.PC || !Selecting.Drone)
	{
		DestroyDroneSelectionTestContext(Selecting);
		return false;
	}

	TestEqual(TEXT("selecting dodge test starts Selecting"),
		Selecting.PC->GetCurrentSelectionState(),
		EPlayerSelectionState::Selecting);
	TestFalse(TEXT("Selecting dodge is ignored"),
		Selecting.Drone->RequestDodgeForServer(FVector2D(1.0f, 0.0f)));
	DestroyDroneSelectionTestContext(Selecting);

	FDroneSelectionTestContext Dead = CreateDroneSelectionTestContext(TEXT("DroneServerDodgeDeadWorld"));
	TestNotNull(TEXT("dead dodge world is created"), Dead.World);
	TestNotNull(TEXT("dead dodge player controller is spawned"), Dead.PC);
	TestNotNull(TEXT("dead dodge drone is spawned"), Dead.Drone);
	if (!Dead.World || !Dead.PC || !Dead.Drone)
	{
		DestroyDroneSelectionTestContext(Dead);
		return false;
	}

	Dead.PC->Server_RequestReadyForRaid_Implementation();
	Dead.Drone->ApplyDamageForServer(Dead.Drone->GetMaxHealth() + 10, FName(TEXT("Automation")));
	TestTrue(TEXT("dead dodge setup kills the drone"), Dead.Drone->IsDead());
	TestFalse(TEXT("Dead dodge is ignored"),
		Dead.Drone->RequestDodgeForServer(FVector2D(1.0f, 0.0f)));
	DestroyDroneSelectionTestContext(Dead);

	FDroneSelectionTestContext RaidEnd = CreateDroneSelectionTestContext(TEXT("DroneServerDodgeRaidEndWorld"));
	TestNotNull(TEXT("raid end dodge world is created"), RaidEnd.World);
	TestNotNull(TEXT("raid end dodge game state is spawned"), RaidEnd.GameState);
	TestNotNull(TEXT("raid end dodge player controller is spawned"), RaidEnd.PC);
	TestNotNull(TEXT("raid end dodge drone is spawned"), RaidEnd.Drone);
	if (!RaidEnd.World || !RaidEnd.GameState || !RaidEnd.PC || !RaidEnd.Drone)
	{
		DestroyDroneSelectionTestContext(RaidEnd);
		return false;
	}

	RaidEnd.PC->Server_RequestReadyForRaid_Implementation();
	RaidEnd.GameState->SetRaidStateForServer(ERaidState::End);
	const FVector RaidEndLocationBefore = RaidEnd.Drone->GetActorLocation();
	TestFalse(TEXT("RaidEnd dodge is ignored"),
		RaidEnd.Drone->RequestDodgeForServer(FVector2D(1.0f, 0.0f)));
	TestTrue(TEXT("RaidEnd dodge keeps server location"),
		RaidEnd.Drone->GetActorLocation().Equals(RaidEndLocationBefore, 0.1f));
	DestroyDroneSelectionTestContext(RaidEnd);

	FDroneSelectionTestContext NoController = CreateDroneSelectionTestContext(TEXT("DroneServerDodgeNoControllerWorld"));
	TestNotNull(TEXT("no controller dodge world is created"), NoController.World);
	TestNotNull(TEXT("no controller dodge player controller is spawned"), NoController.PC);
	TestNotNull(TEXT("no controller dodge drone is spawned"), NoController.Drone);
	if (!NoController.World || !NoController.PC || !NoController.Drone)
	{
		DestroyDroneSelectionTestContext(NoController);
		return false;
	}

	NoController.PC->Server_RequestReadyForRaid_Implementation();
	NoController.PC->UnPossess();
	TestFalse(TEXT("NoController dodge is ignored"),
		NoController.Drone->RequestDodgeForServer(FVector2D(1.0f, 0.0f)));
	DestroyDroneSelectionTestContext(NoController);

	FDroneSelectionTestContext PossessMismatch = CreateDroneSelectionTestContext(TEXT("DroneServerDodgePossessMismatchWorld"));
	TestNotNull(TEXT("possess mismatch dodge world is created"), PossessMismatch.World);
	TestNotNull(TEXT("possess mismatch dodge player controller is spawned"), PossessMismatch.PC);
	TestNotNull(TEXT("possess mismatch dodge drone is spawned"), PossessMismatch.Drone);
	if (!PossessMismatch.World || !PossessMismatch.PC || !PossessMismatch.Drone)
	{
		DestroyDroneSelectionTestContext(PossessMismatch);
		return false;
	}

	PossessMismatch.PC->Server_RequestReadyForRaid_Implementation();
	ADrone* OtherDrone = PossessMismatch.World->SpawnActor<ADrone>();
	TestNotNull(TEXT("possess mismatch alternate drone is spawned"), OtherDrone);
	if (OtherDrone)
	{
		PossessMismatch.PC->Possess(OtherDrone);
		TestTrue(TEXT("possess mismatch test can set stale controller pointer"),
			SetPawnControllerForAutomationTest(PossessMismatch.Drone, PossessMismatch.PC));
		TestFalse(TEXT("PossessMismatch dodge is ignored"),
			PossessMismatch.Drone->RequestDodgeForServer(FVector2D(1.0f, 0.0f)));
	}
	DestroyDroneSelectionTestContext(PossessMismatch);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneDodgeInputBridgeTest,
	"DroneProto.D14_5.Drone.DodgeInputBridge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneDodgeInputBridgeTest::RunTest(const FString& Parameters)
{
	FDroneSelectionTestContext Context = CreateDroneSelectionTestContext(TEXT("DroneDodgeInputBridgeWorld"));
	TestNotNull(TEXT("dodge input bridge world is created"), Context.World);
	TestNotNull(TEXT("dodge input bridge game state is spawned"), Context.GameState);
	TestNotNull(TEXT("dodge input bridge player controller is spawned"), Context.PC);
	TestNotNull(TEXT("dodge input bridge drone is spawned"), Context.Drone);
	if (!Context.World || !Context.GameState || !Context.PC || !Context.Drone)
	{
		DestroyDroneSelectionTestContext(Context);
		return false;
	}

	Context.PC->Server_RequestReadyForRaid_Implementation();
	TestEqual(TEXT("dodge input bridge player is InBattle"),
		Context.PC->GetCurrentSelectionState(),
		EPlayerSelectionState::InBattle);
	Context.PC->SetControlRotation(FRotator::ZeroRotator);
	Context.Drone->SetActorLocation(FVector::ZeroVector);
	Context.Drone->ResetMoveDistanceForServerForTest(FName(TEXT("Automation")));

	TestTrue(TEXT("cache helper clamps large move input for dodge"),
		Context.Drone->CacheMoveInputForDodgeForTest(FVector2D(3.0f, 4.0f)));
	TestTrue(TEXT("cached dodge move input is normalized"),
		FMath::IsNearlyEqual(Context.Drone->GetCachedMoveInputForDodgeForTest().Size(), 1.0f, 0.001f));
	const FVector LocationBeforeDodge = Context.Drone->GetActorLocation();
	TestTrue(TEXT("dodge input bridge requests dodge from cached move axis"),
		Context.Drone->RequestDodgeFromCurrentMoveInputForTest());
	// D19 시간보간: 위치/이동거리는 Drone Tick이 진행돼야 반영된다.
	Context.Drone->TickForTest(0.10f);
	TestTrue(TEXT("dodge input bridge changes server location through D14 path"),
		!Context.Drone->GetActorLocation().Equals(LocationBeforeDodge, 0.1f));
	TestTrue(TEXT("dodge input bridge clears cached axis after request"),
		Context.Drone->GetCachedMoveInputForDodgeForTest().IsNearlyZero());
	TestTrue(TEXT("dodge input bridge adds dodge distance to Vector movement distance"),
		Context.Drone->GetVectorAccumulatedMoveDistanceForTest() > 0.0f);

	const FVector LocationBeforeZeroDodge = Context.Drone->GetActorLocation();
	Context.Drone->ClearMoveInputForDodgeForTest();
	TestFalse(TEXT("dodge input bridge ignores C without direction"),
		Context.Drone->RequestDodgeFromCurrentMoveInputForTest());
	TestTrue(TEXT("C without direction leaves server location unchanged"),
		Context.Drone->GetActorLocation().Equals(LocationBeforeZeroDodge, 0.1f));

	DestroyDroneSelectionTestContext(Context);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRaidBossSpecMaxHPTest,
	"DroneProto.Q1.RaidBoss.SpecMaxHP",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRaidBossSpecMaxHPTest::RunTest(const FString& Parameters)
{
	FDroneSelectionTestContext Context = CreateDroneSelectionTestContext(TEXT("RaidBossSpecMaxHPWorld"));
	ARaidBoss* Boss = Context.World ? Context.World->SpawnActor<ARaidBoss>() : nullptr;
	TestNotNull(TEXT("Q1 boss is spawned"), Boss);
	if (!Boss)
	{
		DestroyDroneSelectionTestContext(Context);
		return false;
	}

	TestTrue(TEXT("Q1 boss MaxHP matches current 60000 spec"),
		FMath::IsNearlyEqual(Boss->GetMaxHP(), 60000.0f, 0.01f));
	TestTrue(TEXT("Q1 boss CurrentHP initializes from MaxHP"),
		FMath::IsNearlyEqual(Boss->GetCurrentHP(), Boss->GetMaxHP(), 0.01f));

	const float HPBeforeDamage = Boss->GetCurrentHP();
	Boss->ApplyDamageForServer(1000.0f, Context.PC, Context.Drone);
	TestTrue(TEXT("Q1 1000 damage is chip damage against 60000 HP boss"),
		FMath::IsNearlyEqual(Boss->GetCurrentHP(), HPBeforeDamage - 1000.0f, 0.01f));

	DestroyDroneSelectionTestContext(Context);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRaidBossDebugAreaAttackTest,
	"DroneProto.D15.RaidBoss.DebugAreaAttack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRaidBossDebugAreaAttackTest::RunTest(const FString& Parameters)
{
	FDroneSelectionTestContext HitContext = CreateDroneSelectionTestContext(TEXT("BossAreaAttackHitWorld"));
	ARaidBoss* HitBoss = nullptr;
	if (!PrepareBattleAttackTest(*this, HitContext, HitBoss, TEXT("boss area hit")))
	{
		DestroyDroneSelectionTestContext(HitContext);
		return false;
	}

	HitContext.Drone->SetActorLocation(FVector::ZeroVector);
	const int32 HitHealthBefore = HitContext.Drone->GetHealth();
	TestEqual(TEXT("InBattle drone inside area is hit once"),
		HitBoss->PerformDebugAreaAttackForServer(FVector::ZeroVector, 150.0f, 25),
		1);
	TestEqual(TEXT("boss area hit reduces drone HP"),
		HitContext.Drone->GetHealth(),
		HitHealthBefore - 25);
	TestEqual(TEXT("boss area hit increments DamageTakenCount"),
		HitContext.Drone->GetCombatRecordForTest().DamageTakenCount,
		1);
	DestroyDroneSelectionTestContext(HitContext);

	FDroneSelectionTestContext MissContext = CreateDroneSelectionTestContext(TEXT("BossAreaAttackMissWorld"));
	ARaidBoss* MissBoss = nullptr;
	if (!PrepareBattleAttackTest(*this, MissContext, MissBoss, TEXT("boss area miss")))
	{
		DestroyDroneSelectionTestContext(MissContext);
		return false;
	}

	MissContext.Drone->SetActorLocation(FVector(500.0f, 0.0f, 0.0f));
	const int32 MissHealthBefore = MissContext.Drone->GetHealth();
	TestEqual(TEXT("InBattle drone outside area is missed"),
		MissBoss->PerformDebugAreaAttackForServer(FVector::ZeroVector, 100.0f, 25),
		0);
	TestEqual(TEXT("boss area miss leaves drone HP unchanged"),
		MissContext.Drone->GetHealth(),
		MissHealthBefore);
	TestEqual(TEXT("boss area miss leaves DamageTakenCount unchanged"),
		MissContext.Drone->GetCombatRecordForTest().DamageTakenCount,
		0);
	DestroyDroneSelectionTestContext(MissContext);

	FDroneSelectionTestContext DodgeContext = CreateDroneSelectionTestContext(TEXT("BossAreaAttackDodgeWorld"));
	ARaidBoss* DodgeBoss = nullptr;
	if (!PrepareBattleAttackTest(*this, DodgeContext, DodgeBoss, TEXT("boss area dodge")))
	{
		DestroyDroneSelectionTestContext(DodgeContext);
		return false;
	}

	DodgeContext.PC->SetControlRotation(FRotator::ZeroRotator);
	DodgeContext.Drone->SetActorLocation(FVector::ZeroVector);
	const int32 DodgeHealthBefore = DodgeContext.Drone->GetHealth();
	TestTrue(TEXT("server dodge moves drone before boss area attack"),
		DodgeContext.Drone->RequestDodgeForServer(FVector2D(1.0f, 0.0f)));
	TestTrue(TEXT("dodge places drone outside small attack radius"),
		FVector::Dist2D(DodgeContext.Drone->GetActorLocation(), FVector::ZeroVector) > 100.0f);
	TestEqual(TEXT("boss area attack after dodge is a miss"),
		DodgeBoss->PerformDebugAreaAttackForServer(FVector::ZeroVector, 100.0f, 25),
		0);
	TestEqual(TEXT("dodge miss leaves HP unchanged"),
		DodgeContext.Drone->GetHealth(),
		DodgeHealthBefore);
	DestroyDroneSelectionTestContext(DodgeContext);

	UDronePartReturnManager* DeathReturnManager = nullptr;
	FDroneSelectionTestContext DeathContext = CreateDroneReturnTestContext(TEXT("BossAreaAttackDeathWorld"), DeathReturnManager);
	TestNotNull(TEXT("death boss area world is created"), DeathContext.World);
	TestNotNull(TEXT("death boss area inventory is spawned"), DeathContext.Inventory);
	TestNotNull(TEXT("death boss area player controller is spawned"), DeathContext.PC);
	TestNotNull(TEXT("death boss area drone is spawned"), DeathContext.Drone);
	TestNotNull(TEXT("death boss area return manager is created"), DeathReturnManager);
	if (!DeathContext.World || !DeathContext.Inventory || !DeathContext.PC || !DeathContext.Drone || !DeathReturnManager)
	{
		DestroyDroneSelectionTestContext(DeathContext);
		return false;
	}

	ARaidBoss* DeathBoss = DeathContext.World->SpawnActor<ARaidBoss>();
	TestNotNull(TEXT("death boss area boss is spawned"), DeathBoss);
	if (!DeathBoss)
	{
		DestroyDroneSelectionTestContext(DeathContext);
		return false;
	}
	if (DeathContext.GameState)
	{
		DeathContext.GameState->SetRaidBossForServer(DeathBoss);
	}

	const FName CoreZenith = ADronePartInventory::GetCoreZenithPartID();
	const FName PulseLaser = ADronePartInventory::GetPulseLaserPartID();
	TestTrue(TEXT("death boss area consumes selected core"), DeathContext.Inventory->TryConsumePart(CoreZenith));
	TestTrue(TEXT("death boss area consumes selected left weapon"), DeathContext.Inventory->TryConsumePart(PulseLaser));
	DeathContext.PC->SetSelectedPartIDForSlotForServer(EPartSlot::Core, CoreZenith);
	DeathContext.PC->SetSelectedPartIDForSlotForServer(EPartSlot::LeftWeapon, PulseLaser);
	DeathContext.PC->Server_RequestReadyForRaid_Implementation();
	DeathContext.Drone->SetActorLocation(FVector::ZeroVector);

	TestEqual(TEXT("lethal boss area attack hits one drone"),
		DeathBoss->PerformDebugAreaAttackForServer(FVector::ZeroVector, 150.0f, DeathContext.Drone->GetMaxHealth() + 10),
		1);
	TestTrue(TEXT("lethal boss area attack marks drone dead"), DeathContext.Drone->IsDead());
	TestTrue(TEXT("lethal boss area attack creates death report"), DeathContext.PC->HasDroneReportGeneratedForTest());
	TestEqual(TEXT("death report records damage taken"),
		DeathContext.PC->GetLastDroneReportDataForTest().DamageTakenCount,
		1);
	TestFalse(TEXT("death report does not grant NoDamage after boss hit"),
		DeathContext.PC->GetLastDroneReportDataForTest().AchievedBonusList.Contains(EDroneReportBonusType::NoDamage));
	TestEqual(TEXT("death return clears equipped core"),
		DeathContext.PC->GetEquippedPartIDBySlot(EPartSlot::Core),
		NAME_None);
	TestEqual(TEXT("death return clears equipped left weapon"),
		DeathContext.PC->GetEquippedPartIDBySlot(EPartSlot::LeftWeapon),
		NAME_None);
	TestEqual(TEXT("death return restores core stock"),
		DeathContext.Inventory->GetCurrentCount(CoreZenith),
		DeathContext.Inventory->GetMaxCount(CoreZenith));
	TestEqual(TEXT("death return restores left weapon stock"),
		DeathContext.Inventory->GetCurrentCount(PulseLaser),
		DeathContext.Inventory->GetMaxCount(PulseLaser));
	TestEqual(TEXT("dead drone ignores later boss area damage"),
		DeathBoss->PerformDebugAreaAttackForServer(FVector::ZeroVector, 150.0f, 25),
		0);
	TestEqual(TEXT("dead drone HP stays zero after later boss area attack"),
		DeathContext.Drone->GetHealth(),
		0);
	DestroyDroneSelectionTestContext(DeathContext);

	FDroneSelectionTestContext SelectingContext = CreateDroneSelectionTestContext(TEXT("BossAreaAttackSelectingWorld"));
	TestNotNull(TEXT("selecting boss area world is created"), SelectingContext.World);
	TestNotNull(TEXT("selecting boss area player controller is spawned"), SelectingContext.PC);
	TestNotNull(TEXT("selecting boss area drone is spawned"), SelectingContext.Drone);
	if (!SelectingContext.World || !SelectingContext.PC || !SelectingContext.Drone)
	{
		DestroyDroneSelectionTestContext(SelectingContext);
		return false;
	}
	ARaidBoss* SelectingBoss = SelectingContext.World->SpawnActor<ARaidBoss>();
	TestNotNull(TEXT("selecting boss area boss is spawned"), SelectingBoss);
	if (SelectingBoss && SelectingContext.GameState)
	{
		SelectingContext.GameState->SetRaidBossForServer(SelectingBoss);
	}
	const int32 SelectingHealthBefore = SelectingContext.Drone->GetHealth();
	TestEqual(TEXT("NotInBattle drone is excluded from boss area attack"),
		SelectingBoss ? SelectingBoss->PerformDebugAreaAttackForServer(FVector::ZeroVector, 150.0f, 25) : -1,
		0);
	TestEqual(TEXT("NotInBattle exclusion leaves HP unchanged"),
		SelectingContext.Drone->GetHealth(),
		SelectingHealthBefore);
	DestroyDroneSelectionTestContext(SelectingContext);

	FDroneSelectionTestContext RaidEndContext = CreateDroneSelectionTestContext(TEXT("BossAreaAttackRaidEndWorld"));
	ARaidBoss* RaidEndBoss = nullptr;
	if (!PrepareBattleAttackTest(*this, RaidEndContext, RaidEndBoss, TEXT("boss area raid end")))
	{
		DestroyDroneSelectionTestContext(RaidEndContext);
		return false;
	}
	RaidEndContext.GameState->SetRaidStateForServer(ERaidState::End);
	const int32 RaidEndHealthBefore = RaidEndContext.Drone->GetHealth();
	TestEqual(TEXT("RaidEnd ignores boss area attack"),
		RaidEndBoss->PerformDebugAreaAttackForServer(FVector::ZeroVector, 150.0f, 25),
		0);
	TestEqual(TEXT("RaidEnd ignored boss area attack leaves HP unchanged"),
		RaidEndContext.Drone->GetHealth(),
		RaidEndHealthBefore);
	DestroyDroneSelectionTestContext(RaidEndContext);

	FDroneSelectionTestContext DeadBossContext = CreateDroneSelectionTestContext(TEXT("BossAreaAttackDeadBossWorld"));
	ARaidBoss* DeadBoss = nullptr;
	if (!PrepareBattleAttackTest(*this, DeadBossContext, DeadBoss, TEXT("dead boss area attack")))
	{
		DestroyDroneSelectionTestContext(DeadBossContext);
		return false;
	}
	DeadBoss->ApplyDamageForServer(DeadBoss->GetMaxHP() + 100.0f, DeadBossContext.PC, DeadBossContext.Drone);
	const int32 DeadBossHealthBefore = DeadBossContext.Drone->GetHealth();
	TestEqual(TEXT("BossDead ignores boss area attack"),
		DeadBoss->PerformDebugAreaAttackForServer(FVector::ZeroVector, 150.0f, 25),
		0);
	TestEqual(TEXT("BossDead ignored boss area attack leaves drone HP unchanged"),
		DeadBossContext.Drone->GetHealth(),
		DeadBossHealthBefore);
	DestroyDroneSelectionTestContext(DeadBossContext);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRaidBossTelegraphedAreaAttackTest,
	"DroneProto.D15.RaidBoss.TelegraphedAreaAttack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRaidBossTelegraphedAreaAttackTest::RunTest(const FString& Parameters)
{
	FDroneSelectionTestContext DelayedHitContext = CreateDroneSelectionTestContext(TEXT("BossTelegraphDelayedHitWorld"));
	ARaidBoss* DelayedHitBoss = nullptr;
	if (!PrepareBattleAttackTest(*this, DelayedHitContext, DelayedHitBoss, TEXT("boss telegraph delayed hit")))
	{
		DestroyDroneSelectionTestContext(DelayedHitContext);
		return false;
	}

	DelayedHitContext.Drone->SetActorLocation(FVector::ZeroVector);
	const int32 DelayedHitHealthBefore = DelayedHitContext.Drone->GetHealth();
	TestTrue(TEXT("telegraphed boss attack starts from server"),
		DelayedHitBoss->StartDebugTelegraphedAreaAttackForServer(FVector::ZeroVector, 150.0f, 25, 0.10f));
	TestEqual(TEXT("telegraphed boss attack spawns one replicated marker immediately"),
		CountBossTelegraphsForAutomationTest(DelayedHitContext.World),
		1);
	ARaidBossAttackTelegraph* DelayedTelegraph = FindBossTelegraphForAutomationTest(DelayedHitContext.World);
	TestNotNull(TEXT("telegraphed boss attack marker has an actor instance"), DelayedTelegraph);
	TestNotNull(TEXT("telegraphed boss attack marker has a stable root component"),
		DelayedTelegraph ? DelayedTelegraph->GetRootComponent() : nullptr);
	TestNotNull(TEXT("telegraphed boss attack marker has an optional mesh component for BP children"),
		DelayedTelegraph ? DelayedTelegraph->FindComponentByClass<UStaticMeshComponent>() : nullptr);
	TestNotNull(TEXT("telegraphed boss attack marker has a visible C++ text marker"),
		DelayedTelegraph ? DelayedTelegraph->FindComponentByClass<UTextRenderComponent>() : nullptr);
	TestTrue(TEXT("telegraphed boss attack marker is replicated"),
		DelayedTelegraph && DelayedTelegraph->GetIsReplicated());
	TestTrue(TEXT("telegraphed boss attack marker scales from radius"),
		DelayedTelegraph && DelayedTelegraph->GetActorScale3D().Equals(FVector(1.5f, 1.5f, 1.0f), 0.01f));
	TestTrue(TEXT("telegraphed boss attack marker lifespan covers the delay"),
		DelayedTelegraph && DelayedTelegraph->GetLifeSpan() >= 0.10f);
	TestEqual(TEXT("telegraphed boss attack does not damage before delay"),
		DelayedHitContext.Drone->GetHealth(),
		DelayedHitHealthBefore);
	TickWorldForAutomationTest(DelayedHitContext.World, 0.15f);
	TestEqual(TEXT("telegraphed boss attack damages after delay"),
		DelayedHitContext.Drone->GetHealth(),
		DelayedHitHealthBefore - 25);
	TestEqual(TEXT("telegraphed boss attack increments DamageTakenCount after delay"),
		DelayedHitContext.Drone->GetCombatRecordForTest().DamageTakenCount,
		1);
	TestEqual(TEXT("telegraph marker is destroyed after execution"),
		CountBossTelegraphsForAutomationTest(DelayedHitContext.World),
		0);
	DestroyDroneSelectionTestContext(DelayedHitContext);

	FDroneSelectionTestContext DebugTriggerContext = CreateDroneSelectionTestContext(TEXT("BossTelegraphDebugTriggerWorld"));
	ARaidBoss* DebugTriggerBoss = nullptr;
	if (!PrepareBattleAttackTest(*this, DebugTriggerContext, DebugTriggerBoss, TEXT("boss telegraph debug trigger")))
	{
		DestroyDroneSelectionTestContext(DebugTriggerContext);
		return false;
	}

	DebugTriggerContext.Drone->SetActorLocation(FVector(120.0f, 30.0f, 0.0f));
	const int32 DebugTriggerHealthBefore = DebugTriggerContext.Drone->GetHealth();
	DebugTriggerContext.PC->DebugTriggerBossTelegraphAttack(150.0f, 25, 0.10f, 0.0f);
	TestEqual(TEXT("debug trigger spawns one telegraph through the server path"),
		CountBossTelegraphsForAutomationTest(DebugTriggerContext.World),
		1);
	ARaidBossAttackTelegraph* DebugTriggerTelegraph = FindBossTelegraphForAutomationTest(DebugTriggerContext.World);
	TestTrue(TEXT("debug trigger uses the pawn location by default"),
		DebugTriggerTelegraph && DebugTriggerTelegraph->GetCenter().Equals(DebugTriggerContext.Drone->GetActorLocation(), 0.1f));
	TestEqual(TEXT("debug trigger does not damage before delay"),
		DebugTriggerContext.Drone->GetHealth(),
		DebugTriggerHealthBefore);
	TickWorldForAutomationTest(DebugTriggerContext.World, 0.15f);
	TestEqual(TEXT("debug trigger executes existing boss attack after delay"),
		DebugTriggerContext.Drone->GetHealth(),
		DebugTriggerHealthBefore - 25);
	DestroyDroneSelectionTestContext(DebugTriggerContext);

	FDroneSelectionTestContext DebugNoBossContext = CreateDroneSelectionTestContext(TEXT("BossTelegraphDebugNoBossWorld"));
	TestNotNull(TEXT("debug no boss world is created"), DebugNoBossContext.World);
	TestNotNull(TEXT("debug no boss player controller is spawned"), DebugNoBossContext.PC);
	TestNotNull(TEXT("debug no boss drone is spawned"), DebugNoBossContext.Drone);
	if (!DebugNoBossContext.World || !DebugNoBossContext.PC || !DebugNoBossContext.Drone)
	{
		DestroyDroneSelectionTestContext(DebugNoBossContext);
		return false;
	}
	DebugNoBossContext.PC->Server_RequestReadyForRaid_Implementation();
	const int32 DebugNoBossHealthBefore = DebugNoBossContext.Drone->GetHealth();
	DebugNoBossContext.PC->DebugTriggerBossTelegraphAttack(150.0f, 25, 0.10f, 0.0f);
	TestEqual(TEXT("debug trigger ignores missing boss without spawning telegraph"),
		CountBossTelegraphsForAutomationTest(DebugNoBossContext.World),
		0);
	TickWorldForAutomationTest(DebugNoBossContext.World, 0.15f);
	TestEqual(TEXT("debug trigger missing boss leaves HP unchanged"),
		DebugNoBossContext.Drone->GetHealth(),
		DebugNoBossHealthBefore);
	DestroyDroneSelectionTestContext(DebugNoBossContext);

	FDroneSelectionTestContext DebugNotInBattleContext = CreateDroneSelectionTestContext(TEXT("BossTelegraphDebugNotInBattleWorld"));
	TestNotNull(TEXT("debug not in battle world is created"), DebugNotInBattleContext.World);
	TestNotNull(TEXT("debug not in battle player controller is spawned"), DebugNotInBattleContext.PC);
	TestNotNull(TEXT("debug not in battle drone is spawned"), DebugNotInBattleContext.Drone);
	if (!DebugNotInBattleContext.World || !DebugNotInBattleContext.PC || !DebugNotInBattleContext.Drone)
	{
		DestroyDroneSelectionTestContext(DebugNotInBattleContext);
		return false;
	}
	ARaidBoss* DebugNotInBattleBoss = DebugNotInBattleContext.World->SpawnActor<ARaidBoss>();
	TestNotNull(TEXT("debug not in battle boss is spawned"), DebugNotInBattleBoss);
	if (DebugNotInBattleBoss && DebugNotInBattleContext.GameState)
	{
		DebugNotInBattleContext.GameState->SetRaidBossForServer(DebugNotInBattleBoss);
	}
	const int32 DebugNotInBattleHealthBefore = DebugNotInBattleContext.Drone->GetHealth();
	DebugNotInBattleContext.PC->DebugTriggerBossTelegraphAttack(150.0f, 25, 0.10f, 0.0f);
	TestEqual(TEXT("debug trigger ignores NotInBattle without spawning telegraph"),
		CountBossTelegraphsForAutomationTest(DebugNotInBattleContext.World),
		0);
	TickWorldForAutomationTest(DebugNotInBattleContext.World, 0.15f);
	TestEqual(TEXT("debug trigger NotInBattle leaves HP unchanged"),
		DebugNotInBattleContext.Drone->GetHealth(),
		DebugNotInBattleHealthBefore);
	DestroyDroneSelectionTestContext(DebugNotInBattleContext);

	FDroneSelectionTestContext DodgeMissContext = CreateDroneSelectionTestContext(TEXT("BossTelegraphDodgeMissWorld"));
	ARaidBoss* DodgeMissBoss = nullptr;
	if (!PrepareBattleAttackTest(*this, DodgeMissContext, DodgeMissBoss, TEXT("boss telegraph dodge miss")))
	{
		DestroyDroneSelectionTestContext(DodgeMissContext);
		return false;
	}

	DodgeMissContext.PC->SetControlRotation(FRotator::ZeroRotator);
	// POR-17: 보스가 원점에 있으면 BossMinDistance 클램프가 dodge 시작 위치를 밀어내
	// 무적 판정 반경(100cm) 밖으로 이탈한다. 보스를 떨어뜨려 기존 검증 기하를 유지한다.
	DodgeMissBoss->SetActorLocation(FVector(-3000.0f, 0.0f, 0.0f));
	DodgeMissContext.Drone->SetActorLocation(FVector::ZeroVector);
	const int32 DodgeMissHealthBefore = DodgeMissContext.Drone->GetHealth();
	TestTrue(TEXT("telegraphed dodge miss attack starts"),
		DodgeMissBoss->StartDebugTelegraphedAreaAttackForServer(FVector::ZeroVector, 100.0f, 25, 0.10f));
	TestEqual(TEXT("telegraphed dodge miss does not damage immediately"),
		DodgeMissContext.Drone->GetHealth(),
		DodgeMissHealthBefore);
	TestTrue(TEXT("server dodge before telegraph expiry succeeds"),
		DodgeMissContext.Drone->RequestDodgeForServer(FVector2D(1.0f, 0.0f)));
	TestTrue(TEXT("dodge invincibility is active before delayed boss attack"),
		DodgeMissContext.Drone->IsInvincibleForTest());
	const int32 DodgeMissIgnoredVisualBefore = DodgeMissContext.Drone->GetCombatVisualDroneDamageIgnoredCountForTest();
	TickWorldForAutomationTest(DodgeMissContext.World, 0.15f);
	TestEqual(TEXT("dodge invincibility keeps delayed area attack from damaging"),
		DodgeMissContext.Drone->GetHealth(),
		DodgeMissHealthBefore);
	TestEqual(TEXT("delayed invincible dodge hit leaves DamageTakenCount unchanged"),
		DodgeMissContext.Drone->GetCombatRecordForTest().DamageTakenCount,
		0);
	TestEqual(TEXT("delayed invincible dodge hit fires ignored visual event"),
		DodgeMissContext.Drone->GetCombatVisualDroneDamageIgnoredCountForTest(),
		DodgeMissIgnoredVisualBefore + 1);
	TestEqual(TEXT("delayed invincible dodge hit records ignored reason"),
		DodgeMissContext.Drone->GetLastCombatVisualDroneDamageIgnoredReasonForTest(),
		FName(TEXT("Invincible")));
	TestEqual(TEXT("dodge miss telegraph is destroyed after execution"),
		CountBossTelegraphsForAutomationTest(DodgeMissContext.World),
		0);
	DestroyDroneSelectionTestContext(DodgeMissContext);

	UDronePartReturnManager* DeathReturnManager = nullptr;
	FDroneSelectionTestContext DeathContext = CreateDroneReturnTestContext(TEXT("BossTelegraphDeathWorld"), DeathReturnManager);
	TestNotNull(TEXT("death telegraph world is created"), DeathContext.World);
	TestNotNull(TEXT("death telegraph inventory is spawned"), DeathContext.Inventory);
	TestNotNull(TEXT("death telegraph player controller is spawned"), DeathContext.PC);
	TestNotNull(TEXT("death telegraph drone is spawned"), DeathContext.Drone);
	TestNotNull(TEXT("death telegraph return manager is created"), DeathReturnManager);
	if (!DeathContext.World || !DeathContext.Inventory || !DeathContext.PC || !DeathContext.Drone || !DeathReturnManager)
	{
		DestroyDroneSelectionTestContext(DeathContext);
		return false;
	}

	ARaidBoss* DeathBoss = DeathContext.World->SpawnActor<ARaidBoss>();
	TestNotNull(TEXT("death telegraph boss is spawned"), DeathBoss);
	if (!DeathBoss)
	{
		DestroyDroneSelectionTestContext(DeathContext);
		return false;
	}
	if (DeathContext.GameState)
	{
		DeathContext.GameState->SetRaidBossForServer(DeathBoss);
	}

	const FName CoreZenith = ADronePartInventory::GetCoreZenithPartID();
	const FName PulseLaser = ADronePartInventory::GetPulseLaserPartID();
	TestTrue(TEXT("death telegraph consumes selected core"), DeathContext.Inventory->TryConsumePart(CoreZenith));
	TestTrue(TEXT("death telegraph consumes selected left weapon"), DeathContext.Inventory->TryConsumePart(PulseLaser));
	DeathContext.PC->SetSelectedPartIDForSlotForServer(EPartSlot::Core, CoreZenith);
	DeathContext.PC->SetSelectedPartIDForSlotForServer(EPartSlot::LeftWeapon, PulseLaser);
	DeathContext.PC->Server_RequestReadyForRaid_Implementation();
	DeathContext.Drone->SetActorLocation(FVector::ZeroVector);

	TestTrue(TEXT("lethal telegraphed boss attack starts"),
		DeathBoss->StartDebugTelegraphedAreaAttackForServer(FVector::ZeroVector, 150.0f, DeathContext.Drone->GetMaxHealth() + 10, 0.05f));
	TestFalse(TEXT("lethal telegraphed attack does not kill before delay"),
		DeathContext.Drone->IsDead());
	TickWorldForAutomationTest(DeathContext.World, 0.10f);
	TestTrue(TEXT("lethal telegraphed boss attack marks drone dead after delay"), DeathContext.Drone->IsDead());
	TestTrue(TEXT("lethal telegraphed boss attack creates death report"), DeathContext.PC->HasDroneReportGeneratedForTest());
	TestEqual(TEXT("lethal telegraph death report records damage taken"),
		DeathContext.PC->GetLastDroneReportDataForTest().DamageTakenCount,
		1);
	TestEqual(TEXT("lethal telegraph death return clears equipped core"),
		DeathContext.PC->GetEquippedPartIDBySlot(EPartSlot::Core),
		NAME_None);
	TestEqual(TEXT("lethal telegraph death return restores core stock"),
		DeathContext.Inventory->GetCurrentCount(CoreZenith),
		DeathContext.Inventory->GetMaxCount(CoreZenith));
	DestroyDroneSelectionTestContext(DeathContext);

	FDroneSelectionTestContext RaidEndContext = CreateDroneSelectionTestContext(TEXT("BossTelegraphRaidEndWorld"));
	ARaidBoss* RaidEndBoss = nullptr;
	if (!PrepareBattleAttackTest(*this, RaidEndContext, RaidEndBoss, TEXT("boss telegraph raid end")))
	{
		DestroyDroneSelectionTestContext(RaidEndContext);
		return false;
	}
	RaidEndContext.GameState->SetRaidStateForServer(ERaidState::End);
	const int32 RaidEndHealthBefore = RaidEndContext.Drone->GetHealth();
	TestFalse(TEXT("RaidEnd ignores telegraphed boss area attack"),
		RaidEndBoss->StartDebugTelegraphedAreaAttackForServer(FVector::ZeroVector, 150.0f, 25, 0.10f));
	TestEqual(TEXT("RaidEnd ignored telegraph does not spawn marker"),
		CountBossTelegraphsForAutomationTest(RaidEndContext.World),
		0);
	TickWorldForAutomationTest(RaidEndContext.World, 0.15f);
	TestEqual(TEXT("RaidEnd ignored telegraph leaves HP unchanged"),
		RaidEndContext.Drone->GetHealth(),
		RaidEndHealthBefore);
	DestroyDroneSelectionTestContext(RaidEndContext);

	FDroneSelectionTestContext DeadBossContext = CreateDroneSelectionTestContext(TEXT("BossTelegraphDeadBossWorld"));
	ARaidBoss* DeadBoss = nullptr;
	if (!PrepareBattleAttackTest(*this, DeadBossContext, DeadBoss, TEXT("dead boss telegraph attack")))
	{
		DestroyDroneSelectionTestContext(DeadBossContext);
		return false;
	}
	DeadBoss->ApplyDamageForServer(DeadBoss->GetMaxHP() + 100.0f, DeadBossContext.PC, DeadBossContext.Drone);
	const int32 DeadBossHealthBefore = DeadBossContext.Drone->GetHealth();
	TestFalse(TEXT("BossDead ignores telegraphed boss area attack"),
		DeadBoss->StartDebugTelegraphedAreaAttackForServer(FVector::ZeroVector, 150.0f, 25, 0.10f));
	TestEqual(TEXT("BossDead ignored telegraph does not spawn marker"),
		CountBossTelegraphsForAutomationTest(DeadBossContext.World),
		0);
	TickWorldForAutomationTest(DeadBossContext.World, 0.15f);
	TestEqual(TEXT("BossDead ignored telegraph leaves HP unchanged"),
		DeadBossContext.Drone->GetHealth(),
		DeadBossHealthBefore);
	DestroyDroneSelectionTestContext(DeadBossContext);

	FDroneSelectionTestContext InvalidContext = CreateDroneSelectionTestContext(TEXT("BossTelegraphInvalidWorld"));
	ARaidBoss* InvalidBoss = nullptr;
	if (!PrepareBattleAttackTest(*this, InvalidContext, InvalidBoss, TEXT("invalid boss telegraph attack")))
	{
		DestroyDroneSelectionTestContext(InvalidContext);
		return false;
	}
	const int32 InvalidHealthBefore = InvalidContext.Drone->GetHealth();
	TestFalse(TEXT("invalid radius ignores telegraphed boss area attack"),
		InvalidBoss->StartDebugTelegraphedAreaAttackForServer(FVector::ZeroVector, 0.0f, 25, 0.10f));
	TestFalse(TEXT("invalid damage ignores telegraphed boss area attack"),
		InvalidBoss->StartDebugTelegraphedAreaAttackForServer(FVector::ZeroVector, 150.0f, 0, 0.10f));
	TestFalse(TEXT("invalid delay ignores telegraphed boss area attack"),
		InvalidBoss->StartDebugTelegraphedAreaAttackForServer(FVector::ZeroVector, 150.0f, 25, 0.0f));
	TestEqual(TEXT("invalid telegraph requests do not spawn markers"),
		CountBossTelegraphsForAutomationTest(InvalidContext.World),
		0);
	TickWorldForAutomationTest(InvalidContext.World, 0.15f);
	TestEqual(TEXT("invalid telegraph requests leave HP unchanged"),
		InvalidContext.Drone->GetHealth(),
		InvalidHealthBefore);
	DestroyDroneSelectionTestContext(InvalidContext);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneCombatVisualHooksTest,
	"DroneProto.D19.CombatVisual.Hooks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneCombatVisualHooksTest::RunTest(const FString& Parameters)
{
	TestNotNull(TEXT("drone attack visual multicast exists"),
		ADrone::StaticClass()->FindFunctionByName(TEXT("Multicast_PlayDroneAttackVisual")));
	TestNotNull(TEXT("drone attack visual Blueprint hook exists"),
		ADrone::StaticClass()->FindFunctionByName(TEXT("BP_OnDroneAttackVisual")));
	TestNotNull(TEXT("drone damaged visual Blueprint hook exists"),
		ADrone::StaticClass()->FindFunctionByName(TEXT("BP_OnDroneDamagedVisual")));
	TestNotNull(TEXT("drone damage ignored visual Blueprint hook exists"),
		ADrone::StaticClass()->FindFunctionByName(TEXT("BP_OnDroneDamageIgnoredVisual")));
	TestNotNull(TEXT("boss damaged visual Blueprint hook exists"),
		ARaidBoss::StaticClass()->FindFunctionByName(TEXT("BP_OnBossDamagedVisual")));
	TestNotNull(TEXT("telegraph start visual Blueprint hook exists"),
		ARaidBossAttackTelegraph::StaticClass()->FindFunctionByName(TEXT("BP_OnTelegraphStartedVisual")));
	TestNotNull(TEXT("telegraph end visual Blueprint hook exists"),
		ARaidBossAttackTelegraph::StaticClass()->FindFunctionByName(TEXT("BP_OnTelegraphEndedVisual")));

	FDroneSelectionTestContext AttackContext = CreateDroneSelectionTestContext(TEXT("CombatVisualAttackWorld"));
	ARaidBoss* AttackBoss = nullptr;
	if (!PrepareBattleAttackTest(*this, AttackContext, AttackBoss, TEXT("combat visual attack")))
	{
		DestroyDroneSelectionTestContext(AttackContext);
		return false;
	}

	// POR-16/17: 보스와 드론이 원점에 겹치면 attack visual 궤적 From/To가 동일해진다.
	AttackBoss->SetActorLocation(FVector(-2000.0f, 0.0f, 0.0f));
	TestTrue(TEXT("combat visual attack loadout applies"),
		AttackContext.Drone->ApplyLoadout(NAME_None, ADronePartInventory::GetPulseLaserPartID(), NAME_None));
	const int32 AttackVisualCountBefore = AttackContext.Drone->GetCombatVisualAttackCountForTest();
	const int32 BossDamagedVisualCountBefore = AttackBoss->GetCombatVisualBossDamagedCountForTest();
	const float BossHPBeforeAttack = AttackBoss->GetCurrentHP();
	const float DamageDealt = AttackBossAndMeasureDamage(AttackContext.Drone, AttackBoss);
	TestTrue(TEXT("combat visual attack test deals damage"),
		DamageDealt > 0.0f);
	TestEqual(TEXT("attack visual event fires once after server damage"),
		AttackContext.Drone->GetCombatVisualAttackCountForTest(),
		AttackVisualCountBefore + 1);
	TestTrue(TEXT("attack visual carries actual dealt damage"),
		FMath::IsNearlyEqual(AttackContext.Drone->GetLastCombatVisualAttackDamageForTest(), DamageDealt, 0.001f));
	TestFalse(TEXT("attack visual has a non-empty trajectory"),
		AttackContext.Drone->GetLastCombatVisualAttackFromForTest().Equals(
			AttackContext.Drone->GetLastCombatVisualAttackToForTest(),
			0.1f));
	TestEqual(TEXT("boss damaged visual event fires once after boss damage"),
		AttackBoss->GetCombatVisualBossDamagedCountForTest(),
		BossDamagedVisualCountBefore + 1);
	TestTrue(TEXT("boss damaged visual carries damage"),
		FMath::IsNearlyEqual(AttackBoss->GetLastCombatVisualBossDamageForTest(), DamageDealt, 0.001f));
	TestTrue(TEXT("boss damaged visual carries old HP"),
		FMath::IsNearlyEqual(AttackBoss->GetLastCombatVisualBossOldHPForTest(), BossHPBeforeAttack, 0.001f));
	TestTrue(TEXT("boss damaged visual carries new HP"),
		FMath::IsNearlyEqual(AttackBoss->GetLastCombatVisualBossNewHPForTest(), AttackBoss->GetCurrentHP(), 0.001f));
	TestTrue(TEXT("boss HP text render updates after damage"),
		AttackBoss->GetPrototypeVisualLabelTextForTest().Contains(
			FString::Printf(TEXT("Boss HP %.0f / %.0f"), AttackBoss->GetCurrentHP(), AttackBoss->GetMaxHP())));
	DestroyDroneSelectionTestContext(AttackContext);

	FDroneSelectionTestContext DamageContext = CreateDroneSelectionTestContext(TEXT("CombatVisualDroneDamageWorld"));
	ARaidBoss* DamageBoss = nullptr;
	if (!PrepareBattleAttackTest(*this, DamageContext, DamageBoss, TEXT("combat visual drone damage")))
	{
		DestroyDroneSelectionTestContext(DamageContext);
		return false;
	}

	DamageContext.Drone->SetActorLocation(FVector::ZeroVector);
	const int32 DroneDamagedVisualCountBefore = DamageContext.Drone->GetCombatVisualDroneDamagedCountForTest();
	const int32 DamageTakenCountBefore = DamageContext.Drone->GetCombatRecordForTest().DamageTakenCount;
	const float DroneHPBeforeDamage = DamageContext.Drone->GetHealthValueForTest();
	TestEqual(TEXT("boss debug attack hits drone for visual damage test"),
		DamageBoss->PerformDebugAreaAttackForServer(FVector::ZeroVector, 150.0f, 25),
		1);
	TestEqual(TEXT("drone damaged visual event fires once after server hit"),
		DamageContext.Drone->GetCombatVisualDroneDamagedCountForTest(),
		DroneDamagedVisualCountBefore + 1);
	TestTrue(TEXT("drone damaged visual carries damage"),
		FMath::IsNearlyEqual(DamageContext.Drone->GetLastCombatVisualDroneDamageForTest(), 25.0f, 0.001f));
	TestTrue(TEXT("drone damaged visual carries old HP"),
		FMath::IsNearlyEqual(DamageContext.Drone->GetLastCombatVisualDroneDamageOldHPForTest(), DroneHPBeforeDamage, 0.001f));
	TestTrue(TEXT("drone damaged visual carries new HP"),
		FMath::IsNearlyEqual(DamageContext.Drone->GetLastCombatVisualDroneDamageNewHPForTest(), DamageContext.Drone->GetHealthValueForTest(), 0.001f));
	TestEqual(TEXT("combat visual damage event does not change DamageTakenCount policy"),
		DamageContext.Drone->GetCombatRecordForTest().DamageTakenCount,
		DamageTakenCountBefore + 1);

	TestTrue(TEXT("combat visual invincible dodge starts"),
		DamageContext.Drone->RequestDodgeForServer(FVector2D(1.0f, 0.0f)));
	const int32 IgnoredVisualCountBefore = DamageContext.Drone->GetCombatVisualDroneDamageIgnoredCountForTest();
	const int32 HPBeforeIgnoredDamage = DamageContext.Drone->GetHealth();
	DamageContext.Drone->ApplyDamageForServer(25, FName(TEXT("AutomationInvincible")));
	TestEqual(TEXT("invincible ignored damage leaves HP unchanged"),
		DamageContext.Drone->GetHealth(),
		HPBeforeIgnoredDamage);
	TestEqual(TEXT("invincible ignored damage fires ignored visual event"),
		DamageContext.Drone->GetCombatVisualDroneDamageIgnoredCountForTest(),
		IgnoredVisualCountBefore + 1);
	TestEqual(TEXT("invincible ignored visual reason is explicit"),
		DamageContext.Drone->GetLastCombatVisualDroneDamageIgnoredReasonForTest(),
		FName(TEXT("Invincible")));
	DestroyDroneSelectionTestContext(DamageContext);

	FDroneSelectionTestContext TelegraphContext = CreateDroneSelectionTestContext(TEXT("CombatVisualTelegraphWorld"));
	ARaidBoss* TelegraphBoss = nullptr;
	if (!PrepareBattleAttackTest(*this, TelegraphContext, TelegraphBoss, TEXT("combat visual telegraph")))
	{
		DestroyDroneSelectionTestContext(TelegraphContext);
		return false;
	}

	TestTrue(TEXT("combat visual telegraph starts"),
		TelegraphBoss->StartDebugTelegraphedAreaAttackForServer(FVector::ZeroVector, 150.0f, 25, 0.10f));
	ARaidBossAttackTelegraph* Telegraph = FindBossTelegraphForAutomationTest(TelegraphContext.World);
	TestNotNull(TEXT("combat visual telegraph actor exists"), Telegraph);
	TestEqual(TEXT("telegraph visual start event fires once"),
		Telegraph ? Telegraph->GetCombatVisualTelegraphStartCountForTest() : 0,
		1);
	DestroyDroneSelectionTestContext(TelegraphContext);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneD20LogSemanticsTest,
	"DroneProto.D20.LogSemantics.NoDamageAttack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneD20LogSemanticsTest::RunTest(const FString& Parameters)
{
	FDroneSelectionTestContext NoWeaponContext = CreateDroneSelectionTestContext(TEXT("D20NoWeaponAttackWorld"));
	ARaidBoss* NoWeaponBoss = nullptr;
	if (!PrepareBattleAttackTest(*this, NoWeaponContext, NoWeaponBoss, TEXT("D20 no weapon attack")))
	{
		DestroyDroneSelectionTestContext(NoWeaponContext);
		return false;
	}

	TestEqual(TEXT("no weapon attack starts with no left weapon"),
		NoWeaponContext.Drone->GetEquippedLeftWeaponPartIDForTest(),
		NAME_None);
	TestEqual(TEXT("no weapon attack starts with no right weapon"),
		NoWeaponContext.Drone->GetEquippedRightWeaponPartIDForTest(),
		NAME_None);

	const float BossHPBeforeNoWeaponAttack = NoWeaponBoss->GetCurrentHP();
	const FDroneCombatRecord RecordBeforeNoWeaponAttack = NoWeaponContext.Drone->GetCombatRecordForTest();
	const int32 AttackVisualCountBefore = NoWeaponContext.Drone->GetCombatVisualAttackCountForTest();
	const int32 BossDamagedVisualCountBefore = NoWeaponBoss->GetCombatVisualBossDamagedCountForTest();
	const float DamageDealt = AttackBossAndMeasureDamage(NoWeaponContext.Drone, NoWeaponBoss);
	TestTrue(TEXT("no weapon attack is allowed but deals no damage"),
		FMath::IsNearlyZero(DamageDealt, 0.001f));
	TestTrue(TEXT("no weapon attack leaves boss HP unchanged"),
		FMath::IsNearlyEqual(NoWeaponBoss->GetCurrentHP(), BossHPBeforeNoWeaponAttack, 0.001f));
	TestTrue(TEXT("no weapon attack leaves combat record boss damage unchanged"),
		FMath::IsNearlyEqual(
			NoWeaponContext.Drone->GetCombatRecordForTest().BossDamage,
			RecordBeforeNoWeaponAttack.BossDamage,
			0.001f));
	TestEqual(TEXT("no weapon attack does not fire attack visual"),
		NoWeaponContext.Drone->GetCombatVisualAttackCountForTest(),
		AttackVisualCountBefore);
	TestEqual(TEXT("no weapon attack does not fire boss damaged visual"),
		NoWeaponBoss->GetCombatVisualBossDamagedCountForTest(),
		BossDamagedVisualCountBefore);
	TestEqual(TEXT("no weapon attack records explicit no damage reason"),
		NoWeaponContext.Drone->GetLastAttackNoDamageReasonForTest(),
		FName(TEXT("NoWeapon")));

	DestroyDroneSelectionTestContext(NoWeaponContext);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDronePOR16BossTargetAssignmentTest,
	"DroneProto.POR16.Targeting.AssignmentAndClear",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDronePOR16BossTargetAssignmentTest::RunTest(const FString& Parameters)
{
	FDroneSelectionTestContext Context = CreateDroneSelectionTestContext(TEXT("POR16TargetAssignmentWorld"));
	ARaidBoss* Boss = nullptr;
	if (!PrepareBattleAttackTest(*this, Context, Boss, TEXT("POR16 target assignment")))
	{
		DestroyDroneSelectionTestContext(Context);
		return false;
	}

	TestEqual(TEXT("Ready assigns the current boss target"),
		Context.PC->GetCurrentTargetBoss(),
		Boss);
	TestTrue(TEXT("Ready locks the boss target"),
		Context.PC->IsTargetLocked());
	TestTrue(TEXT("Assigned boss is valid for server targeting"),
		Context.PC->HasValidBossTargetForServer());
	TestEqual(TEXT("Boss marker fallback is 2m above the actor"),
		Boss->GetTargetMarkerWorldLocation(),
		Boss->GetActorLocation() + FVector(0.0f, 0.0f, 200.0f));
	TestEqual(TEXT("PC marker getter uses the current boss marker"),
		Context.PC->GetTargetMarkerWorldLocation(),
		Boss->GetTargetMarkerWorldLocation());

	const int32 MarkerEventCountBeforeRefresh = Context.PC->GetTargetMarkerChangedCountForTest();
	Context.PC->RefreshTargetMarkerUI();
	const bool bMarkerHookShouldRun = Context.PC->IsLocalController() && Context.PC->GetNetMode() != NM_DedicatedServer;
	if (bMarkerHookShouldRun)
	{
		TestEqual(TEXT("local marker refresh calls the Blueprint hook once"),
			Context.PC->GetTargetMarkerChangedCountForTest(),
			MarkerEventCountBeforeRefresh + 1);
		TestTrue(TEXT("local marker hook is visible for an assigned live boss"),
			Context.PC->WasLastTargetMarkerVisibleForTest());
		TestEqual(TEXT("local marker hook receives the boss"),
			Context.PC->GetLastTargetMarkerBossForTest(),
			Boss);
	}
	else
	{
		TestEqual(TEXT("non-local marker refresh does not call the Blueprint hook"),
			Context.PC->GetTargetMarkerChangedCountForTest(),
			MarkerEventCountBeforeRefresh);
	}

	Context.PC->ClearBossTargetForServer(FName(TEXT("Automation")));
	TestEqual(TEXT("target clear removes the boss"),
		Context.PC->GetCurrentTargetBoss(),
		static_cast<ARaidBoss*>(nullptr));
	TestFalse(TEXT("target clear unlocks targeting"),
		Context.PC->IsTargetLocked());
	TestFalse(TEXT("cleared target is invalid for server targeting"),
		Context.PC->HasValidBossTargetForServer());
	Context.PC->ClearBossTargetForServer(FName(TEXT("AutomationAgain")));
	TestEqual(TEXT("second target clear remains cleared"),
		Context.PC->GetCurrentTargetBoss(),
		static_cast<ARaidBoss*>(nullptr));
	TestFalse(TEXT("second target clear remains unlocked"),
		Context.PC->IsTargetLocked());

	DestroyDroneSelectionTestContext(Context);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDronePOR16BossTargetAttackValidationTest,
	"DroneProto.POR16.Targeting.AttackValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDronePOR16BossTargetAttackValidationTest::RunTest(const FString& Parameters)
{
	FDroneSelectionTestContext NoBossContext = CreateDroneSelectionTestContext(TEXT("POR16NoBossWorld"));
	TestNotNull(TEXT("no boss context has PC"), NoBossContext.PC);
	TestNotNull(TEXT("no boss context has drone"), NoBossContext.Drone);
	if (!NoBossContext.PC || !NoBossContext.Drone)
	{
		DestroyDroneSelectionTestContext(NoBossContext);
		return false;
	}
	NoBossContext.PC->Server_RequestReadyForRaid_Implementation();
	TestEqual(TEXT("NoBoss ready still enters battle for existing flow"),
		NoBossContext.PC->GetCurrentSelectionState(),
		EPlayerSelectionState::InBattle);
	TestEqual(TEXT("NoBoss ready leaves target empty"),
		NoBossContext.PC->GetCurrentTargetBoss(),
		static_cast<ARaidBoss*>(nullptr));
	TestTrue(TEXT("NoBoss loadout applies"),
		NoBossContext.Drone->ApplyLoadout(NAME_None, ADronePartInventory::GetPulseLaserPartID(), NAME_None));
	NoBossContext.Drone->RequestAttackBoss();
	TestEqual(TEXT("NoBoss attack records NoTarget"),
		NoBossContext.Drone->GetLastAttackIgnoredReasonForTest(),
		FName(TEXT("NoTarget")));
	DestroyDroneSelectionTestContext(NoBossContext);

	FDroneSelectionTestContext SelectingContext = CreateDroneSelectionTestContext(TEXT("POR16SelectingAttackWorld"));
	ARaidBoss* SelectingBoss = SelectingContext.World ? SelectingContext.World->SpawnActor<ARaidBoss>() : nullptr;
	TestNotNull(TEXT("selecting boss is spawned"), SelectingBoss);
	if (!SelectingContext.PC || !SelectingContext.Drone || !SelectingBoss)
	{
		DestroyDroneSelectionTestContext(SelectingContext);
		return false;
	}
	SelectingContext.GameState->SetRaidBossForServer(SelectingBoss);
	TestTrue(TEXT("selecting loadout applies"),
		SelectingContext.Drone->ApplyLoadout(NAME_None, ADronePartInventory::GetPulseLaserPartID(), NAME_None));
	const float SelectingBossHPBefore = SelectingBoss->GetCurrentHP();
	SelectingContext.Drone->RequestAttackBoss();
	TestTrue(TEXT("Selecting attack leaves boss HP unchanged"),
		FMath::IsNearlyEqual(SelectingBoss->GetCurrentHP(), SelectingBossHPBefore, 0.001f));
	TestEqual(TEXT("Selecting attack records NotInBattle"),
		SelectingContext.Drone->GetLastAttackIgnoredReasonForTest(),
		FName(TEXT("NotInBattle")));
	DestroyDroneSelectionTestContext(SelectingContext);

	FDroneSelectionTestContext ValidContext = CreateDroneSelectionTestContext(TEXT("POR16ValidAttackWorld"));
	ARaidBoss* ValidBoss = nullptr;
	if (!PrepareBattleAttackTest(*this, ValidContext, ValidBoss, TEXT("POR16 valid attack")))
	{
		DestroyDroneSelectionTestContext(ValidContext);
		return false;
	}
	TestTrue(TEXT("valid attack loadout applies"),
		ValidContext.Drone->ApplyLoadout(NAME_None, ADronePartInventory::GetPulseLaserPartID(), NAME_None));
	ValidContext.Drone->SetActorLocation(FVector(-100000.0f, 0.0f, 0.0f));
	ValidBoss->SetActorLocation(FVector(100000.0f, 500.0f, 200.0f));
	const FVector ExpectedAttackDirection = (ValidBoss->GetActorLocation() - ValidContext.Drone->GetActorLocation()).GetSafeNormal();
	const float ValidBossHPBefore = ValidBoss->GetCurrentHP();
	const int32 AttackVisualCountBefore = ValidContext.Drone->GetCombatVisualAttackCountForTest();
	ValidContext.Drone->RequestAttackBoss();
	TestTrue(TEXT("valid target attack deals damage regardless of distance"),
		ValidBoss->GetCurrentHP() < ValidBossHPBefore);
	TestEqual(TEXT("valid target attack fires attack visual"),
		ValidContext.Drone->GetCombatVisualAttackCountForTest(),
		AttackVisualCountBefore + 1);
	const FVector ActualAttackDirection = (
		ValidContext.Drone->GetLastCombatVisualAttackToForTest()
		- ValidContext.Drone->GetLastCombatVisualAttackFromForTest()).GetSafeNormal();
	TestTrue(TEXT("attack visual direction uses boss minus drone direction"),
		ActualAttackDirection.Equals(ExpectedAttackDirection, 0.001f));

	ValidContext.PC->ClearBossTargetForServer(FName(TEXT("AutomationNoTarget")));
	const float NoTargetBossHPBefore = ValidBoss->GetCurrentHP();
	ValidContext.Drone->RequestAttackBoss();
	TestTrue(TEXT("cleared target attack leaves boss HP unchanged"),
		FMath::IsNearlyEqual(ValidBoss->GetCurrentHP(), NoTargetBossHPBefore, 0.001f));
	TestEqual(TEXT("cleared target attack records NoTarget"),
		ValidContext.Drone->GetLastAttackIgnoredReasonForTest(),
		FName(TEXT("NoTarget")));

	TestTrue(TEXT("target can be reassigned after clear"),
		ValidContext.PC->AssignBossTargetForServer());
	ValidContext.Drone->ApplyDamageForServer(ValidContext.Drone->GetMaxHealth() + 10, FName(TEXT("Automation")));
	TestTrue(TEXT("drone death clears target"),
		ValidContext.PC->GetCurrentTargetBoss() == nullptr);
	const float DeadDroneBossHPBefore = ValidBoss->GetCurrentHP();
	ValidContext.Drone->RequestAttackBoss();
	TestTrue(TEXT("dead drone attack leaves boss HP unchanged"),
		FMath::IsNearlyEqual(ValidBoss->GetCurrentHP(), DeadDroneBossHPBefore, 0.001f));
	TestEqual(TEXT("dead drone attack records DroneDead"),
		ValidContext.Drone->GetLastAttackIgnoredReasonForTest(),
		FName(TEXT("DroneDead")));
	DestroyDroneSelectionTestContext(ValidContext);

	FDroneSelectionTestContext BossDeadContext = CreateDroneSelectionTestContext(TEXT("POR16BossDeadWorld"));
	ARaidBoss* BossDeadBoss = nullptr;
	if (!PrepareBattleAttackTest(*this, BossDeadContext, BossDeadBoss, TEXT("POR16 boss dead attack")))
	{
		DestroyDroneSelectionTestContext(BossDeadContext);
		return false;
	}
	// 테스트 월드는 InitializeActorsForPlay를 거치지 않아 PC가 이터레이터에 자동 등록되지 않는다.
	BossDeadContext.World->AddController(BossDeadContext.PC);
	TestTrue(TEXT("boss dead loadout applies"),
		BossDeadContext.Drone->ApplyLoadout(NAME_None, ADronePartInventory::GetPulseLaserPartID(), NAME_None));
	BossDeadBoss->ApplyDamageForServer(BossDeadBoss->GetMaxHP() + 10.0f, BossDeadContext.PC, BossDeadContext.Drone);
	TestTrue(TEXT("boss is defeated for targeting"),
		BossDeadBoss->IsDefeated());
	TestEqual(TEXT("boss death clears current target"),
		BossDeadContext.PC->GetCurrentTargetBoss(),
		static_cast<ARaidBoss*>(nullptr));
	const float BossDeadHPBefore = BossDeadBoss->GetCurrentHP();
	BossDeadContext.Drone->RequestAttackBoss();
	TestTrue(TEXT("boss dead attack leaves HP unchanged"),
		FMath::IsNearlyEqual(BossDeadBoss->GetCurrentHP(), BossDeadHPBefore, 0.001f));
	TestEqual(TEXT("boss dead attack records BossDead"),
		BossDeadContext.Drone->GetLastAttackIgnoredReasonForTest(),
		FName(TEXT("BossDead")));
	DestroyDroneSelectionTestContext(BossDeadContext);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDronePOR18BossDestroyClearsTargetTest,
	"DroneProto.POR18.Targeting.BossDestroyClearsTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDronePOR18BossDestroyClearsTargetTest::RunTest(const FString& Parameters)
{
	FDroneSelectionTestContext Context = CreateDroneSelectionTestContext(TEXT("POR18BossDestroyWorld"));
	ARaidBoss* Boss = nullptr;
	if (!PrepareBattleAttackTest(*this, Context, Boss, TEXT("POR18 boss destroy")))
	{
		DestroyDroneSelectionTestContext(Context);
		return false;
	}
	// 테스트 월드는 InitializeActorsForPlay를 거치지 않아 PC가 이터레이터에 자동 등록되지 않는다.
	Context.World->AddController(Context.PC);

	TestNotNull(TEXT("ready assigns a boss target before destroy"),
		Context.PC->GetCurrentTargetBoss());
	TestTrue(TEXT("target is locked before destroy"),
		Context.PC->IsTargetLocked());

	// defeat 경로 없이 보스가 파괴되는 경우: EndPlay가 서버의 모든 PC 타겟을 해제해야 한다.
	Boss->Destroy();
	TestEqual(TEXT("boss destroy clears current target"),
		Context.PC->GetCurrentTargetBoss(),
		static_cast<ARaidBoss*>(nullptr));
	TestFalse(TEXT("boss destroy unlocks target"),
		Context.PC->IsTargetLocked());
	TestFalse(TEXT("boss destroy invalidates target for attack"),
		Context.PC->HasValidBossTarget());

	TestTrue(TEXT("destroyed boss loadout applies for attack check"),
		Context.Drone->ApplyLoadout(NAME_None, ADronePartInventory::GetPulseLaserPartID(), NAME_None));
	Context.Drone->RequestAttackBoss();
	TestEqual(TEXT("attack after boss destroy records NoTarget"),
		Context.Drone->GetLastAttackIgnoredReasonForTest(),
		FName(TEXT("NoTarget")));
	DestroyDroneSelectionTestContext(Context);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDronePOR18ReportPlayerKeyGuardTest,
	"DroneProto.POR18.Report.PlayerKeyDuplicateGuard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDronePOR18ReportPlayerKeyGuardTest::RunTest(const FString& Parameters)
{
	FDroneSelectionTestContext Context = CreateDroneSelectionTestContext(TEXT("POR18ReportKeyWorld"));
	ARaidGameMode* GameMode = Context.World ? Context.World->SpawnActor<ARaidGameMode>() : nullptr;
	TestNotNull(TEXT("report key game mode is spawned"), GameMode);
	ARaidBoss* Boss = nullptr;
	if (!GameMode || !PrepareBattleAttackTest(*this, Context, Boss, TEXT("POR18 report key")))
	{
		DestroyDroneSelectionTestContext(Context);
		return false;
	}

	TestTrue(TEXT("first report is created"),
		Context.PC->TryCreateDroneReportForServer(EDroneReportTrigger::RaidTimeLimit, false));

	// 재접속(새 PC 인스턴스) 시뮬레이션: PC bool만 리셋해도 GameMode PlayerKey set이 중복을 막아야 한다.
	Context.PC->ResetDroneReportForTest();
	TestFalse(TEXT("PC-level reset alone cannot duplicate the report"),
		Context.PC->TryCreateDroneReportForServer(EDroneReportTrigger::RaidTimeLimit, false));

	// 새 레이드 시작(RaidReady)과 같은 키 해제 후에는 다시 생성 가능해야 한다.
	GameMode->ClearDroneReportKeyForServer(Context.PC, FName(TEXT("Automation")));
	Context.PC->ResetDroneReportForTest();
	TestTrue(TEXT("report can be created again after key clear"),
		Context.PC->TryCreateDroneReportForServer(EDroneReportTrigger::RaidTimeLimit, false));
	DestroyDroneSelectionTestContext(Context);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDronePOR18StatsRecalcGuardTest,
	"DroneProto.POR18.Drone.StatsRecalcInBattleGuard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDronePOR18StatsRecalcGuardTest::RunTest(const FString& Parameters)
{
	FDroneSelectionTestContext Context = CreateDroneSelectionTestContext(TEXT("POR18StatsRecalcWorld"));
	ARaidBoss* Boss = nullptr;
	if (!PrepareBattleAttackTest(*this, Context, Boss, TEXT("POR18 stats recalc")))
	{
		DestroyDroneSelectionTestContext(Context);
		return false;
	}

	TestTrue(TEXT("InBattle loadout applies"),
		Context.Drone->ApplyLoadout(NAME_None, ADronePartInventory::GetPulseLaserPartID(), NAME_None));
	Context.Drone->ApplyDamageForServer(30, FName(TEXT("Automation")));
	const int32 HealthAfterDamage = Context.Drone->GetHealth();
	TestTrue(TEXT("drone takes damage for the guard check"),
		HealthAfterDamage < Context.Drone->GetMaxHealth());

	// InBattle 중 RecalculateStats(ApplyLoadout 경유)가 풀피 회복을 일으키지 않아야 한다.
	TestTrue(TEXT("InBattle loadout re-apply succeeds"),
		Context.Drone->ApplyLoadout(NAME_None, ADronePartInventory::GetPulseLaserPartID(), NAME_None));
	TestEqual(TEXT("InBattle recalc attempt keeps damaged health"),
		Context.Drone->GetHealth(),
		HealthAfterDamage);
	DestroyDroneSelectionTestContext(Context);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRaidBossStateJoinGateTest,
	"DroneProto.Q4.RaidBoss.BossStateJoinGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRaidBossStateJoinGateTest::RunTest(const FString& Parameters)
{
	FDroneSelectionTestContext Context = CreateDroneSelectionTestContext(TEXT("Q4BossStateJoinGateWorld"));
	ARaidGameMode* GameMode = Context.World ? Context.World->SpawnActor<ARaidGameMode>() : nullptr;
	ARaidBoss* Boss = Context.World ? Context.World->SpawnActor<ARaidBoss>() : nullptr;

	TestNotNull(TEXT("boss state gate world is created"), Context.World);
	TestNotNull(TEXT("boss state gate game state is spawned"), Context.GameState);
	TestNotNull(TEXT("boss state gate inventory is spawned"), Context.Inventory);
	TestNotNull(TEXT("boss state gate player controller is spawned"), Context.PC);
	TestNotNull(TEXT("boss state gate drone is spawned"), Context.Drone);
	TestNotNull(TEXT("boss state gate game mode is spawned"), GameMode);
	TestNotNull(TEXT("boss state gate boss is spawned"), Boss);
	if (!Context.World || !Context.GameState || !Context.Inventory || !Context.PC || !Context.Drone || !GameMode || !Boss)
	{
		DestroyDroneSelectionTestContext(Context);
		return false;
	}

	Context.World->AddController(Context.PC);
	Context.GameState->SetRaidBossForServer(Boss);

	TestEqual(TEXT("boss starts in Spawn state"), Boss->GetBossState(), EBossState::Spawn);
	FName RejectReason;
	TestTrue(TEXT("Waiting/Drafting with Spawn boss accepts raid join"), GameMode->CanAcceptRaidJoinForServer(RejectReason));
	TestEqual(TEXT("accepted join has no reject reason"), RejectReason, NAME_None);

	Context.PC->Server_RequestReadyForRaid_Implementation();
	TestEqual(TEXT("Ready moves raid to Battle"), Context.GameState->RaidState, ERaidState::Battle);
	TestEqual(TEXT("Ready/pattern start moves boss to Battle"), Boss->GetBossState(), EBossState::Battle);

	ARaidPlayerController* BattleLateJoinPC = Context.World->SpawnActor<ARaidPlayerController>();
	ADrone* BattleLateJoinDrone = Context.World->SpawnActor<ADrone>();
	TestNotNull(TEXT("battle late join PC is spawned"), BattleLateJoinPC);
	TestNotNull(TEXT("battle late join drone is spawned"), BattleLateJoinDrone);
	if (!BattleLateJoinPC || !BattleLateJoinDrone)
	{
		DestroyDroneSelectionTestContext(Context);
		return false;
	}
	Context.World->AddController(BattleLateJoinPC);
	BattleLateJoinPC->Possess(BattleLateJoinDrone);
	BattleLateJoinPC->Server_RequestReadyForRaid_Implementation();
	TestEqual(TEXT("Battle late join Ready remains allowed"), BattleLateJoinPC->GetPlayerSelectionState(), EPlayerSelectionState::InBattle);
	TestEqual(TEXT("Battle late join keeps boss in Battle"), Boss->GetBossState(), EBossState::Battle);

	Boss->ApplyDamageForServer(Boss->GetMaxHP() + 1.0f, Context.PC, Context.Drone);
	TestTrue(TEXT("boss HP defeat flag remains HP based"), Boss->IsDefeated());
	TestEqual(TEXT("boss death moves BossState to Dead"), Boss->GetBossState(), EBossState::Dead);
	TestFalse(TEXT("Dead boss rejects raid join"), GameMode->CanAcceptRaidJoinForServer(RejectReason));
	TestEqual(TEXT("Dead boss reject reason is BossDead"), RejectReason, FName(TEXT("BossDead")));

	ARaidPlayerController* DeadLateJoinPC = Context.World->SpawnActor<ARaidPlayerController>();
	ADrone* DeadLateJoinDrone = Context.World->SpawnActor<ADrone>();
	TestNotNull(TEXT("dead late join PC is spawned"), DeadLateJoinPC);
	TestNotNull(TEXT("dead late join drone is spawned"), DeadLateJoinDrone);
	if (!DeadLateJoinPC || !DeadLateJoinDrone)
	{
		DestroyDroneSelectionTestContext(Context);
		return false;
	}
	Context.World->AddController(DeadLateJoinPC);
	DeadLateJoinPC->Possess(DeadLateJoinDrone);
	DeadLateJoinPC->Server_RequestReadyForRaid_Implementation();
	TestEqual(TEXT("Dead boss Ready is rejected before InBattle"), DeadLateJoinPC->GetPlayerSelectionState(), EPlayerSelectionState::Selecting);

	GameMode->HandleBossDefeatedForServer();
	TestEqual(TEXT("boss defeated flow moves raid to End"), Context.GameState->RaidState, ERaidState::End);
	TestEqual(TEXT("RaidEnd completion moves BossState to Clear"), Boss->GetBossState(), EBossState::Clear);
	TestFalse(TEXT("Clear boss rejects raid join"), GameMode->CanAcceptRaidJoinForServer(RejectReason));
	TestEqual(TEXT("Clear boss reject reason is BossClear"), RejectReason, FName(TEXT("BossClear")));

	FDroneSelectionTestContext TimeOverContext = CreateDroneSelectionTestContext(TEXT("Q4BossStateTimeOverWorld"));
	ARaidGameMode* TimeOverGameMode = TimeOverContext.World ? TimeOverContext.World->SpawnActor<ARaidGameMode>() : nullptr;
	ARaidBoss* TimeOverBoss = TimeOverContext.World ? TimeOverContext.World->SpawnActor<ARaidBoss>() : nullptr;
	TestNotNull(TEXT("time over game mode is spawned"), TimeOverGameMode);
	TestNotNull(TEXT("time over boss is spawned"), TimeOverBoss);
	if (!TimeOverContext.World || !TimeOverContext.GameState || !TimeOverContext.PC || !TimeOverContext.Drone || !TimeOverGameMode || !TimeOverBoss)
	{
		DestroyDroneSelectionTestContext(TimeOverContext);
		DestroyDroneSelectionTestContext(Context);
		return false;
	}

	TimeOverContext.World->AddController(TimeOverContext.PC);
	TimeOverContext.GameState->SetRaidBossForServer(TimeOverBoss);
	TestTrue(TEXT("time over raid time limit is test-configurable"),
		SetFloatPropertyForAutomationTest(TimeOverGameMode, FName(TEXT("RaidTimeLimitSeconds")), 0.05f));
	TimeOverContext.PC->Server_RequestReadyForRaid_Implementation();
	TestEqual(TEXT("time over setup moves boss to Battle"), TimeOverBoss->GetBossState(), EBossState::Battle);
	TimeOverGameMode->ExpireRaidTimeLimitForTest();
	TestEqual(TEXT("time over moves raid to End"), TimeOverContext.GameState->RaidState, ERaidState::End);
	TestEqual(TEXT("time over RaidEnd moves boss to Clear"), TimeOverBoss->GetBossState(), EBossState::Clear);
	TestFalse(TEXT("TimeOver rejects raid join"), TimeOverGameMode->CanAcceptRaidJoinForServer(RejectReason));
	TestEqual(TEXT("TimeOver reject reason is TimeOver"), RejectReason, FName(TEXT("TimeOver")));

	ARaidPlayerController* TimeOverLateJoinPC = TimeOverContext.World->SpawnActor<ARaidPlayerController>();
	ADrone* TimeOverLateJoinDrone = TimeOverContext.World->SpawnActor<ADrone>();
	TestNotNull(TEXT("time over late join PC is spawned"), TimeOverLateJoinPC);
	TestNotNull(TEXT("time over late join drone is spawned"), TimeOverLateJoinDrone);
	if (!TimeOverLateJoinPC || !TimeOverLateJoinDrone)
	{
		DestroyDroneSelectionTestContext(TimeOverContext);
		DestroyDroneSelectionTestContext(Context);
		return false;
	}
	TimeOverContext.World->AddController(TimeOverLateJoinPC);
	TimeOverLateJoinPC->Possess(TimeOverLateJoinDrone);
	TimeOverLateJoinPC->Server_RequestReadyForRaid_Implementation();
	TestEqual(TEXT("TimeOver Ready is rejected before InBattle"), TimeOverLateJoinPC->GetPlayerSelectionState(), EPlayerSelectionState::Selecting);

	DestroyDroneSelectionTestContext(TimeOverContext);
	DestroyDroneSelectionTestContext(Context);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDronePOR19BossPatternLifecycleTest,
	"DroneProto.POR19.BossPattern.StartStopLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDronePOR19BossPatternLifecycleTest::RunTest(const FString& Parameters)
{
	FDroneSelectionTestContext Context = CreateDroneSelectionTestContext(TEXT("POR19PatternLifecycleWorld"));
	ARaidBoss* Boss = nullptr;
	if (!PrepareBattleAttackTest(*this, Context, Boss, TEXT("POR19 pattern lifecycle")))
	{
		DestroyDroneSelectionTestContext(Context);
		return false;
	}

	TestTrue(TEXT("pattern start succeeds in battle"), Boss->StartBossPatternForServer());
	TestTrue(TEXT("pattern timer is active after start"), Boss->IsBossPatternTimerActiveForTest());
	TestFalse(TEXT("duplicate start is ignored"), Boss->StartBossPatternForServer());
	TestTrue(TEXT("pattern timer stays active after duplicate start"), Boss->IsBossPatternTimerActiveForTest());

	// 수동 1회 fire: 기존 telegraph 서버 공격 경로를 그대로 재사용해야 한다.
	Boss->FireBossPatternOnceForTest();
	TestEqual(TEXT("pattern fire increments sequence"), Boss->GetBossPatternFireSequenceForTest(), 1);
	int32 TelegraphCount = 0;
	for (TActorIterator<ARaidBossAttackTelegraph> It(Context.World); It; ++It)
	{
		TelegraphCount++;
	}
	TestEqual(TEXT("pattern fire spawns a telegraph actor"), TelegraphCount, 1);

	Boss->StopBossPatternForServer(FName(TEXT("Automation")));
	TestFalse(TEXT("pattern timer is inactive after stop"), Boss->IsBossPatternTimerActiveForTest());

	// 보스 사망 경로가 패턴을 자체 정지해야 한다.
	TestTrue(TEXT("pattern restart succeeds"), Boss->StartBossPatternForServer());
	Boss->ApplyDamageForServer(Boss->GetMaxHP() + 10.0f, Context.PC, Context.Drone);
	TestTrue(TEXT("boss is defeated for pattern stop"), Boss->IsDefeated());
	TestFalse(TEXT("boss death stops the pattern timer"), Boss->IsBossPatternTimerActiveForTest());
	TestFalse(TEXT("start is ignored while boss is dead"), Boss->StartBossPatternForServer());
	DestroyDroneSelectionTestContext(Context);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDronePOR19BossPatternRaidFlowTest,
	"DroneProto.POR19.BossPattern.BattleStartAndRaidEndFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDronePOR19BossPatternRaidFlowTest::RunTest(const FString& Parameters)
{
	FDroneSelectionTestContext Context = CreateDroneSelectionTestContext(TEXT("POR19PatternRaidFlowWorld"));
	ARaidGameMode* GameMode = Context.World ? Context.World->SpawnActor<ARaidGameMode>() : nullptr;
	TestNotNull(TEXT("pattern game mode is spawned"), GameMode);
	ARaidBoss* Boss = nullptr;
	if (!GameMode || !PrepareBattleAttackTest(*this, Context, Boss, TEXT("POR19 pattern raid flow")))
	{
		DestroyDroneSelectionTestContext(Context);
		return false;
	}

	// Ready→Battle 전이 시 GameMode 오케스트레이션이 모든 보스 패턴을 시작해야 한다.
	TestTrue(TEXT("battle transition starts the boss pattern"),
		Boss->IsBossPatternTimerActiveForTest());

	// RaidEnd 상태에서 fire가 오면 공격 없이 자체 정지해야 한다 (정지 누락 대비 마지막 저지선).
	Context.GameState->SetRaidStateForServer(ERaidState::End);
	Boss->FireBossPatternOnceForTest();
	TestFalse(TEXT("raid end fire stops the pattern"), Boss->IsBossPatternTimerActiveForTest());
	TestEqual(TEXT("raid end fire does not run the attack"),
		Boss->GetBossPatternFireSequenceForTest(), 0);

	// RaidEnd 반환 플로우(GameMode)가 패턴을 정지해야 한다.
	Context.GameState->SetRaidStateForServer(ERaidState::Battle);
	TestTrue(TEXT("pattern restarts for raid end flow"), Boss->StartBossPatternForServer());
	// 테스트 월드는 InitializeActorsForPlay를 거치지 않아 PC가 이터레이터에 자동 등록되지 않는다.
	Context.World->AddController(Context.PC);
	GameMode->ReturnAllEquippedPartsForRaidEnd(FName(TEXT("RaidTimeLimit")));
	TestFalse(TEXT("raid end return stops the pattern"), Boss->IsBossPatternTimerActiveForTest());
	DestroyDroneSelectionTestContext(Context);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDronePOR19BossStunDamageMultiplierTest,
	"DroneProto.POR19.BossStun.DamageMultiplier",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDronePOR19BossStunDamageMultiplierTest::RunTest(const FString& Parameters)
{
	FDroneSelectionTestContext Context = CreateDroneSelectionTestContext(TEXT("POR19BossStunWorld"));
	ARaidBoss* Boss = nullptr;
	if (!PrepareBattleAttackTest(*this, Context, Boss, TEXT("POR19 boss stun")))
	{
		DestroyDroneSelectionTestContext(Context);
		return false;
	}

	// 스턴 전: 기본 데미지 그대로.
	const float HPStart = Boss->GetCurrentHP();
	Boss->ApplyDamageForServer(100.0f, Context.PC, Context.Drone);
	TestTrue(TEXT("base damage is applied without stun"),
		FMath::IsNearlyEqual(HPStart - Boss->GetCurrentHP(), 100.0f, 0.001f));

	// 스턴 중: 데미지 x StunDamageMultiplier(기본 1.5).
	Boss->SetStunnedForServer(true, FName(TEXT("Automation")));
	TestTrue(TEXT("boss enters stun on server"), Boss->IsStunned());
	const float HPBeforeStunnedHit = Boss->GetCurrentHP();
	Boss->ApplyDamageForServer(100.0f, Context.PC, Context.Drone);
	TestTrue(TEXT("stunned damage applies the multiplier"),
		FMath::IsNearlyEqual(HPBeforeStunnedHit - Boss->GetCurrentHP(), 150.0f, 0.001f));

	// 스턴 해제 후: 기본 데미지로 복원.
	Boss->SetStunnedForServer(false, FName(TEXT("Automation")));
	TestFalse(TEXT("boss leaves stun on server"), Boss->IsStunned());
	const float HPBeforeNormalHit = Boss->GetCurrentHP();
	Boss->ApplyDamageForServer(100.0f, Context.PC, Context.Drone);
	TestTrue(TEXT("damage returns to base after stun end"),
		FMath::IsNearlyEqual(HPBeforeNormalHit - Boss->GetCurrentHP(), 100.0f, 0.001f));

	// 죽은 보스는 스턴 진입 불가.
	Boss->ApplyDamageForServer(Boss->GetMaxHP() + 1000.0f, Context.PC, Context.Drone);
	TestTrue(TEXT("boss is defeated for stun guard"), Boss->IsDefeated());
	Boss->SetStunnedForServer(true, FName(TEXT("Automation")));
	TestFalse(TEXT("dead boss cannot be stunned"), Boss->IsStunned());

	// 복제/OnRep 등록 확인.
	FProperty* StunProperty = ARaidBoss::StaticClass()->FindPropertyByName(FName(TEXT("bIsStunned")));
	TestNotNull(TEXT("bIsStunned property exists"), StunProperty);
	if (StunProperty)
	{
		TestTrue(TEXT("bIsStunned is replicated"), StunProperty->HasAnyPropertyFlags(CPF_Net));
		TestEqual(TEXT("bIsStunned rep notify targets OnRep_IsStunned"),
			StunProperty->RepNotifyFunc,
			FName(TEXT("OnRep_IsStunned")));
	}
	DestroyDroneSelectionTestContext(Context);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneMoveDistanceAccumulationTest,
	"DroneProto.D8.Drone.MoveDistanceAccumulation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneMoveDistanceAccumulationTest::RunTest(const FString& Parameters)
{
	FDroneSelectionTestContext Context = CreateDroneSelectionTestContext(TEXT("DroneMoveDistanceAccumulationWorld"));
	TestNotNull(TEXT("move distance world is created"), Context.World);
	TestNotNull(TEXT("move distance player controller is spawned"), Context.PC);
	TestNotNull(TEXT("move distance drone is spawned"), Context.Drone);
	if (!Context.World || !Context.PC || !Context.Drone)
	{
		DestroyDroneSelectionTestContext(Context);
		return false;
	}

	Context.Drone->SetActorLocation(FVector::ZeroVector);
	Context.Drone->ResetMoveDistanceForServerForTest(FName(TEXT("Spawn")));
	Context.Drone->SetActorLocation(FVector(100.0f, 0.0f, 0.0f));
	Context.Drone->UpdateMoveDistanceForServerForTest(0.016f);
	TestTrue(TEXT("first server location sample is ignored"),
		FMath::IsNearlyZero(Context.Drone->GetVectorAccumulatedMoveDistanceForTest(), 0.001f));
	TestTrue(TEXT("Selecting movement is not accumulated"),
		FMath::IsNearlyZero(Context.Drone->GetBoosterAccumulatedMoveDistanceForTest(), 0.001f));

	Context.PC->Server_RequestReadyForRaid_Implementation();
	TestEqual(TEXT("move distance player is InBattle"),
		Context.PC->GetCurrentSelectionState(),
		EPlayerSelectionState::InBattle);
	Context.Drone->ResetMoveDistanceForServerForTest(FName(TEXT("Possess")));
	Context.Drone->SetActorLocation(FVector::ZeroVector);
	Context.Drone->UpdateMoveDistanceForServerForTest(0.016f);
	Context.Drone->SetActorLocation(FVector(100.0f, 0.0f, 0.0f));
	Context.Drone->UpdateMoveDistanceForServerForTest(0.10f);
	TestTrue(TEXT("InBattle alive movement accumulates Vector meters"),
		FMath::IsNearlyEqual(Context.Drone->GetVectorAccumulatedMoveDistanceForTest(), 1.0f, 0.001f));
	TestTrue(TEXT("InBattle alive movement accumulates Booster meters separately"),
		FMath::IsNearlyEqual(Context.Drone->GetBoosterAccumulatedMoveDistanceForTest(), 1.0f, 0.001f));

	Context.Drone->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
	Context.Drone->UpdateMoveDistanceForServerForTest(0.10f);
	TestTrue(TEXT("second normal move adds to Vector total"),
		FMath::IsNearlyEqual(Context.Drone->GetVectorAccumulatedMoveDistanceForTest(), 1.5f, 0.001f));
	TestTrue(TEXT("second normal move adds to Booster total"),
		FMath::IsNearlyEqual(Context.Drone->GetBoosterAccumulatedMoveDistanceForTest(), 1.5f, 0.001f));

	Context.Drone->SetActorLocation(FVector(100000.0f, 0.0f, 0.0f));
	Context.Drone->UpdateMoveDistanceForServerForTest(0.016f);
	TestTrue(TEXT("TooLargeDelta is ignored for Vector total"),
		FMath::IsNearlyEqual(Context.Drone->GetVectorAccumulatedMoveDistanceForTest(), 1.5f, 0.001f));
	TestTrue(TEXT("TooLargeDelta is ignored for Booster total"),
		FMath::IsNearlyEqual(Context.Drone->GetBoosterAccumulatedMoveDistanceForTest(), 1.5f, 0.001f));

	Context.Drone->SetActorLocation(FVector(100100.0f, 0.0f, 0.0f));
	Context.Drone->UpdateMoveDistanceForServerForTest(0.50f);
	TestTrue(TEXT("hitch delta time is ignored for Vector total"),
		FMath::IsNearlyEqual(Context.Drone->GetVectorAccumulatedMoveDistanceForTest(), 1.5f, 0.001f));
	TestTrue(TEXT("hitch delta time is ignored for Booster total"),
		FMath::IsNearlyEqual(Context.Drone->GetBoosterAccumulatedMoveDistanceForTest(), 1.5f, 0.001f));

	Context.Drone->ResetVectorMoveDistanceForServerForTest(FName(TEXT("VectorAttack")));
	TestTrue(TEXT("Vector reset clears only Vector total"),
		FMath::IsNearlyZero(Context.Drone->GetVectorAccumulatedMoveDistanceForTest(), 0.001f));
	TestTrue(TEXT("Vector reset does not clear Booster total"),
		FMath::IsNearlyEqual(Context.Drone->GetBoosterAccumulatedMoveDistanceForTest(), 1.5f, 0.001f));

	Context.Drone->ResetMoveDistanceForServerForTest(FName(TEXT("Loadout")));
	TestTrue(TEXT("full move distance reset clears Vector total"),
		FMath::IsNearlyZero(Context.Drone->GetVectorAccumulatedMoveDistanceForTest(), 0.001f));
	TestTrue(TEXT("full move distance reset clears Booster total"),
		FMath::IsNearlyZero(Context.Drone->GetBoosterAccumulatedMoveDistanceForTest(), 0.001f));

	DestroyDroneSelectionTestContext(Context);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneMoveDistanceResetTest,
	"DroneProto.D8.Drone.MoveDistanceReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneMoveDistanceResetTest::RunTest(const FString& Parameters)
{
	UDronePartReturnManager* DeathReturnManager = nullptr;
	FDroneSelectionTestContext DeathContext = CreateDroneReturnTestContext(
		TEXT("DroneMoveDistanceDeathResetWorld"),
		DeathReturnManager);
	TestNotNull(TEXT("death reset world is created"), DeathContext.World);
	TestNotNull(TEXT("death reset player controller is spawned"), DeathContext.PC);
	TestNotNull(TEXT("death reset drone is spawned"), DeathContext.Drone);
	if (!DeathContext.World || !DeathContext.PC || !DeathContext.Drone)
	{
		DestroyDroneSelectionTestContext(DeathContext);
		return false;
	}

	DeathContext.PC->Server_RequestReadyForRaid_Implementation();
	DeathContext.Drone->ResetMoveDistanceForServerForTest(FName(TEXT("Possess")));
	DeathContext.Drone->SetActorLocation(FVector::ZeroVector);
	DeathContext.Drone->UpdateMoveDistanceForServerForTest(0.016f);
	DeathContext.Drone->SetActorLocation(FVector(200.0f, 0.0f, 0.0f));
	DeathContext.Drone->UpdateMoveDistanceForServerForTest(0.10f);
	TestTrue(TEXT("death reset setup accumulates movement"),
		DeathContext.Drone->GetVectorAccumulatedMoveDistanceForTest() > 0.0f);
	DeathContext.Drone->ApplyDamageForServer(DeathContext.Drone->GetMaxHealth() + 10, FName(TEXT("Automation")));
	TestTrue(TEXT("death reset drone dies"), DeathContext.Drone->IsDead());
	TestTrue(TEXT("death resets Vector movement distance"),
		FMath::IsNearlyZero(DeathContext.Drone->GetVectorAccumulatedMoveDistanceForTest(), 0.001f));
	TestTrue(TEXT("death resets Booster movement distance"),
		FMath::IsNearlyZero(DeathContext.Drone->GetBoosterAccumulatedMoveDistanceForTest(), 0.001f));
	DestroyDroneSelectionTestContext(DeathContext);

	UDronePartReturnManager* RaidEndReturnManager = nullptr;
	FDroneSelectionTestContext RaidEndContext = CreateDroneReturnTestContext(
		TEXT("DroneMoveDistanceRaidEndResetWorld"),
		RaidEndReturnManager);
	TestNotNull(TEXT("raid end reset world is created"), RaidEndContext.World);
	TestNotNull(TEXT("raid end reset player controller is spawned"), RaidEndContext.PC);
	TestNotNull(TEXT("raid end reset drone is spawned"), RaidEndContext.Drone);
	if (!RaidEndContext.World || !RaidEndContext.PC || !RaidEndContext.Drone)
	{
		DestroyDroneSelectionTestContext(RaidEndContext);
		return false;
	}

	RaidEndContext.PC->Server_RequestReadyForRaid_Implementation();
	RaidEndContext.Drone->ResetMoveDistanceForServerForTest(FName(TEXT("Possess")));
	RaidEndContext.Drone->SetActorLocation(FVector::ZeroVector);
	RaidEndContext.Drone->UpdateMoveDistanceForServerForTest(0.016f);
	RaidEndContext.Drone->SetActorLocation(FVector(300.0f, 0.0f, 0.0f));
	RaidEndContext.Drone->UpdateMoveDistanceForServerForTest(0.10f);
	TestTrue(TEXT("raid end reset setup accumulates movement"),
		RaidEndContext.Drone->GetBoosterAccumulatedMoveDistanceForTest() > 0.0f);
	if (ARaidGameMode* GameMode = RaidEndContext.World->SpawnActor<ARaidGameMode>())
	{
		RaidEndContext.World->AddController(RaidEndContext.PC);
		GameMode->ReturnAllEquippedPartsForRaidEnd(FName(TEXT("Automation")));
	}
	TestTrue(TEXT("RaidEnd resets Vector movement distance"),
		FMath::IsNearlyZero(RaidEndContext.Drone->GetVectorAccumulatedMoveDistanceForTest(), 0.001f));
	TestTrue(TEXT("RaidEnd resets Booster movement distance"),
		FMath::IsNearlyZero(RaidEndContext.Drone->GetBoosterAccumulatedMoveDistanceForTest(), 0.001f));

	RaidEndContext.Drone->SetActorLocation(FVector::ZeroVector);
	RaidEndContext.Drone->UpdateMoveDistanceForServerForTest(0.016f);
	RaidEndContext.Drone->SetActorLocation(FVector(100.0f, 0.0f, 0.0f));
	RaidEndContext.Drone->UpdateMoveDistanceForServerForTest(0.10f);
	TestTrue(TEXT("RaidEnd state keeps later movement from accumulating"),
		FMath::IsNearlyZero(RaidEndContext.Drone->GetVectorAccumulatedMoveDistanceForTest(), 0.001f));
	DestroyDroneSelectionTestContext(RaidEndContext);

	FDroneSelectionTestContext LoadoutContext = CreateDroneSelectionTestContext(TEXT("DroneMoveDistanceLoadoutResetWorld"));
	TestNotNull(TEXT("loadout reset world is created"), LoadoutContext.World);
	TestNotNull(TEXT("loadout reset player controller is spawned"), LoadoutContext.PC);
	TestNotNull(TEXT("loadout reset drone is spawned"), LoadoutContext.Drone);
	if (!LoadoutContext.World || !LoadoutContext.PC || !LoadoutContext.Drone)
	{
		DestroyDroneSelectionTestContext(LoadoutContext);
		return false;
	}

	LoadoutContext.PC->Server_RequestReadyForRaid_Implementation();
	LoadoutContext.Drone->ResetMoveDistanceForServerForTest(FName(TEXT("Possess")));
	LoadoutContext.Drone->SetActorLocation(FVector::ZeroVector);
	LoadoutContext.Drone->UpdateMoveDistanceForServerForTest(0.016f);
	LoadoutContext.Drone->SetActorLocation(FVector(100.0f, 0.0f, 0.0f));
	LoadoutContext.Drone->UpdateMoveDistanceForServerForTest(0.10f);
	TestTrue(TEXT("new loadout reset setup accumulates movement"),
		LoadoutContext.Drone->GetVectorAccumulatedMoveDistanceForTest() > 0.0f);
	TestTrue(TEXT("reapplying loadout resets Vector movement distance"),
		LoadoutContext.Drone->ApplyLoadout(NAME_None, ADronePartInventory::GetFractureBurstPartID(), NAME_None));
	TestTrue(TEXT("new loadout resets Vector movement distance"),
		FMath::IsNearlyZero(LoadoutContext.Drone->GetVectorAccumulatedMoveDistanceForTest(), 0.001f));
	TestTrue(TEXT("new loadout resets Booster movement distance"),
		FMath::IsNearlyZero(LoadoutContext.Drone->GetBoosterAccumulatedMoveDistanceForTest(), 0.001f));

	DestroyDroneSelectionTestContext(LoadoutContext);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneDeathReturnTest,
	"DroneProto.D6.Drone.DeathReturnClearsEquippedSlots",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneDeathReturnTest::RunTest(const FString& Parameters)
{
	UDronePartReturnManager* FullReturnManager = nullptr;
	FDroneSelectionTestContext FullLoadout = CreateDroneReturnTestContext(TEXT("DroneDeathReturnFullLoadoutWorld"), FullReturnManager);
	TestNotNull(TEXT("full loadout world is created"), FullLoadout.World);
	TestNotNull(TEXT("full loadout inventory is spawned"), FullLoadout.Inventory);
	TestNotNull(TEXT("full loadout player controller is spawned"), FullLoadout.PC);
	TestNotNull(TEXT("full loadout drone is spawned"), FullLoadout.Drone);
	TestNotNull(TEXT("full loadout return manager is created"), FullReturnManager);
	if (!FullLoadout.World || !FullLoadout.Inventory || !FullLoadout.PC || !FullLoadout.Drone || !FullReturnManager)
	{
		DestroyDroneSelectionTestContext(FullLoadout);
		return false;
	}

	const FName CoreZenith = ADronePartInventory::GetCoreZenithPartID();
	const FName PulseLaser = ADronePartInventory::GetPulseLaserPartID();
	const FName VectorCannon = ADronePartInventory::GetVectorCannonPartID();

	TestTrue(TEXT("full death test consumes selected core"), FullLoadout.Inventory->TryConsumePart(CoreZenith));
	TestTrue(TEXT("full death test consumes selected left weapon"), FullLoadout.Inventory->TryConsumePart(PulseLaser));
	TestTrue(TEXT("full death test consumes selected right weapon"), FullLoadout.Inventory->TryConsumePart(VectorCannon));
	FullLoadout.PC->SetSelectedPartIDForSlotForServer(EPartSlot::Core, CoreZenith);
	FullLoadout.PC->SetSelectedPartIDForSlotForServer(EPartSlot::LeftWeapon, PulseLaser);
	FullLoadout.PC->SetSelectedPartIDForSlotForServer(EPartSlot::RightWeapon, VectorCannon);
	FullLoadout.PC->Server_RequestReadyForRaid_Implementation();

	TestEqual(TEXT("full loadout ready moves player to battle"),
		FullLoadout.PC->GetCurrentSelectionState(),
		EPlayerSelectionState::InBattle);
	TestEqual(TEXT("full loadout equips core before death"),
		FullLoadout.PC->GetEquippedPartIDBySlot(EPartSlot::Core),
		CoreZenith);
	TestEqual(TEXT("full loadout equips left weapon before death"),
		FullLoadout.PC->GetEquippedPartIDBySlot(EPartSlot::LeftWeapon),
		PulseLaser);
	TestEqual(TEXT("full loadout equips right weapon before death"),
		FullLoadout.PC->GetEquippedPartIDBySlot(EPartSlot::RightWeapon),
		VectorCannon);
	TestEqual(TEXT("full loadout equips drone internal core before death"),
		FullLoadout.Drone->GetEquippedCorePartIDForTest(),
		CoreZenith);
	TestEqual(TEXT("full loadout equips drone internal left weapon before death"),
		FullLoadout.Drone->GetEquippedLeftWeaponPartIDForTest(),
		PulseLaser);
	TestEqual(TEXT("full loadout equips drone internal right weapon before death"),
		FullLoadout.Drone->GetEquippedRightWeaponPartIDForTest(),
		VectorCannon);

	ARaidBoss* Boss = FullLoadout.World->SpawnActor<ARaidBoss>();
	TestNotNull(TEXT("boss is spawned for dead attack guard"), Boss);
	if (Boss && FullLoadout.GameState)
	{
		FullLoadout.GameState->SetRaidBossForServer(Boss);
	}

	FullLoadout.Drone->ApplyDamageForServer(FullLoadout.Drone->GetMaxHealth() + 50, FName(TEXT("Automation")));

	TestTrue(TEXT("lethal damage marks drone dead"), FullLoadout.Drone->IsDead());
	TestEqual(TEXT("lethal damage clamps health to zero"), FullLoadout.Drone->GetHealth(), 0);
	TestEqual(TEXT("death return clears equipped core slot"),
		FullLoadout.PC->GetEquippedPartIDBySlot(EPartSlot::Core),
		NAME_None);
	TestEqual(TEXT("death return clears equipped left weapon slot"),
		FullLoadout.PC->GetEquippedPartIDBySlot(EPartSlot::LeftWeapon),
		NAME_None);
	TestEqual(TEXT("death return clears equipped right weapon slot"),
		FullLoadout.PC->GetEquippedPartIDBySlot(EPartSlot::RightWeapon),
		NAME_None);
	TestFalse(TEXT("death return clears drone internal loadout"),
		FullLoadout.Drone->HasEquippedLoadoutForTest());
	TestEqual(TEXT("death return clears drone internal core"),
		FullLoadout.Drone->GetEquippedCorePartIDForTest(),
		NAME_None);
	TestEqual(TEXT("death return clears drone internal left weapon"),
		FullLoadout.Drone->GetEquippedLeftWeaponPartIDForTest(),
		NAME_None);
	TestEqual(TEXT("death return clears drone internal right weapon"),
		FullLoadout.Drone->GetEquippedRightWeaponPartIDForTest(),
		NAME_None);
	TestEqual(TEXT("death return restores core stock"),
		FullLoadout.Inventory->GetCurrentCount(CoreZenith),
		FullLoadout.Inventory->GetMaxCount(CoreZenith));
	TestEqual(TEXT("death return restores left weapon stock"),
		FullLoadout.Inventory->GetCurrentCount(PulseLaser),
		FullLoadout.Inventory->GetMaxCount(PulseLaser));
	TestEqual(TEXT("death return restores right weapon stock"),
		FullLoadout.Inventory->GetCurrentCount(VectorCannon),
		FullLoadout.Inventory->GetMaxCount(VectorCannon));

	const float BossHPAfterDeath = Boss ? Boss->GetCurrentHP() : 0.0f;
	FullLoadout.Drone->RequestAttackBoss();
	if (Boss)
	{
		TestTrue(TEXT("dead attack is ignored before boss damage"),
			FMath::IsNearlyEqual(Boss->GetCurrentHP(), BossHPAfterDeath, 0.01f));
	}
	TestFalse(TEXT("dead dodge is ignored"), FullLoadout.Drone->RequestDodgeForServer());
	TestFalse(TEXT("dead heal is ignored"), FullLoadout.Drone->HealForServer(50));
	TestEqual(TEXT("dead heal does not revive the drone"), FullLoadout.Drone->GetHealth(), 0);

	TestFalse(TEXT("logout after death skips because equipped slots were cleared"),
		FullLoadout.PC->ReturnEquippedPartsForServer(EDronePartReturnReason::Disconnect));
	if (ARaidGameMode* DeathLogoutGameMode = FullLoadout.World->SpawnActor<ARaidGameMode>())
	{
		FullLoadout.World->AddController(FullLoadout.PC);
		DeathLogoutGameMode->Logout(FullLoadout.PC);
	}
	TestEqual(TEXT("logout after death does not over-return core stock"),
		FullLoadout.Inventory->GetCurrentCount(CoreZenith),
		FullLoadout.Inventory->GetMaxCount(CoreZenith));
	TestEqual(TEXT("logout after death does not over-return left stock"),
		FullLoadout.Inventory->GetCurrentCount(PulseLaser),
		FullLoadout.Inventory->GetMaxCount(PulseLaser));
	TestEqual(TEXT("logout after death does not over-return right stock"),
		FullLoadout.Inventory->GetCurrentCount(VectorCannon),
		FullLoadout.Inventory->GetMaxCount(VectorCannon));
	TestFalse(TEXT("logout after death keeps drone internal loadout clear"),
		FullLoadout.Drone->HasEquippedLoadoutForTest());
	DestroyDroneSelectionTestContext(FullLoadout);

	UDronePartReturnManager* EmptyReturnManager = nullptr;
	FDroneSelectionTestContext EmptyLoadout = CreateDroneReturnTestContext(TEXT("DroneDeathReturnEmptyLoadoutWorld"), EmptyReturnManager);
	TestNotNull(TEXT("empty loadout world is created"), EmptyLoadout.World);
	TestNotNull(TEXT("empty loadout inventory is spawned"), EmptyLoadout.Inventory);
	TestNotNull(TEXT("empty loadout player controller is spawned"), EmptyLoadout.PC);
	TestNotNull(TEXT("empty loadout drone is spawned"), EmptyLoadout.Drone);
	if (!EmptyLoadout.World || !EmptyLoadout.Inventory || !EmptyLoadout.PC || !EmptyLoadout.Drone)
	{
		DestroyDroneSelectionTestContext(EmptyLoadout);
		return false;
	}

	const int32 EmptyPulseCountBeforeDeath = EmptyLoadout.Inventory->GetCurrentCount(PulseLaser);
	EmptyLoadout.PC->Server_RequestReadyForRaid_Implementation();
	EmptyLoadout.Drone->ApplyDamageForServer(999, FName(TEXT("Automation")));
	TestTrue(TEXT("default drone can die without equipped parts"), EmptyLoadout.Drone->IsDead());
	TestFalse(TEXT("default drone death leaves internal loadout empty"),
		EmptyLoadout.Drone->HasEquippedLoadoutForTest());
	TestEqual(TEXT("default drone death does not change weapon stock"),
		EmptyLoadout.Inventory->GetCurrentCount(PulseLaser),
		EmptyPulseCountBeforeDeath);
	TestFalse(TEXT("default drone death leaves equipped return as no-op"),
		EmptyLoadout.PC->ReturnEquippedPartsForServer(EDronePartReturnReason::Disconnect));
	DestroyDroneSelectionTestContext(EmptyLoadout);

	UDronePartReturnManager* PartialReturnManager = nullptr;
	FDroneSelectionTestContext PartialLoadout = CreateDroneReturnTestContext(TEXT("DroneDeathReturnPartialLoadoutWorld"), PartialReturnManager);
	TestNotNull(TEXT("partial loadout world is created"), PartialLoadout.World);
	TestNotNull(TEXT("partial loadout inventory is spawned"), PartialLoadout.Inventory);
	TestNotNull(TEXT("partial loadout player controller is spawned"), PartialLoadout.PC);
	TestNotNull(TEXT("partial loadout drone is spawned"), PartialLoadout.Drone);
	if (!PartialLoadout.World || !PartialLoadout.Inventory || !PartialLoadout.PC || !PartialLoadout.Drone)
	{
		DestroyDroneSelectionTestContext(PartialLoadout);
		return false;
	}

	TestTrue(TEXT("partial death test consumes one weapon"), PartialLoadout.Inventory->TryConsumePart(PulseLaser));
	PartialLoadout.PC->SetSelectedPartIDForSlotForServer(EPartSlot::LeftWeapon, PulseLaser);
	PartialLoadout.PC->Server_RequestReadyForRaid_Implementation();
	PartialLoadout.Drone->ApplyDamageForServer(999, FName(TEXT("Automation")));
	TestTrue(TEXT("partial loadout drone dies"), PartialLoadout.Drone->IsDead());
	TestEqual(TEXT("partial death clears left weapon only"),
		PartialLoadout.PC->GetEquippedPartIDBySlot(EPartSlot::LeftWeapon),
		NAME_None);
	TestFalse(TEXT("partial death clears drone internal loadout"),
		PartialLoadout.Drone->HasEquippedLoadoutForTest());
	TestEqual(TEXT("partial death leaves empty core slot empty"),
		PartialLoadout.PC->GetEquippedPartIDBySlot(EPartSlot::Core),
		NAME_None);
	TestEqual(TEXT("partial death restores the one equipped weapon"),
		PartialLoadout.Inventory->GetCurrentCount(PulseLaser),
		PartialLoadout.Inventory->GetMaxCount(PulseLaser));
	DestroyDroneSelectionTestContext(PartialLoadout);

	UDronePartReturnManager* DuplicateReturnManager = nullptr;
	FDroneSelectionTestContext DuplicateLoadout = CreateDroneReturnTestContext(TEXT("DroneDeathReturnDuplicateWeaponWorld"), DuplicateReturnManager);
	TestNotNull(TEXT("duplicate loadout world is created"), DuplicateLoadout.World);
	TestNotNull(TEXT("duplicate loadout inventory is spawned"), DuplicateLoadout.Inventory);
	TestNotNull(TEXT("duplicate loadout player controller is spawned"), DuplicateLoadout.PC);
	TestNotNull(TEXT("duplicate loadout drone is spawned"), DuplicateLoadout.Drone);
	if (!DuplicateLoadout.World || !DuplicateLoadout.Inventory || !DuplicateLoadout.PC || !DuplicateLoadout.Drone)
	{
		DestroyDroneSelectionTestContext(DuplicateLoadout);
		return false;
	}

	TestTrue(TEXT("duplicate death test consumes first left/right weapon"), DuplicateLoadout.Inventory->TryConsumePart(PulseLaser));
	TestTrue(TEXT("duplicate death test consumes second left/right weapon"), DuplicateLoadout.Inventory->TryConsumePart(PulseLaser));
	DuplicateLoadout.PC->SetSelectedPartIDForSlotForServer(EPartSlot::LeftWeapon, PulseLaser);
	DuplicateLoadout.PC->SetSelectedPartIDForSlotForServer(EPartSlot::RightWeapon, PulseLaser);
	DuplicateLoadout.PC->Server_RequestReadyForRaid_Implementation();
	TestEqual(TEXT("duplicate weapons consume two stock entries before death"),
		DuplicateLoadout.Inventory->GetCurrentCount(PulseLaser),
		DuplicateLoadout.Inventory->GetMaxCount(PulseLaser) - 2);

	DuplicateLoadout.Drone->ApplyDamageForServer(999, FName(TEXT("Automation")));
	TestTrue(TEXT("duplicate weapon drone dies"), DuplicateLoadout.Drone->IsDead());
	TestEqual(TEXT("duplicate death clears left duplicate weapon"),
		DuplicateLoadout.PC->GetEquippedPartIDBySlot(EPartSlot::LeftWeapon),
		NAME_None);
	TestEqual(TEXT("duplicate death clears right duplicate weapon"),
		DuplicateLoadout.PC->GetEquippedPartIDBySlot(EPartSlot::RightWeapon),
		NAME_None);
	TestFalse(TEXT("duplicate death clears drone internal loadout"),
		DuplicateLoadout.Drone->HasEquippedLoadoutForTest());
	TestEqual(TEXT("duplicate left/right weapons return exactly two stock entries without exceeding max"),
		DuplicateLoadout.Inventory->GetCurrentCount(PulseLaser),
		DuplicateLoadout.Inventory->GetMaxCount(PulseLaser));
	DestroyDroneSelectionTestContext(DuplicateLoadout);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneLogoutCleanupTest,
	"DroneProto.D6.RaidGameMode.LogoutClearsDroneLoadout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneLogoutCleanupTest::RunTest(const FString& Parameters)
{
	UDronePartReturnManager* ReturnManager = nullptr;
	FDroneSelectionTestContext Context = CreateDroneReturnTestContext(TEXT("DroneLogoutCleanupWorld"), ReturnManager);
	ARaidGameMode* GameMode = Context.World ? Context.World->SpawnActor<ARaidGameMode>() : nullptr;

	TestNotNull(TEXT("logout cleanup world is created"), Context.World);
	TestNotNull(TEXT("logout cleanup inventory is spawned"), Context.Inventory);
	TestNotNull(TEXT("logout cleanup player controller is spawned"), Context.PC);
	TestNotNull(TEXT("logout cleanup drone is spawned"), Context.Drone);
	TestNotNull(TEXT("logout cleanup return manager is created"), ReturnManager);
	TestNotNull(TEXT("logout cleanup game mode is spawned"), GameMode);
	if (!Context.World || !Context.Inventory || !Context.PC || !Context.Drone || !ReturnManager || !GameMode)
	{
		DestroyDroneSelectionTestContext(Context);
		return false;
	}

	Context.World->AddController(Context.PC);

	const FName CoreZenith = ADronePartInventory::GetCoreZenithPartID();
	const FName PulseLaser = ADronePartInventory::GetPulseLaserPartID();
	const FName VectorCannon = ADronePartInventory::GetVectorCannonPartID();

	TestTrue(TEXT("logout cleanup consumes selected core"), Context.Inventory->TryConsumePart(CoreZenith));
	TestTrue(TEXT("logout cleanup consumes selected left weapon"), Context.Inventory->TryConsumePart(PulseLaser));
	TestTrue(TEXT("logout cleanup consumes selected right weapon"), Context.Inventory->TryConsumePart(VectorCannon));
	Context.PC->SetSelectedPartIDForSlotForServer(EPartSlot::Core, CoreZenith);
	Context.PC->SetSelectedPartIDForSlotForServer(EPartSlot::LeftWeapon, PulseLaser);
	Context.PC->SetSelectedPartIDForSlotForServer(EPartSlot::RightWeapon, VectorCannon);
	Context.PC->Server_RequestReadyForRaid_Implementation();

	TestEqual(TEXT("logout cleanup player is InBattle before logout"),
		Context.PC->GetCurrentSelectionState(),
		EPlayerSelectionState::InBattle);
	TestEqual(TEXT("ready clears selected core before logout"),
		Context.PC->GetSelectedPartIDBySlot(EPartSlot::Core),
		NAME_None);
	TestEqual(TEXT("ready equips PC core before logout"),
		Context.PC->GetEquippedPartIDBySlot(EPartSlot::Core),
		CoreZenith);
	TestEqual(TEXT("ready equips drone internal core before logout"),
		Context.Drone->GetEquippedCorePartIDForTest(),
		CoreZenith);
	TestEqual(TEXT("ready equips drone internal left weapon before logout"),
		Context.Drone->GetEquippedLeftWeaponPartIDForTest(),
		PulseLaser);
	TestEqual(TEXT("ready equips drone internal right weapon before logout"),
		Context.Drone->GetEquippedRightWeaponPartIDForTest(),
		VectorCannon);

	GameMode->Logout(Context.PC);

	TestEqual(TEXT("logout clears PC equipped core"),
		Context.PC->GetEquippedPartIDBySlot(EPartSlot::Core),
		NAME_None);
	TestEqual(TEXT("logout clears PC equipped left weapon"),
		Context.PC->GetEquippedPartIDBySlot(EPartSlot::LeftWeapon),
		NAME_None);
	TestEqual(TEXT("logout clears PC equipped right weapon"),
		Context.PC->GetEquippedPartIDBySlot(EPartSlot::RightWeapon),
		NAME_None);
	TestFalse(TEXT("logout clears drone internal loadout"),
		Context.Drone->HasEquippedLoadoutForTest());
	TestEqual(TEXT("logout restores core stock"),
		Context.Inventory->GetCurrentCount(CoreZenith),
		Context.Inventory->GetMaxCount(CoreZenith));
	TestEqual(TEXT("logout restores left weapon stock"),
		Context.Inventory->GetCurrentCount(PulseLaser),
		Context.Inventory->GetMaxCount(PulseLaser));
	TestEqual(TEXT("logout restores right weapon stock"),
		Context.Inventory->GetCurrentCount(VectorCannon),
		Context.Inventory->GetMaxCount(VectorCannon));
	TestFalse(TEXT("second logout cleanup skips because PC slots were cleared"),
		Context.PC->ReturnEquippedPartsForServer(EDronePartReturnReason::Disconnect));
	TestEqual(TEXT("second logout cleanup does not over-return core"),
		Context.Inventory->GetCurrentCount(CoreZenith),
		Context.Inventory->GetMaxCount(CoreZenith));

	DestroyDroneSelectionTestContext(Context);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRaidEndReturnTest,
	"DroneProto.D6.RaidGameMode.RaidEndReturnClearsInBattlePlayers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRaidEndReturnTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("RaidEndReturnTestWorld")));
	TestNotNull(TEXT("raid end test world is created"), World);
	if (!World)
	{
		return false;
	}

	ARaidGameMode* GameMode = World->SpawnActor<ARaidGameMode>();
	ARaidGameState* GameState = World->SpawnActor<ARaidGameState>();
	ADronePartInventory* Inventory = World->SpawnActor<ADronePartInventory>();
	ARaidPlayerController* BattlePC = World->SpawnActor<ARaidPlayerController>();
	ADrone* BattleDrone = World->SpawnActor<ADrone>();
	ARaidPlayerController* SelectingPC = World->SpawnActor<ARaidPlayerController>();
	ADrone* SelectingDrone = World->SpawnActor<ADrone>();
	ARaidPlayerController* PartialSelectingPC = World->SpawnActor<ARaidPlayerController>();
	ADrone* PartialSelectingDrone = World->SpawnActor<ADrone>();
	ARaidPlayerController* EmptySelectingPC = World->SpawnActor<ARaidPlayerController>();
	ADrone* EmptySelectingDrone = World->SpawnActor<ADrone>();

	TestNotNull(TEXT("raid end game mode is spawned"), GameMode);
	TestNotNull(TEXT("raid end game state is spawned"), GameState);
	TestNotNull(TEXT("raid end inventory is spawned"), Inventory);
	TestNotNull(TEXT("battle player controller is spawned"), BattlePC);
	TestNotNull(TEXT("battle drone is spawned"), BattleDrone);
	TestNotNull(TEXT("selecting player controller is spawned"), SelectingPC);
	TestNotNull(TEXT("selecting drone is spawned"), SelectingDrone);
	TestNotNull(TEXT("partial selecting player controller is spawned"), PartialSelectingPC);
	TestNotNull(TEXT("partial selecting drone is spawned"), PartialSelectingDrone);
	TestNotNull(TEXT("empty selecting player controller is spawned"), EmptySelectingPC);
	TestNotNull(TEXT("empty selecting drone is spawned"), EmptySelectingDrone);
	if (!GameMode || !GameState || !Inventory || !BattlePC || !BattleDrone || !SelectingPC || !SelectingDrone
		|| !PartialSelectingPC || !PartialSelectingDrone || !EmptySelectingPC || !EmptySelectingDrone)
	{
		World->DestroyWorld(false);
		return false;
	}

	World->SetGameState(GameState);
	GameState->SetDronePartInventory(Inventory);
	World->AddController(BattlePC);
	World->AddController(SelectingPC);
	World->AddController(PartialSelectingPC);
	World->AddController(EmptySelectingPC);
	BattlePC->Possess(BattleDrone);
	SelectingPC->Possess(SelectingDrone);
	PartialSelectingPC->Possess(PartialSelectingDrone);
	EmptySelectingPC->Possess(EmptySelectingDrone);

	const FName CoreZenith = ADronePartInventory::GetCoreZenithPartID();
	const FName CoreBooster = ADronePartInventory::GetCoreBoosterPartID();
	const FName PulseLaser = ADronePartInventory::GetPulseLaserPartID();
	const FName FractureBurst = ADronePartInventory::GetFractureBurstPartID();
	const FName VectorCannon = ADronePartInventory::GetVectorCannonPartID();

	TestTrue(TEXT("battle raid end test consumes left weapon"), Inventory->TryConsumePart(PulseLaser));
	TestTrue(TEXT("battle raid end test consumes right weapon"), Inventory->TryConsumePart(VectorCannon));
	BattlePC->SetSelectedPartIDForSlotForServer(EPartSlot::LeftWeapon, PulseLaser);
	BattlePC->SetSelectedPartIDForSlotForServer(EPartSlot::RightWeapon, VectorCannon);
	BattlePC->Server_RequestReadyForRaid_Implementation();
	ARaidBoss* Boss = World->SpawnActor<ARaidBoss>();
	TestNotNull(TEXT("raid end boss is spawned"), Boss);
	if (Boss)
	{
		GameState->SetRaidBossForServer(Boss);
	}
	TestEqual(TEXT("battle player is InBattle before RaidEnd"),
		BattlePC->GetCurrentSelectionState(),
		EPlayerSelectionState::InBattle);
	TestEqual(TEXT("Ready clears battle selected left weapon before RaidEnd"),
		BattlePC->GetSelectedPartIDBySlot(EPartSlot::LeftWeapon),
		NAME_None);
	TestEqual(TEXT("Ready moves left weapon into drone internal loadout before RaidEnd"),
		BattleDrone->GetEquippedLeftWeaponPartIDForTest(),
		PulseLaser);
	TestEqual(TEXT("Ready moves right weapon into drone internal loadout before RaidEnd"),
		BattleDrone->GetEquippedRightWeaponPartIDForTest(),
		VectorCannon);

	TestTrue(TEXT("selecting raid end test consumes selected core"), Inventory->TryConsumePart(CoreZenith));
	TestTrue(TEXT("selecting raid end test consumes selected left weapon"), Inventory->TryConsumePart(FractureBurst));
	TestTrue(TEXT("selecting raid end test consumes selected right weapon"), Inventory->TryConsumePart(VectorCannon));
	SelectingPC->SetSelectedPartIDForSlotForServer(EPartSlot::Core, CoreZenith);
	SelectingPC->SetSelectedPartIDForSlotForServer(EPartSlot::LeftWeapon, FractureBurst);
	SelectingPC->SetSelectedPartIDForSlotForServer(EPartSlot::RightWeapon, VectorCannon);
	TestEqual(TEXT("selecting player stays Selecting before RaidEnd"),
		SelectingPC->GetCurrentSelectionState(),
		EPlayerSelectionState::Selecting);
	TestFalse(TEXT("selecting drone has no internal loadout before RaidEnd"),
		SelectingDrone->HasEquippedLoadoutForTest());
	TestEqual(TEXT("selecting core stock is consumed before RaidEnd"),
		Inventory->GetCurrentCount(CoreZenith),
		Inventory->GetMaxCount(CoreZenith) - 1);
	TestEqual(TEXT("selecting left weapon stock is consumed before RaidEnd"),
		Inventory->GetCurrentCount(FractureBurst),
		Inventory->GetMaxCount(FractureBurst) - 1);
	TestEqual(TEXT("selecting right weapon stock includes battle and selecting consumes before RaidEnd"),
		Inventory->GetCurrentCount(VectorCannon),
		Inventory->GetMaxCount(VectorCannon) - 2);

	TestTrue(TEXT("partial selecting raid end test consumes selected core only"), Inventory->TryConsumePart(CoreBooster));
	PartialSelectingPC->SetSelectedPartIDForSlotForServer(EPartSlot::Core, CoreBooster);
	TestEqual(TEXT("partial selecting player stays Selecting before RaidEnd"),
		PartialSelectingPC->GetCurrentSelectionState(),
		EPlayerSelectionState::Selecting);
	TestFalse(TEXT("partial selecting drone has no internal loadout before RaidEnd"),
		PartialSelectingDrone->HasEquippedLoadoutForTest());
	TestEqual(TEXT("partial selecting left slot starts empty"),
		PartialSelectingPC->GetSelectedPartIDBySlot(EPartSlot::LeftWeapon),
		NAME_None);
	TestEqual(TEXT("empty selecting player stays Selecting before RaidEnd"),
		EmptySelectingPC->GetCurrentSelectionState(),
		EPlayerSelectionState::Selecting);

	GameMode->ReturnAllEquippedPartsForRaidEnd(FName(TEXT("Automation")));

	TestEqual(TEXT("RaidEnd clears battle left weapon"),
		BattlePC->GetEquippedPartIDBySlot(EPartSlot::LeftWeapon),
		NAME_None);
	TestEqual(TEXT("RaidEnd clears battle right weapon"),
		BattlePC->GetEquippedPartIDBySlot(EPartSlot::RightWeapon),
		NAME_None);
	TestFalse(TEXT("RaidEnd clears battle drone internal loadout"),
		BattleDrone->HasEquippedLoadoutForTest());
	TestEqual(TEXT("RaidEnd clears battle drone internal left weapon"),
		BattleDrone->GetEquippedLeftWeaponPartIDForTest(),
		NAME_None);
	TestEqual(TEXT("RaidEnd clears battle drone internal right weapon"),
		BattleDrone->GetEquippedRightWeaponPartIDForTest(),
		NAME_None);
	TestTrue(TEXT("RaidEnd moves battle player out of InBattle"),
		BattlePC->GetCurrentSelectionState() != EPlayerSelectionState::InBattle);
	TestEqual(TEXT("RaidEnd returns battle left weapon stock"),
		Inventory->GetCurrentCount(PulseLaser),
		Inventory->GetMaxCount(PulseLaser));
	TestEqual(TEXT("RaidEnd returns battle right weapon stock"),
		Inventory->GetCurrentCount(VectorCannon),
		Inventory->GetMaxCount(VectorCannon));
	TestEqual(TEXT("RaidEnd returns selecting player's selected core"),
		Inventory->GetCurrentCount(CoreZenith),
		Inventory->GetMaxCount(CoreZenith));
	TestEqual(TEXT("RaidEnd returns selecting player's selected left weapon"),
		Inventory->GetCurrentCount(FractureBurst),
		Inventory->GetMaxCount(FractureBurst));
	TestEqual(TEXT("RaidEnd returns selecting player's selected right weapon"),
		Inventory->GetCurrentCount(VectorCannon),
		Inventory->GetMaxCount(VectorCannon));
	TestEqual(TEXT("RaidEnd returns partial selecting player's selected core"),
		Inventory->GetCurrentCount(CoreBooster),
		Inventory->GetMaxCount(CoreBooster));
	TestEqual(TEXT("RaidEnd clears selecting player's selected core"),
		SelectingPC->GetSelectedPartIDBySlot(EPartSlot::Core),
		NAME_None);
	TestEqual(TEXT("RaidEnd clears selecting player's selected left weapon"),
		SelectingPC->GetSelectedPartIDBySlot(EPartSlot::LeftWeapon),
		NAME_None);
	TestEqual(TEXT("RaidEnd clears selecting player's selected right weapon"),
		SelectingPC->GetSelectedPartIDBySlot(EPartSlot::RightWeapon),
		NAME_None);
	TestEqual(TEXT("RaidEnd clears partial selecting player's selected core"),
		PartialSelectingPC->GetSelectedPartIDBySlot(EPartSlot::Core),
		NAME_None);
	TestEqual(TEXT("RaidEnd leaves partial selecting player's empty left slot empty"),
		PartialSelectingPC->GetSelectedPartIDBySlot(EPartSlot::LeftWeapon),
		NAME_None);
	TestEqual(TEXT("RaidEnd keeps empty selecting player's core slot empty"),
		EmptySelectingPC->GetSelectedPartIDBySlot(EPartSlot::Core),
		NAME_None);
	TestFalse(TEXT("RaidEnd keeps selecting drone internal loadout empty"),
		SelectingDrone->HasEquippedLoadoutForTest());
	TestFalse(TEXT("RaidEnd keeps partial selecting drone internal loadout empty"),
		PartialSelectingDrone->HasEquippedLoadoutForTest());
	TestFalse(TEXT("RaidEnd keeps empty selecting drone internal loadout empty"),
		EmptySelectingDrone->HasEquippedLoadoutForTest());
	TestEqual(TEXT("empty selecting player does not change unrelated stock"),
		Inventory->GetCurrentCount(PulseLaser),
		Inventory->GetMaxCount(PulseLaser));

	if (Boss)
	{
		const float BossHPAfterRaidEnd = Boss->GetCurrentHP();
		BattleDrone->RequestAttackBoss();
		TestTrue(TEXT("RaidEnd attack input does not damage boss"),
			FMath::IsNearlyEqual(Boss->GetCurrentHP(), BossHPAfterRaidEnd, 0.01f));
	}

	TestFalse(TEXT("RaidEnd move input is ignored before server movement"),
		BattleDrone->ApplyMoveInputForServerForTest(FVector2D(1.0f, 0.0f)));
	const FVector BattleLocationAfterRaidEnd = BattleDrone->GetActorLocation();
	TestFalse(TEXT("RaidEnd pending move is not applied"),
		BattleDrone->ApplyPendingServerMoveInputForTest(0.10f));
	TestTrue(TEXT("RaidEnd pending move leaves location unchanged"),
		BattleDrone->GetActorLocation().Equals(BattleLocationAfterRaidEnd, 0.1f));

	BattlePC->SetDronePartReturnManagerForTest(GameMode->GetDronePartReturnManager());
	TestFalse(TEXT("logout after RaidEnd skips because equipped slots were cleared"),
		BattlePC->ReturnEquippedPartsForServer(EDronePartReturnReason::Disconnect));
	GameMode->Logout(BattlePC);
	TestEqual(TEXT("logout after RaidEnd does not over-return left weapon"),
		Inventory->GetCurrentCount(PulseLaser),
		Inventory->GetMaxCount(PulseLaser));
	TestEqual(TEXT("logout after RaidEnd does not over-return right weapon"),
		Inventory->GetCurrentCount(VectorCannon),
		Inventory->GetMaxCount(VectorCannon));

	SelectingPC->SetDronePartReturnManagerForTest(GameMode->GetDronePartReturnManager());
	PartialSelectingPC->SetDronePartReturnManagerForTest(GameMode->GetDronePartReturnManager());
	EmptySelectingPC->SetDronePartReturnManagerForTest(GameMode->GetDronePartReturnManager());
	TestFalse(TEXT("selecting logout after RaidEnd skips because selected slots were cleared"),
		SelectingPC->ReturnSelectedPartsForServer(EDronePartReturnReason::Disconnect));
	TestFalse(TEXT("partial selecting logout after RaidEnd skips because selected slot was cleared"),
		PartialSelectingPC->ReturnSelectedPartsForServer(EDronePartReturnReason::Disconnect));
	TestFalse(TEXT("empty selected slots skip without changing stock"),
		EmptySelectingPC->ReturnSelectedPartsForServer(EDronePartReturnReason::Disconnect));
	GameMode->Logout(SelectingPC);
	TestEqual(TEXT("selecting logout after RaidEnd does not over-return core"),
		Inventory->GetCurrentCount(CoreZenith),
		Inventory->GetMaxCount(CoreZenith));
	TestEqual(TEXT("partial selecting logout after RaidEnd does not over-return core"),
		Inventory->GetCurrentCount(CoreBooster),
		Inventory->GetMaxCount(CoreBooster));
	TestEqual(TEXT("empty selecting skip leaves pulse stock at max"),
		Inventory->GetCurrentCount(PulseLaser),
		Inventory->GetMaxCount(PulseLaser));
	TestFalse(TEXT("logout after RaidEnd keeps battle drone internal loadout clear"),
		BattleDrone->HasEquippedLoadoutForTest());
	TestFalse(TEXT("logout after RaidEnd keeps selecting drone internal loadout clear"),
		SelectingDrone->HasEquippedLoadoutForTest());

	GameMode->ReturnAllEquippedPartsForRaidEnd(FName(TEXT("AutomationRetry")));
	TestEqual(TEXT("second RaidEnd does not over-return left weapon"),
		Inventory->GetCurrentCount(PulseLaser),
		Inventory->GetMaxCount(PulseLaser));
	TestEqual(TEXT("second RaidEnd does not over-return right weapon"),
		Inventory->GetCurrentCount(VectorCannon),
		Inventory->GetMaxCount(VectorCannon));
	TestEqual(TEXT("second RaidEnd does not over-return selecting core"),
		Inventory->GetCurrentCount(CoreZenith),
		Inventory->GetMaxCount(CoreZenith));
	TestEqual(TEXT("second RaidEnd does not over-return partial core"),
		Inventory->GetCurrentCount(CoreBooster),
		Inventory->GetMaxCount(CoreBooster));

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRaidTimeLimitEndTest,
	"DroneProto.D12.RaidGameMode.RaidTimeLimitEndsRaid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRaidTimeLimitEndTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("RaidTimeLimitEndWorld")));
	TestNotNull(TEXT("raid time limit world is created"), World);
	if (!World)
	{
		return false;
	}

	ARaidGameMode* GameMode = World->SpawnActor<ARaidGameMode>();
	ARaidGameState* GameState = World->SpawnActor<ARaidGameState>();
	ADronePartInventory* Inventory = World->SpawnActor<ADronePartInventory>();
	ARaidPlayerController* BattlePC = World->SpawnActor<ARaidPlayerController>();
	ADrone* BattleDrone = World->SpawnActor<ADrone>();
	ARaidPlayerController* SelectingPC = World->SpawnActor<ARaidPlayerController>();
	ADrone* SelectingDrone = World->SpawnActor<ADrone>();
	ARaidPlayerController* DeadPC = World->SpawnActor<ARaidPlayerController>();
	ADrone* DeadDrone = World->SpawnActor<ADrone>();

	TestNotNull(TEXT("raid time limit game mode is spawned"), GameMode);
	TestNotNull(TEXT("raid time limit game state is spawned"), GameState);
	TestNotNull(TEXT("raid time limit inventory is spawned"), Inventory);
	TestNotNull(TEXT("raid time limit battle PC is spawned"), BattlePC);
	TestNotNull(TEXT("raid time limit battle drone is spawned"), BattleDrone);
	TestNotNull(TEXT("raid time limit selecting PC is spawned"), SelectingPC);
	TestNotNull(TEXT("raid time limit selecting drone is spawned"), SelectingDrone);
	TestNotNull(TEXT("raid time limit dead PC is spawned"), DeadPC);
	TestNotNull(TEXT("raid time limit dead drone is spawned"), DeadDrone);
	if (!GameMode || !GameState || !Inventory || !BattlePC || !BattleDrone || !SelectingPC || !SelectingDrone || !DeadPC || !DeadDrone)
	{
		World->DestroyWorld(false);
		return false;
	}

	World->SetGameState(GameState);
	GameState->SetDronePartInventory(Inventory);
	World->AddController(BattlePC);
	World->AddController(SelectingPC);
	World->AddController(DeadPC);
	BattlePC->Possess(BattleDrone);
	SelectingPC->Possess(SelectingDrone);
	DeadPC->Possess(DeadDrone);

	TestTrue(TEXT("raid time limit is test-configurable"),
		SetFloatPropertyForAutomationTest(GameMode, FName(TEXT("RaidTimeLimitSeconds")), 0.05f));

	const FName CoreZenith = ADronePartInventory::GetCoreZenithPartID();
	const FName PulseLaser = ADronePartInventory::GetPulseLaserPartID();
	const FName VectorCannon = ADronePartInventory::GetVectorCannonPartID();

	TestTrue(TEXT("raid time limit consumes battle core"), Inventory->TryConsumePart(CoreZenith));
	TestTrue(TEXT("raid time limit consumes battle left weapon"), Inventory->TryConsumePart(PulseLaser));
	BattlePC->SetSelectedPartIDForSlotForServer(EPartSlot::Core, CoreZenith);
	BattlePC->SetSelectedPartIDForSlotForServer(EPartSlot::LeftWeapon, PulseLaser);
	BattlePC->Server_RequestReadyForRaid_Implementation();

	TestTrue(TEXT("raid time limit consumes selecting right weapon"), Inventory->TryConsumePart(VectorCannon));
	SelectingPC->SetSelectedPartIDForSlotForServer(EPartSlot::RightWeapon, VectorCannon);

	DeadPC->Server_RequestReadyForRaid_Implementation();
	DeadDrone->ApplyDamageForServer(999, FName(TEXT("Automation")));
	TestTrue(TEXT("dead player receives death report before raid time limit"),
		DeadPC->HasDroneReportGeneratedForTest());
	const FDroneReportData DeathReport = DeadPC->GetLastDroneReportDataForTest();

	TestEqual(TEXT("ready starts Battle before raid time limit expires"),
		GameState->RaidState,
		ERaidState::Battle);
	TestTrue(TEXT("ready starts raid time limit timer"),
		GameMode->IsRaidTimeLimitTimerActiveForTest());

	GameMode->ExpireRaidTimeLimitForTest();

	TestEqual(TEXT("raid time limit moves raid to End"),
		GameState->RaidState,
		ERaidState::End);
	TestTrue(TEXT("surviving battle player receives raid time limit report"),
		BattlePC->HasDroneReportGeneratedForTest());
	TestFalse(TEXT("raid time limit report does not mark BossSlayer bonus"),
		BattlePC->GetLastDroneReportDataForTest().AchievedBonusList.Contains(EDroneReportBonusType::BossSlayer));
	TestFalse(TEXT("selecting player does not receive raid time limit report"),
		SelectingPC->HasDroneReportGeneratedForTest());
	TestTrue(TEXT("dead player keeps original death report"),
		DeadPC->HasDroneReportGeneratedForTest());
	TestTrue(TEXT("raid time limit does not replace death report"),
		FMath::IsNearlyEqual(DeadPC->GetLastDroneReportDataForTest().ReportScore, DeathReport.ReportScore, 0.01f));

	TestEqual(TEXT("raid time limit returns battle core"),
		Inventory->GetCurrentCount(CoreZenith),
		Inventory->GetMaxCount(CoreZenith));
	TestEqual(TEXT("raid time limit returns battle left weapon"),
		Inventory->GetCurrentCount(PulseLaser),
		Inventory->GetMaxCount(PulseLaser));
	TestEqual(TEXT("raid time limit returns selecting right weapon"),
		Inventory->GetCurrentCount(VectorCannon),
		Inventory->GetMaxCount(VectorCannon));
	TestFalse(TEXT("raid time limit clears battle drone loadout"),
		BattleDrone->HasEquippedLoadoutForTest());
	TestEqual(TEXT("raid time limit clears battle PC equipped core"),
		BattlePC->GetEquippedPartIDBySlot(EPartSlot::Core),
		NAME_None);
	TestEqual(TEXT("raid time limit clears selecting selected right weapon"),
		SelectingPC->GetSelectedPartIDBySlot(EPartSlot::RightWeapon),
		NAME_None);

	GameMode->Logout(BattlePC);
	GameMode->Logout(SelectingPC);
	TestEqual(TEXT("logout after raid time limit does not over-return battle core"),
		Inventory->GetCurrentCount(CoreZenith),
		Inventory->GetMaxCount(CoreZenith));
	TestEqual(TEXT("logout after raid time limit does not over-return selecting weapon"),
		Inventory->GetCurrentCount(VectorCannon),
		Inventory->GetMaxCount(VectorCannon));

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRaidTimeLimitBossDefeatedGuardTest,
	"DroneProto.D12.RaidGameMode.RaidTimeLimitDoesNotDuplicateBossDefeated",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRaidTimeLimitBossDefeatedGuardTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("RaidTimeLimitBossDefeatedWorld")));
	TestNotNull(TEXT("raid boss defeated timer guard world is created"), World);
	if (!World)
	{
		return false;
	}

	ARaidGameMode* GameMode = World->SpawnActor<ARaidGameMode>();
	ARaidGameState* GameState = World->SpawnActor<ARaidGameState>();
	ADronePartInventory* Inventory = World->SpawnActor<ADronePartInventory>();
	ARaidPlayerController* BattlePC = World->SpawnActor<ARaidPlayerController>();
	ADrone* BattleDrone = World->SpawnActor<ADrone>();

	TestNotNull(TEXT("boss defeated timer guard game mode is spawned"), GameMode);
	TestNotNull(TEXT("boss defeated timer guard game state is spawned"), GameState);
	TestNotNull(TEXT("boss defeated timer guard inventory is spawned"), Inventory);
	TestNotNull(TEXT("boss defeated timer guard battle PC is spawned"), BattlePC);
	TestNotNull(TEXT("boss defeated timer guard battle drone is spawned"), BattleDrone);
	if (!GameMode || !GameState || !Inventory || !BattlePC || !BattleDrone)
	{
		World->DestroyWorld(false);
		return false;
	}

	World->SetGameState(GameState);
	GameState->SetDronePartInventory(Inventory);
	World->AddController(BattlePC);
	BattlePC->Possess(BattleDrone);

	TestTrue(TEXT("boss defeated timer guard raid time limit is test-configurable"),
		SetFloatPropertyForAutomationTest(GameMode, FName(TEXT("RaidTimeLimitSeconds")), 0.05f));

	const FName PulseLaser = ADronePartInventory::GetPulseLaserPartID();
	TestTrue(TEXT("boss defeated timer guard consumes left weapon"), Inventory->TryConsumePart(PulseLaser));
	BattlePC->SetSelectedPartIDForSlotForServer(EPartSlot::LeftWeapon, PulseLaser);
	BattlePC->Server_RequestReadyForRaid_Implementation();

	GameMode->HandleBossDefeatedForServer();

	TestEqual(TEXT("boss defeated ends raid before timer"),
		GameState->RaidState,
		ERaidState::End);
	TestFalse(TEXT("boss defeated clears raid time limit timer"),
		GameMode->IsRaidTimeLimitTimerActiveForTest());
	TestTrue(TEXT("boss defeated creates report"),
		BattlePC->HasDroneReportGeneratedForTest());
	const FDroneReportData BossReport = BattlePC->GetLastDroneReportDataForTest();
	TestEqual(TEXT("boss defeated returns left weapon"),
		Inventory->GetCurrentCount(PulseLaser),
		Inventory->GetMaxCount(PulseLaser));

	GameMode->ExpireRaidTimeLimitForTest();

	TestTrue(TEXT("timer after boss defeated does not replace report"),
		FMath::IsNearlyEqual(BattlePC->GetLastDroneReportDataForTest().ReportScore, BossReport.ReportScore, 0.01f));
	TestEqual(TEXT("timer after boss defeated preserves report bonus score"),
		BattlePC->GetLastDroneReportDataForTest().BonusScore,
		BossReport.BonusScore);
	TestEqual(TEXT("timer after boss defeated preserves report bonus count"),
		BattlePC->GetLastDroneReportDataForTest().AchievedBonusList.Num(),
		BossReport.AchievedBonusList.Num());
	TestEqual(TEXT("timer after boss defeated does not over-return left weapon"),
		Inventory->GetCurrentCount(PulseLaser),
		Inventory->GetMaxCount(PulseLaser));

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneRaidSummaryLogSourceTest,
	"DroneProto.D5.ManualSummaryLogs.SourceMarkers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneRaidSummaryLogSourceTest::RunTest(const FString& Parameters)
{
	const FString SourceRoot = FPaths::ProjectDir() / TEXT("Source/DroneProto");

	auto LoadSourceFile = [this, SourceRoot](const TCHAR* RelativePath, FString& OutContents) -> bool
	{
		const FString FullPath = SourceRoot / RelativePath;
		const bool bLoaded = FFileHelper::LoadFileToString(OutContents, *FullPath);
		TestTrue(FString::Printf(TEXT("%s loads"), RelativePath), bLoaded);
		return bLoaded;
	};

	FString RaidGameModeSource;
	FString RaidPlayerControllerSource;
	FString RaidPlayerControllerHeaderSource;
	FString DronePartReturnManagerSource;
	FString DronePartSelectWidgetSource;
	FString DroneReportWidgetSource;
	FString DroneSource;
	FString RaidBossSource;
	FString RaidBossAttackTelegraphSource;
	FString RaidSessionSubsystemSource;
	FString LocalAssignmentSource;
	FString ServerEndpointSource;
	FString LobbyPlayerControllerHeaderSource;
	FString LobbyPlayerControllerSource;
	FString RaidLobbyWidgetSource;

	if (!LoadSourceFile(TEXT("Raid/RaidGameMode.cpp"), RaidGameModeSource)
		|| !LoadSourceFile(TEXT("Raid/RaidPlayerController.cpp"), RaidPlayerControllerSource)
		|| !LoadSourceFile(TEXT("Raid/RaidPlayerController.h"), RaidPlayerControllerHeaderSource)
		|| !LoadSourceFile(TEXT("Raid/DronePartReturnManager.cpp"), DronePartReturnManagerSource)
		|| !LoadSourceFile(TEXT("Raid/DronePartSelectWidget.cpp"), DronePartSelectWidgetSource)
		|| !LoadSourceFile(TEXT("Raid/DroneReportWidget.cpp"), DroneReportWidgetSource)
		|| !LoadSourceFile(TEXT("Drone.cpp"), DroneSource)
		|| !LoadSourceFile(TEXT("Raid/RaidBoss.cpp"), RaidBossSource)
		|| !LoadSourceFile(TEXT("Raid/RaidBossAttackTelegraph.cpp"), RaidBossAttackTelegraphSource)
		|| !LoadSourceFile(TEXT("Lobby/RaidSessionSubsystem.cpp"), RaidSessionSubsystemSource)
		|| !LoadSourceFile(TEXT("Lobby/LocalAssignment.cpp"), LocalAssignmentSource)
		|| !LoadSourceFile(TEXT("Lobby/ServerEndpoint.h"), ServerEndpointSource)
		|| !LoadSourceFile(TEXT("Lobby/LobbyPlayerController.h"), LobbyPlayerControllerHeaderSource)
		|| !LoadSourceFile(TEXT("Lobby/LobbyPlayerController.cpp"), LobbyPlayerControllerSource)
		|| !LoadSourceFile(TEXT("Lobby/RaidLobbyWidget.cpp"), RaidLobbyWidgetSource))
	{
		return false;
	}

	TestTrue(TEXT("spawn summary log marker exists"),
		RaidGameModeSource.Contains(TEXT("[DR_SUMMARY] Spawn PC=")));
	TestTrue(TEXT("select summary log marker exists"),
		RaidPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] Select PC=")));
	TestTrue(TEXT("cancel summary log marker exists"),
		RaidPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] Cancel PC=")));
	TestTrue(TEXT("ready summary log marker exists"),
		RaidPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] Ready PC=")));
	TestTrue(TEXT("ready ignored dead pawn summary log marker exists"),
		RaidPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] ReadyIgnored PC=")));
	TestTrue(TEXT("return summary log marker exists"),
		DronePartReturnManagerSource.Contains(TEXT("ToReturnSummaryLogName")));
	TestTrue(TEXT("attack calculation summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] AttackCalc Player=")));
	TestTrue(TEXT("attack accepted summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] Attack Accepted: Player=")));
	TestTrue(TEXT("attack no damage summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] Attack NoDamage:")));
	TestTrue(TEXT("attack ignored summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] AttackIgnored PC=")));
	TestTrue(TEXT("not in battle attack ignored summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] Attack Ignored: Reason=NotInBattle")));
	TestTrue(TEXT("raid end attack ignored summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] Attack Ignored: Reason=RaidEnd")));
	TestTrue(TEXT("boss dead attack ignored summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] Attack Ignored: Reason=%s"))
		&& DroneSource.Contains(TEXT("FName(TEXT(\"BossDead\"))")));
	TestTrue(TEXT("no target attack ignored summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] Attack Ignored: Reason=%s"))
		&& DroneSource.Contains(TEXT("FName(TEXT(\"NoTarget\"))")));
	TestTrue(TEXT("invalid pawn attack failed summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] Attack Failed: Reason=InvalidPawn")));
	TestTrue(TEXT("attack path checks PlayerSelectionState"),
		DroneSource.Contains(TEXT("GetPlayerSelectionState()")));
	TestTrue(TEXT("boss damage summary log marker exists"),
		RaidBossSource.Contains(TEXT("[DR_SUMMARY] BossDamage: OldHP=")));
	TestTrue(TEXT("boss death summary log marker exists"),
		RaidBossSource.Contains(TEXT("[DR_SUMMARY] BossDeath:")));
	TestTrue(TEXT("dead boss damage ignored summary log marker exists"),
		RaidBossSource.Contains(TEXT("[DR_SUMMARY] BossDamageIgnored: Reason=BossDead")));
	TestTrue(TEXT("zero boss damage is separated from BossDamage marker"),
		RaidBossSource.Contains(TEXT("[DR_SUMMARY] BossDamageIgnored: Reason=NoDamage")));
	TestTrue(TEXT("D20 combat visual PIE checklist marker exists"),
		RaidBossSource.Contains(TEXT("[DR_SUMMARY] D20PIEChecklist CombatVisual")));
	TestTrue(TEXT("boss area attack summary log marker exists"),
		RaidBossSource.Contains(TEXT("[DR_SUMMARY] BossAttack")));
	TestTrue(TEXT("boss area attack hit summary log marker exists"),
		RaidBossSource.Contains(TEXT("[DR_SUMMARY] BossAttackHit")));
	TestTrue(TEXT("boss area attack miss summary log marker exists"),
		RaidBossSource.Contains(TEXT("[DR_SUMMARY] BossAttackMiss")));
	TestTrue(TEXT("boss area attack ignored summary log marker exists"),
		RaidBossSource.Contains(TEXT("[DR_SUMMARY] BossAttackIgnored")));
	TestTrue(TEXT("boss telegraph start summary log marker exists"),
		RaidBossSource.Contains(TEXT("[DR_SUMMARY] BossTelegraphStart Center=")));
	TestTrue(TEXT("boss telegraph spawned summary log marker exists"),
		RaidBossSource.Contains(TEXT("[DR_SUMMARY] BossTelegraphSpawned Actor=")));
	TestTrue(TEXT("boss telegraph execute summary log marker exists"),
		RaidBossSource.Contains(TEXT("[DR_SUMMARY] BossAttackExecute Reason=TelegraphExpired")));
	TestTrue(TEXT("boss telegraph expired summary log marker exists"),
		RaidBossAttackTelegraphSource.Contains(TEXT("[DR_SUMMARY] BossTelegraphExpired Actor=")));
	TestTrue(TEXT("boss telegraph actor replicates radius"),
		RaidBossAttackTelegraphSource.Contains(TEXT("DOREPLIFETIME(ARaidBossAttackTelegraph, RadiusCm)")));
	TestTrue(TEXT("boss telegraph actor has optional static mesh component"),
		RaidBossAttackTelegraphSource.Contains(TEXT("CreateDefaultSubobject<UStaticMeshComponent>")));
	TestTrue(TEXT("boss telegraph actor has visible text render component"),
		RaidBossAttackTelegraphSource.Contains(TEXT("CreateDefaultSubobject<UTextRenderComponent>")));
	TestTrue(TEXT("boss telegraph actor applies radius actor scale"),
		RaidBossAttackTelegraphSource.Contains(TEXT("SetActorScale3D")));
	TestTrue(TEXT("boss telegraph debug trigger summary log marker exists"),
		RaidPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] BossTelegraphDebugTrigger")));
	TestTrue(TEXT("boss telegraph debug trigger callable exists"),
		RaidPlayerControllerHeaderSource.Contains(TEXT("DebugTriggerBossTelegraphAttack")));
	TestTrue(TEXT("boss telegraph debug-only server RPC exists"),
		RaidPlayerControllerHeaderSource.Contains(TEXT("Server_DebugTriggerBossTelegraphAttack")));
	TestTrue(TEXT("raid entry request summary log marker exists"),
		RaidSessionSubsystemSource.Contains(TEXT("[DR_SUMMARY] RaidEntryRequest")));
	TestTrue(TEXT("raid assignment result summary log marker exists"),
		RaidSessionSubsystemSource.Contains(TEXT("[DR_SUMMARY] RaidAssignmentResult")));
	TestTrue(TEXT("raid server state summary log marker exists"),
		LocalAssignmentSource.Contains(TEXT("[DR_SUMMARY] RaidServerState")));
	TestTrue(TEXT("raid assignment result records server state"),
		RaidSessionSubsystemSource.Contains(TEXT("ServerState=%s")));
	TestTrue(TEXT("raid assignment result records local prototype authority"),
		RaidSessionSubsystemSource.Contains(TEXT("AuthorityModel=LocalPrototype")));
	TestTrue(TEXT("raid server availability type exists outside RaidGameState"),
		ServerEndpointSource.Contains(TEXT("FRaidServerAvailability")));
	TestTrue(TEXT("raid entry travel summary log marker exists"),
		RaidSessionSubsystemSource.Contains(TEXT("[DR_SUMMARY] RaidEntryTravel")));
	TestTrue(TEXT("raid entry wait summary log marker exists"),
		RaidSessionSubsystemSource.Contains(TEXT("[DR_SUMMARY] RaidEntryWait")));
	TestTrue(TEXT("raid entry retry summary log marker exists"),
		RaidSessionSubsystemSource.Contains(TEXT("[DR_SUMMARY] RaidEntryRetry")));
	TestTrue(TEXT("raid entry fail summary log marker exists"),
		RaidSessionSubsystemSource.Contains(TEXT("[DR_SUMMARY] RaidEntryFail")));
	TestTrue(TEXT("raid entry cancel summary log marker exists"),
		RaidSessionSubsystemSource.Contains(TEXT("[DR_SUMMARY] RaidEntryCancel")));
	TestTrue(TEXT("lobby widget class is reflected for Blueprint assignment"),
		LobbyPlayerControllerHeaderSource.Contains(TEXT("UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=\"UI\")")));
	TestTrue(TEXT("lobby begin play summary log marker exists"),
		LobbyPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] LobbyPCBeginPlay")));
	TestTrue(TEXT("lobby widget create attempt summary log marker exists"),
		LobbyPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] LobbyWidgetCreateAttempt")));
	TestTrue(TEXT("lobby widget missing class summary log marker exists"),
		LobbyPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] LobbyWidgetMissingClass")));
	TestTrue(TEXT("lobby widget create failed summary log marker exists"),
		LobbyPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] LobbyWidgetCreateFailed")));
	TestTrue(TEXT("lobby widget shown summary log marker exists"),
		LobbyPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] LobbyWidgetShown")));
	TestTrue(TEXT("raid lobby native construct summary log marker exists"),
		RaidLobbyWidgetSource.Contains(TEXT("[DR_SUMMARY] RaidLobbyWidgetNativeConstruct")));
	TestTrue(TEXT("raid lobby request entry click summary log marker exists"),
		RaidLobbyWidgetSource.Contains(TEXT("[DR_SUMMARY] RaidLobbyRequestEntryClicked")));
	TestTrue(TEXT("raid lobby UI state summary log marker exists"),
		RaidLobbyWidgetSource.Contains(TEXT("[DR_SUMMARY] LobbyUIState State=")));
	TestTrue(TEXT("raid lobby main state helper exists"),
		RaidLobbyWidgetSource.Contains(TEXT("ShowMainLobby")));
	TestTrue(TEXT("raid lobby waiting state helper exists"),
		RaidLobbyWidgetSource.Contains(TEXT("ShowWaitingPopup")));
	TestTrue(TEXT("raid lobby no-server state helper exists"),
		RaidLobbyWidgetSource.Contains(TEXT("ShowNoServerPopup")));
	TestTrue(TEXT("raid lobby loading state helper exists"),
		RaidLobbyWidgetSource.Contains(TEXT("ShowLoading")));
	TestTrue(TEXT("raid lobby main panel optional binding exists"),
		RaidLobbyWidgetSource.Contains(TEXT("MainLobbyPanel")));
	TestTrue(TEXT("raid lobby waiting popup optional binding exists"),
		RaidLobbyWidgetSource.Contains(TEXT("WaitingPopupPanel")));
	TestTrue(TEXT("raid lobby no-server popup optional binding exists"),
		RaidLobbyWidgetSource.Contains(TEXT("NoServerPopupPanel")));
	TestTrue(TEXT("raid lobby loading panel optional binding exists"),
		RaidLobbyWidgetSource.Contains(TEXT("LoadingPanel")));
	TestTrue(TEXT("raid lobby join button optional binding exists"),
		RaidLobbyWidgetSource.Contains(TEXT("RaidJoinButton")));
	TestTrue(TEXT("raid lobby cancel matchmaking button optional binding exists"),
		RaidLobbyWidgetSource.Contains(TEXT("CancelMatchmakingButton")));
	TestTrue(TEXT("raid lobby no-server confirm button optional binding exists"),
		RaidLobbyWidgetSource.Contains(TEXT("NoServerConfirmButton")));
	TestTrue(TEXT("raid session tracks active lobby widget"),
		RaidSessionSubsystemSource.Contains(TEXT("ActiveLobbyWidget")));
	TestTrue(TEXT("raid session success state shows loading before travel"),
		RaidSessionSubsystemSource.Contains(TEXT("ShowLoading()")));
	TestTrue(TEXT("raid session waiting state shows lobby wait panel"),
		RaidSessionSubsystemSource.Contains(TEXT("ShowWaitingPopup()")));
	TestTrue(TEXT("raid session failed state shows lobby no-server panel"),
		RaidSessionSubsystemSource.Contains(TEXT("ShowNoServerPopup()")));
	TestTrue(TEXT("raid session cancel state returns to main lobby panel"),
		RaidSessionSubsystemSource.Contains(TEXT("ShowMainLobby()")));
	TestTrue(TEXT("raid lobby debug waiting hook exists"),
		RaidLobbyWidgetSource.Contains(TEXT("ShowDebugWaitingPopup")));
	TestTrue(TEXT("raid lobby debug no-server hook exists"),
		RaidLobbyWidgetSource.Contains(TEXT("ShowDebugNoServerPopup")));
	TestTrue(TEXT("raid lobby debug main reset hook exists"),
		RaidLobbyWidgetSource.Contains(TEXT("ResetDebugMainLobby")));
	TestTrue(TEXT("raid lobby debug hook summary log marker exists"),
		RaidLobbyWidgetSource.Contains(TEXT("[DR_SUMMARY] LobbyUIDebugHook")));
	TestTrue(TEXT("raid lobby duplicate request guard summary log marker exists"),
		RaidLobbyWidgetSource.Contains(TEXT("[DR_SUMMARY] RaidLobbyRequestEntryIgnored")));
	TestTrue(TEXT("raid lobby duplicate request guard state exists"),
		RaidLobbyWidgetSource.Contains(TEXT("bRaidEntryRequestInFlight")));
	TestTrue(TEXT("boss max HP is replicated with current HP"),
		RaidBossSource.Contains(TEXT("DOREPLIFETIME(ARaidBoss, MaxHP)")));
	TestTrue(TEXT("selection timer start summary log marker exists"),
		RaidPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] SelectTimerStart PC=")));
	TestTrue(TEXT("selection timer stop summary log marker exists"),
		RaidPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] SelectTimerStop PC=")));
	TestTrue(TEXT("auto ready summary log marker exists"),
		RaidPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] AutoReady PC=")));
	TestTrue(TEXT("auto ready dead pawn ignored reason marker exists"),
		RaidPlayerControllerSource.Contains(TEXT("Result=Ignored Reason=DeadPawn")));
	TestTrue(TEXT("D6 kill drone target summary marker exists"),
		RaidPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] D6KillDrone RequestPC=")));
	TestTrue(TEXT("ui refresh summary log marker exists"),
		RaidPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] UIRefresh PC=")));
	TestTrue(TEXT("death return summary log marker exists"),
		DronePartReturnManagerSource.Contains(TEXT("DeathReturn")));
	TestTrue(TEXT("drone damage summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] DroneDamage PC=")));
	TestTrue(TEXT("drone death summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] DroneDeath PC=")));
	TestTrue(TEXT("raid end summary log marker exists"),
		RaidGameModeSource.Contains(TEXT("[DR_SUMMARY] RaidEnd Reason=")));
	TestTrue(TEXT("raid end return summary log marker exists"),
		DronePartReturnManagerSource.Contains(TEXT("RaidEndReturn")));
	TestTrue(TEXT("return skipped summary log marker exists"),
		DronePartReturnManagerSource.Contains(TEXT("[DR_SUMMARY] ReturnSkipped PC=")));
	TestTrue(TEXT("dead input ignored summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] DeadInputIgnored PC=")));
	TestTrue(TEXT("dead attack ignored marker exists"),
		DroneSource.Contains(TEXT("TEXT(\"Attack\")")));
	TestTrue(TEXT("dead move ignored marker exists"),
		DroneSource.Contains(TEXT("TEXT(\"Move\")")));
	TestTrue(TEXT("dead dodge ignored marker exists"),
		DroneSource.Contains(TEXT("TEXT(\"Dodge\")")));
	TestTrue(TEXT("callable dodge request helper exists"),
		DroneSource.Contains(TEXT("RequestDodge(")));
	TestTrue(TEXT("client dodge requests use one server RPC"),
		DroneSource.Contains(TEXT("Server_RequestDodge")));
	TestTrue(TEXT("dodge input action slot exists for editor assignment"),
		DroneSource.Contains(TEXT("DodgeAction")));
	TestTrue(TEXT("dodge input binding is null guarded"),
		DroneSource.Contains(TEXT("if (DodgeAction)")));
	TestTrue(TEXT("dodge input action uses a single press-style binding"),
		DroneSource.Contains(TEXT("BindAction(DodgeAction, ETriggerEvent::Started")));
	TestTrue(TEXT("dodge zero direction ignore reason marker exists"),
		DroneSource.Contains(TEXT("NoDirection")));
	TestTrue(TEXT("dodge summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] Dodge PC=")));
	TestTrue(TEXT("dodge accepted summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] Dodge Accepted:")));
	TestTrue(TEXT("dodge ignored summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] Dodge Ignored")));
	TestTrue(TEXT("dodge invincible begin summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] DodgeInvincible: State=Begin")));
	TestTrue(TEXT("dodge invincible end summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] DodgeInvincible: State=End")));
	TestTrue(TEXT("server dodge applied summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] ServerDodgeApplied PC=")));
	TestTrue(TEXT("dodge move-distance policy marker exists"),
		DroneSource.Contains(TEXT("MoveDistancePolicy=Included")));
	TestTrue(TEXT("dead heal ignored marker exists"),
		DroneSource.Contains(TEXT("TEXT(\"Heal\")")));
	TestTrue(TEXT("weapon calculation summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] WeaponCalc Player=")));
	TestTrue(TEXT("weapon calculation summary log keeps additional hit count marker"),
		DroneSource.Contains(TEXT("AdditionalHitCount=")));
	TestTrue(TEXT("core calculation summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] CoreCalc Player=")));
	TestTrue(TEXT("Drain heal summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] DrainHeal PC=")));
	TestTrue(TEXT("combat record summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] CombatRecord Player=")));
	TestTrue(TEXT("report created summary log marker exists"),
		RaidPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] ReportCreated Player=")));
	TestTrue(TEXT("bonus calculation summary log marker exists"),
		RaidPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] BonusCalc Player=")));
	TestTrue(TEXT("grade calculation summary log marker exists"),
		RaidPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] GradeCalc Player=")));
	TestTrue(TEXT("report duplicate summary log marker exists"),
		RaidPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] ReportDuplicateIgnored Player=")));
	TestTrue(TEXT("report client received summary log marker exists"),
		RaidPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] ReportClientReceived Player=")));
	TestTrue(TEXT("report widget shown summary log marker exists"),
		RaidPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] ReportWidgetShown Player=")));
	TestTrue(TEXT("report widget refreshed summary log marker exists"),
		RaidPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] ReportWidgetRefreshed Player=")));
	TestTrue(TEXT("report widget missing class summary log marker exists"),
		RaidPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] ReportWidgetMissingClass Player=")));
	TestTrue(TEXT("report widget hidden summary log marker exists"),
		RaidPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] ReportWidgetHidden Player=")));
	TestTrue(TEXT("report return-to-lobby button optional binding exists"),
		DroneReportWidgetSource.Contains(TEXT("ReturnToLobbyButton")));
	TestTrue(TEXT("report return-to-lobby click summary log marker exists"),
		DroneReportWidgetSource.Contains(TEXT("[DR_SUMMARY] ReportReturnToLobbyClicked")));
	TestTrue(TEXT("report return-to-lobby travel summary log marker exists"),
		DroneReportWidgetSource.Contains(TEXT("[DR_SUMMARY] ReportReturnToLobbyTravel Target=LobbyMap")));
	TestTrue(TEXT("report return-to-lobby duplicate guard marker exists"),
		DroneReportWidgetSource.Contains(TEXT("[DR_SUMMARY] ReportReturnToLobbyIgnored Reason=AlreadyRequested")));
	TestTrue(TEXT("report return-to-lobby missing button marker exists"),
		DroneReportWidgetSource.Contains(TEXT("[DR_SUMMARY] ReportReturnToLobbyButtonMissing")));
	TestTrue(TEXT("report return-to-lobby target stays LobbyMap"),
		DroneReportWidgetSource.Contains(TEXT("LobbyMap")));
	TestFalse(TEXT("report widget does not expose ReportScore getter"),
		DroneReportWidgetSource.Contains(TEXT("GetReportScoreText")));
	TestTrue(TEXT("return after report summary log marker exists"),
		RaidGameModeSource.Contains(TEXT("[DR_SUMMARY] ReturnAfterReport Player="))
		&& DroneSource.Contains(TEXT("[DR_SUMMARY] ReturnAfterReport Player=")));
	TestTrue(TEXT("move input summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] MoveInput PC=")));
	TestTrue(TEXT("movement boundary clamp summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] MoveClamp: Reason=Boundary")));
	TestTrue(TEXT("movement boss-min clamp summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] MoveClamp: Reason=BossMinDistance")));
	TestTrue(TEXT("move accepted summary uses shared throttle helper"),
		DroneSource.Contains(TEXT("ShouldEmitMoveAcceptedSummaryLog(")));
	TestTrue(TEXT("server move applied summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] ServerMoveApplied PC=")));
	TestTrue(TEXT("server move ignored summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] ServerMoveIgnored PC=")));
	TestTrue(TEXT("owner-only move sync is registered"),
		DroneSource.Contains(TEXT("DOREPLIFETIME_CONDITION(ADrone, OwnerMoveSync, COND_OwnerOnly)")));
	TestTrue(TEXT("owner move sync sent summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] OwnerMoveSyncSent PC=")));
	TestTrue(TEXT("owner move corrected summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] OwnerMoveCorrected PC=")));
	TestTrue(TEXT("owner move corrected target actor marker exists"),
		DroneSource.Contains(TEXT("CorrectedActor=")));
	TestTrue(TEXT("owner move corrected pawn ownership comparison marker exists"),
		DroneSource.Contains(TEXT("bPawnMatchesCorrectedActor=")));
	TestTrue(TEXT("server move pawn ownership comparison marker exists"),
		DroneSource.Contains(TEXT("bPawnMatchesDrone=")));
	TestTrue(TEXT("replicated location summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] ReplicatedLocation PC=")));
	TestTrue(TEXT("boss-facing camera configured summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] Camera: Result=Configured Mode=BossFacingQuarterView")));
	TestTrue(TEXT("view target result summary marker exists"),
		DroneSource.Contains(TEXT("ViewTargetResult=")));
	TestTrue(TEXT("view target matched pawn result marker exists"),
		DroneSource.Contains(TEXT("ViewTargetMatchesPawn")));
	TestTrue(TEXT("simulated proxy view target result marker exists"),
		DroneSource.Contains(TEXT("SimProxyObserved")));
	TestTrue(TEXT("move audit summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] MoveAudit PC=")));
	TestTrue(TEXT("move distance summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] MoveDistance PC=")));
	TestTrue(TEXT("move distance ignored summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] MoveDistanceIgnored PC=")));
	TestTrue(TEXT("move distance reset summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] MoveDistanceReset PC=")));
	TestTrue(TEXT("drone movement replication is explicit"),
		DroneSource.Contains(TEXT("SetReplicateMovement(true)")));
	TestTrue(TEXT("timer text refresh interval marker exists"),
		DronePartSelectWidgetSource.Contains(TEXT("TimerTextRefreshIntervalSeconds")));
	TestTrue(TEXT("timer text refresh starts from the widget"),
		DronePartSelectWidgetSource.Contains(TEXT("StartTimerTextRefresh")));
	TestTrue(TEXT("timer text refresh stops from the widget"),
		DronePartSelectWidgetSource.Contains(TEXT("StopTimerTextRefresh")));
	TestTrue(TEXT("timer text refresh reads the computed remaining time"),
		DronePartSelectWidgetSource.Contains(TEXT("GetSelectionRemainingTime()")));
	TestTrue(TEXT("remaining timer clamps to the selection duration"),
		RaidPlayerControllerSource.Contains(TEXT("SelectionDurationSeconds")));
	TestTrue(TEXT("remaining timer uses an upper clamp"),
		RaidPlayerControllerSource.Contains(TEXT("FMath::Clamp(")));
	TestTrue(TEXT("summary logs use stable controller naming"),
		RaidPlayerControllerSource.Contains(TEXT("BuildStableControllerLogString")));
	TestTrue(TEXT("drone logs use stable controller naming"),
		DroneSource.Contains(TEXT("BuildStableControllerLogString")));
	TestTrue(TEXT("return logs use stable controller naming"),
		DronePartReturnManagerSource.Contains(TEXT("BuildStableControllerLogString")));
	TestTrue(TEXT("raid end duplicate skip summary log marker exists"),
		RaidGameModeSource.Contains(TEXT("[DR_SUMMARY] RaidEndSkipped Reason=AlreadyEnded")));
	TestTrue(TEXT("raid end server state cleanup summary log marker exists"),
		RaidPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] RaidEndStateCleaned Player=")));
	TestTrue(TEXT("raid end client refresh summary log marker exists"),
		RaidPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] RaidEndClientRefresh Player=")));
	TestTrue(TEXT("in battle UI summary reports equipped parts"),
		RaidPlayerControllerSource.Contains(TEXT("SummaryCorePartID")));

	return true;
}

#endif
