#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Tutorial/TutorialGameMode.h"
#include "Tutorial/TutorialDebris.h"
#include "Tutorial/TutorialPlayerController.h"
#include "Tutorial/TutorialTypes.h"
#include "Drone.h"
#include "Raid/RaidBoss.h"
#include "Raid/RaidGameState.h"
#include "Raid/RaidPlayerController.h"

#include "Engine/World.h"
#include "EngineUtils.h"

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
	TestEqual(TEXT("StartTutorial enters Start"), TutorialPC->GetCurrentTutorialStep(), ETutorialStep::Start);

	TestTrue(TEXT("start presentation advances to world briefing"), TutorialPC->AdvanceTutorialStep());
	TestEqual(TEXT("world briefing follows start"), TutorialPC->GetCurrentTutorialStep(), ETutorialStep::WorldBriefing);
	TestFalse(TEXT("attack input does not skip world briefing"), TutorialPC->NotifyTutorialAttackInput());
	TestEqual(TEXT("wrong input keeps world briefing"), TutorialPC->GetCurrentTutorialStep(), ETutorialStep::WorldBriefing);

	TestTrue(TEXT("world briefing advances to move"), TutorialPC->AdvanceTutorialStep());
	TestEqual(TEXT("move follows world briefing"), TutorialPC->GetCurrentTutorialStep(), ETutorialStep::Move);
	TestFalse(TEXT("dialogue advance cannot skip move"), TutorialPC->AdvanceTutorialStep());
	TestTrue(TEXT("actual movement advances to attack"), TutorialPC->NotifyTutorialMoveInput(FVector2D(1.0f, 0.0f)));
	TestEqual(TEXT("movement completion enters attack"), TutorialPC->GetCurrentTutorialStep(), ETutorialStep::Attack);

	TestTrue(TEXT("actual attack advances to dodge"), TutorialPC->NotifyTutorialAttackInput());
	TestEqual(TEXT("attack completion enters dodge"), TutorialPC->GetCurrentTutorialStep(), ETutorialStep::Dodge);

	TestTrue(TEXT("actual dodge advances to combat briefing"), TutorialPC->NotifyTutorialDodgeInput(FVector2D(0.0f, 1.0f)));
	TestEqual(TEXT("dodge completion enters combat briefing"), TutorialPC->GetCurrentTutorialStep(), ETutorialStep::CombatBriefing);
	TestTrue(TEXT("combat briefing advances to debris combat"), TutorialPC->AdvanceTutorialStep());
	TestEqual(TEXT("debris combat follows combat briefing"), TutorialPC->GetCurrentTutorialStep(), ETutorialStep::DebrisCombat);
	TestFalse(TEXT("dialogue advance cannot skip debris combat"), TutorialPC->AdvanceTutorialStep());
	TestTrue(TEXT("debris destruction advances to closing briefing"), TutorialPC->NotifyTutorialDebrisDestroyed());
	TestEqual(TEXT("closing briefing follows debris combat"), TutorialPC->GetCurrentTutorialStep(), ETutorialStep::ClosingBriefing);
	TestFalse(TEXT("tutorial is incomplete before closing briefing ends"), TutorialPC->IsTutorialComplete());

	TestTrue(TEXT("closing briefing completes tutorial"), TutorialPC->AdvanceTutorialStep());
	TestEqual(TEXT("closing briefing completion enters complete"), TutorialPC->GetCurrentTutorialStep(), ETutorialStep::Complete);
	TestTrue(TEXT("tutorial is complete after closing briefing"), TutorialPC->IsTutorialComplete());
	TestFalse(TEXT("advance after complete is ignored"), TutorialPC->AdvanceTutorialStep());

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
	TestEqual(TEXT("game mode start enters Start"), TutorialPC->GetCurrentTutorialStep(), ETutorialStep::Start);

	TestEqual(TEXT("tutorial does not change RaidState"), RaidGameState->RaidState, InitialRaidState);
	TestEqual(TEXT("tutorial does not change PlayerSelectionState"), RaidPC->GetPlayerSelectionState(), InitialSelectionState);

	TutorialPC->AdvanceTutorialStep();
	TutorialPC->AdvanceTutorialStep();
	TutorialPC->NotifyTutorialMoveInput(FVector2D(-1.0f, 0.0f));
	TutorialPC->NotifyTutorialAttackInput();
	TutorialPC->NotifyTutorialDodgeInput(FVector2D(0.0f, 1.0f));
	TutorialPC->AdvanceTutorialStep();
	TutorialPC->NotifyTutorialDebrisDestroyed();
	TutorialPC->AdvanceTutorialStep();
	TestTrue(TEXT("tutorial reaches complete"), TutorialPC->IsTutorialComplete());

	TutorialPC->SetSuppressTutorialLobbyTravelForTest(true);
	TestTrue(TEXT("return to lobby hook accepts completed tutorial"), TutorialPC->ReturnToLobbyAfterTutorial());
	TestEqual(TEXT("suppressed return records one travel request"), TutorialPC->GetTutorialLobbyTravelRequestCountForTest(), 1);
	TestFalse(TEXT("second return request is ignored"), TutorialPC->ReturnToLobbyAfterTutorial());
	TestEqual(TEXT("duplicate return does not add travel request"), TutorialPC->GetTutorialLobbyTravelRequestCountForTest(), 1);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDronePOR23TutorialDebrisTest,
	"DroneProto.POR23.Tutorial.DebrisThreeHit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDronePOR23TutorialDebrisTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("TutorialDebrisWorld")));
	TestNotNull(TEXT("tutorial debris world is created"), World);
	if (!World)
	{
		return false;
	}

	ATutorialPlayerController* TutorialPC = World->SpawnActor<ATutorialPlayerController>();
	ATutorialDebris* Debris = World->SpawnActor<ATutorialDebris>();
	TestNotNull(TEXT("tutorial controller is spawned"), TutorialPC);
	TestNotNull(TEXT("tutorial debris is spawned"), Debris);
	if (!TutorialPC || !Debris)
	{
		World->DestroyWorld(false);
		return false;
	}

	TutorialPC->StartTutorial();
	TutorialPC->AdvanceTutorialStep();
	TutorialPC->AdvanceTutorialStep();
	TutorialPC->NotifyTutorialMoveInput(FVector2D(1.0f, 0.0f));
	TutorialPC->NotifyTutorialAttackInput();
	TutorialPC->NotifyTutorialDodgeInput(FVector2D(0.0f, 1.0f));
	TutorialPC->AdvanceTutorialStep();
	TestEqual(TEXT("debris test reaches combat"), TutorialPC->GetCurrentTutorialStep(), ETutorialStep::DebrisCombat);

	TestTrue(TEXT("first actual hit is accepted"), Debris->ApplyTutorialHitForServer(TutorialPC));
	TestEqual(TEXT("first hit count"), Debris->GetTutorialHitCount(), 1);
	TestFalse(TEXT("first hit does not destroy debris"), Debris->IsTutorialDebrisDestroyed());
	TestTrue(TEXT("second actual hit is accepted"), Debris->ApplyTutorialHitForServer(TutorialPC));
	TestEqual(TEXT("second hit count"), Debris->GetTutorialHitCount(), 2);
	TestFalse(TEXT("second hit does not destroy debris"), Debris->IsTutorialDebrisDestroyed());
	TestTrue(TEXT("third actual hit is accepted"), Debris->ApplyTutorialHitForServer(TutorialPC));
	TestEqual(TEXT("third hit count"), Debris->GetTutorialHitCount(), 3);
	TestTrue(TEXT("third hit destroys debris"), Debris->IsTutorialDebrisDestroyed());
	TestEqual(TEXT("debris destruction enters closing briefing"), TutorialPC->GetCurrentTutorialStep(), ETutorialStep::ClosingBriefing);
	TestFalse(TEXT("destroyed debris rejects extra hits"), Debris->ApplyTutorialHitForServer(TutorialPC));

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDronePOR23TutorialGameModeSpawnTest,
	"DroneProto.POR23.Tutorial.GameModeSpawnsDebrisWithoutBoss",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDronePOR23TutorialGameModeSpawnTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("TutorialGameModeSpawnWorld")));
	TestNotNull(TEXT("tutorial game mode world is created"), World);
	if (!World)
	{
		return false;
	}

	ATutorialGameMode* GameMode = World->SpawnActor<ATutorialGameMode>();
	ATutorialPlayerController* TutorialPC = World->SpawnActor<ATutorialPlayerController>();
	ADrone* Drone = World->SpawnActor<ADrone>();
	TestNotNull(TEXT("tutorial game mode is spawned"), GameMode);
	TestNotNull(TEXT("tutorial player controller is spawned"), TutorialPC);
	TestNotNull(TEXT("tutorial drone is spawned"), Drone);
	if (!GameMode || !TutorialPC || !Drone)
	{
		World->DestroyWorld(false);
		return false;
	}

	World->AddController(TutorialPC);
	TutorialPC->Possess(Drone);
	TestTrue(TEXT("tutorial game mode uses the real drone class"), GameMode->DefaultPawnClass == ADrone::StaticClass());
	TestNull(TEXT("tutorial starts without debris"), GameMode->GetTutorialDebris());

	GameMode->StartTutorialForController(TutorialPC);
	GameMode->AdvanceTutorialForController(TutorialPC);
	GameMode->AdvanceTutorialForController(TutorialPC);
	TutorialPC->NotifyTutorialMoveInput(FVector2D(1.0f, 0.0f));
	TutorialPC->NotifyTutorialAttackInput();
	TutorialPC->NotifyTutorialDodgeInput(FVector2D(0.0f, 1.0f));
	GameMode->AdvanceTutorialForController(TutorialPC);

	ATutorialDebris* SpawnedDebris = GameMode->GetTutorialDebris();
	TestNotNull(TEXT("game mode spawns one debris for combat"), SpawnedDebris);
	TestTrue(TEXT("spawned debris targets the tutorial drone"), SpawnedDebris && SpawnedDebris->GetTargetPawn() == Drone);

	int32 BossCount = 0;
	for (TActorIterator<ARaidBoss> It(World); It; ++It)
	{
		++BossCount;
	}
	TestEqual(TEXT("tutorial game mode does not spawn a raid boss"), BossCount, 0);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDronePOR23TutorialRealDroneFlowTest,
	"DroneProto.POR23.Tutorial.RealDroneInputFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDronePOR23TutorialRealDroneFlowTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("TutorialRealDroneFlowWorld")));
	TestNotNull(TEXT("real drone tutorial world is created"), World);
	if (!World)
	{
		return false;
	}

	ATutorialGameMode* GameMode = World->SpawnActor<ATutorialGameMode>();
	ATutorialPlayerController* TutorialPC = World->SpawnActor<ATutorialPlayerController>();
	ADrone* Drone = World->SpawnActor<ADrone>();
	TestNotNull(TEXT("real flow game mode is spawned"), GameMode);
	TestNotNull(TEXT("real flow controller is spawned"), TutorialPC);
	TestNotNull(TEXT("real flow drone is spawned"), Drone);
	if (!GameMode || !TutorialPC || !Drone)
	{
		World->DestroyWorld(false);
		return false;
	}

	World->AddController(TutorialPC);
	TutorialPC->Possess(Drone);
	GameMode->StartTutorialForController(TutorialPC);
	TestFalse(TEXT("movement is restricted during start"), Drone->ApplyMoveInputForServerForTest(FVector2D(1.0f, 0.0f)));

	GameMode->AdvanceTutorialForController(TutorialPC);
	GameMode->AdvanceTutorialForController(TutorialPC);
	TestTrue(TEXT("real drone move input is accepted during move step"), Drone->ApplyMoveInputForServerForTest(FVector2D(1.0f, 0.0f)));
	TestTrue(TEXT("real drone movement is applied"), Drone->ApplyPendingServerMoveInputForTest(0.1f));
	TestEqual(TEXT("real movement advances tutorial"), TutorialPC->GetCurrentTutorialStep(), ETutorialStep::Attack);

	Drone->RequestAttackBoss();
	TestEqual(TEXT("real attack advances tutorial without a target"), TutorialPC->GetCurrentTutorialStep(), ETutorialStep::Dodge);
	TestTrue(TEXT("real dodge is accepted during dodge step"), Drone->RequestDodgeForServer(FVector2D(0.0f, 1.0f)));
	TestEqual(TEXT("real dodge advances tutorial"), TutorialPC->GetCurrentTutorialStep(), ETutorialStep::CombatBriefing);
	Drone->TickForTest(1.0f);

	GameMode->AdvanceTutorialForController(TutorialPC);
	ATutorialDebris* Debris = GameMode->GetTutorialDebris();
	TestNotNull(TEXT("real flow spawns debris"), Debris);
	if (Debris)
	{
		Drone->RequestAttackBoss();
		Drone->RequestAttackBoss();
		Drone->RequestAttackBoss();
		TestTrue(TEXT("three real attacks destroy tutorial debris"), Debris->IsTutorialDebrisDestroyed());
	}
	TestEqual(TEXT("real debris combat enters closing briefing"), TutorialPC->GetCurrentTutorialStep(), ETutorialStep::ClosingBriefing);

	const int32 HealthBeforeDamage = Drone->GetHealth();
	Drone->ApplyDamageForServer(50, FName(TEXT("TutorialAutomation")));
	TestEqual(TEXT("tutorial drone is invulnerable"), Drone->GetHealth(), HealthBeforeDamage);

	World->DestroyWorld(false);
	return true;
}

#endif
