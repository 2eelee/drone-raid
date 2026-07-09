#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Tutorial/TutorialGameMode.h"
#include "Tutorial/TutorialPlayerController.h"
#include "Tutorial/TutorialTypes.h"
#include "Raid/RaidGameState.h"
#include "Raid/RaidPlayerController.h"

#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneQ10TutorialStepSequenceTest,
	"DroneProto.Q10.Tutorial.StepSequence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneQ10TutorialStepSequenceTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("TutorialStepSequenceWorld")));
	TestNotNull(TEXT("tutorial world is created"), World);
	if (!World)
	{
		return false;
	}

	ATutorialPlayerController* TutorialPC = World->SpawnActor<ATutorialPlayerController>();
	TestNotNull(TEXT("tutorial player controller is spawned"), TutorialPC);
	if (!TutorialPC)
	{
		World->DestroyWorld(false);
		return false;
	}

	TestEqual(TEXT("tutorial starts at None"), TutorialPC->GetCurrentTutorialStep(), ETutorialStep::None);
	TestFalse(TEXT("tutorial is not complete before start"), TutorialPC->IsTutorialComplete());

	TutorialPC->StartTutorial();
	TestEqual(TEXT("StartTutorial enters MoveLeft"), TutorialPC->GetCurrentTutorialStep(), ETutorialStep::MoveLeft);

	TestFalse(TEXT("attack input does not skip MoveLeft"), TutorialPC->NotifyTutorialAttackInput());
	TestEqual(TEXT("wrong input keeps MoveLeft"), TutorialPC->GetCurrentTutorialStep(), ETutorialStep::MoveLeft);

	TestTrue(TEXT("left movement advances to Attack"), TutorialPC->NotifyTutorialMoveInput(FVector2D(-1.0f, 0.0f)));
	TestEqual(TEXT("MoveLeft completion enters Attack"), TutorialPC->GetCurrentTutorialStep(), ETutorialStep::Attack);

	TestTrue(TEXT("attack input advances to Dodge"), TutorialPC->NotifyTutorialAttackInput());
	TestEqual(TEXT("Attack completion enters Dodge"), TutorialPC->GetCurrentTutorialStep(), ETutorialStep::Dodge);

	TestTrue(TEXT("dodge input advances to Complete"), TutorialPC->NotifyTutorialDodgeInput(FVector2D(0.0f, 1.0f)));
	TestEqual(TEXT("Dodge completion enters Complete"), TutorialPC->GetCurrentTutorialStep(), ETutorialStep::Complete);
	TestTrue(TEXT("tutorial is complete after Dodge"), TutorialPC->IsTutorialComplete());

	TutorialPC->AdvanceTutorialStep();
	TestEqual(TEXT("Advance after Complete keeps Complete"), TutorialPC->GetCurrentTutorialStep(), ETutorialStep::Complete);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneQ10TutorialIsolationAndReturnTest,
	"DroneProto.Q10.Tutorial.IsolationAndReturn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneQ10TutorialIsolationAndReturnTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("TutorialIsolationWorld")));
	TestNotNull(TEXT("tutorial isolation world is created"), World);
	if (!World)
	{
		return false;
	}

	ARaidGameState* RaidGameState = World->SpawnActor<ARaidGameState>();
	ARaidPlayerController* RaidPC = World->SpawnActor<ARaidPlayerController>();
	ATutorialGameMode* TutorialGameMode = World->SpawnActor<ATutorialGameMode>();
	ATutorialPlayerController* TutorialPC = World->SpawnActor<ATutorialPlayerController>();
	TestNotNull(TEXT("raid game state is spawned"), RaidGameState);
	TestNotNull(TEXT("raid player controller is spawned"), RaidPC);
	TestNotNull(TEXT("tutorial game mode is spawned"), TutorialGameMode);
	TestNotNull(TEXT("tutorial player controller is spawned"), TutorialPC);
	if (!RaidGameState || !RaidPC || !TutorialGameMode || !TutorialPC)
	{
		World->DestroyWorld(false);
		return false;
	}

	World->SetGameState(RaidGameState);
	const ERaidState InitialRaidState = RaidGameState->RaidState;
	const EPlayerSelectionState InitialSelectionState = RaidPC->GetPlayerSelectionState();

	TestTrue(TEXT("game mode starts tutorial for controller"), TutorialGameMode->StartTutorialForController(TutorialPC));
	TestEqual(TEXT("game mode start enters MoveLeft"), TutorialPC->GetCurrentTutorialStep(), ETutorialStep::MoveLeft);
	TutorialPC->NotifyTutorialMoveInput(FVector2D(-1.0f, 0.0f));
	TutorialPC->NotifyTutorialAttackInput();
	TutorialPC->NotifyTutorialDodgeInput(FVector2D(0.0f, 1.0f));
	TestTrue(TEXT("tutorial reaches complete"), TutorialPC->IsTutorialComplete());

	TestEqual(TEXT("tutorial does not change RaidState"), RaidGameState->RaidState, InitialRaidState);
	TestEqual(TEXT("tutorial does not change PlayerSelectionState"), RaidPC->GetPlayerSelectionState(), InitialSelectionState);

	TutorialPC->SetSuppressTutorialLobbyTravelForTest(true);
	TestTrue(TEXT("return to lobby hook accepts completed tutorial"), TutorialPC->ReturnToLobbyAfterTutorial());
	TestEqual(TEXT("suppressed return records one travel request"), TutorialPC->GetTutorialLobbyTravelRequestCountForTest(), 1);
	TestFalse(TEXT("second return request is ignored"), TutorialPC->ReturnToLobbyAfterTutorial());
	TestEqual(TEXT("duplicate return does not add travel request"), TutorialPC->GetTutorialLobbyTravelRequestCountForTest(), 1);

	World->DestroyWorld(false);
	return true;
}

#endif
