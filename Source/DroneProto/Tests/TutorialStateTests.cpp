#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Tutorial/TutorialGameMode.h"
#include "Tutorial/TutorialDebris.h"
#include "Tutorial/TutorialPlayerController.h"
#include "Tutorial/TutorialTypes.h"
#include "Drone.h"
#include "Lobby/RaidSessionSubsystem.h"
#include "Raid/DroneReportWidget.h"
#include "Raid/RaidBoss.h"
#include "Raid/RaidGameState.h"
#include "Raid/RaidPlayerController.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

namespace
{
bool RevealFinalTutorialDialogue(ATutorialPlayerController* TutorialPC)
{
	if (!TutorialPC || TutorialPC->GetCurrentTutorialDialogueText().IsEmpty())
	{
		return false;
	}

	for (int32 SafetyCounter = 0;
		!TutorialPC->IsCurrentTutorialDialogueReady() && SafetyCounter < 16;
		++SafetyCounter)
	{
		if (!TutorialPC->TryAdvanceTutorialDialogue())
		{
			return false;
		}
	}
	return TutorialPC->IsCurrentTutorialDialogueReady();
}

bool AdvanceDialogueStepToNext(ATutorialPlayerController* TutorialPC)
{
	if (!TutorialPC)
	{
		return false;
	}

	const ETutorialStep InitialStep = TutorialPC->GetCurrentTutorialStep();
	for (int32 SafetyCounter = 0;
		TutorialPC->GetCurrentTutorialStep() == InitialStep && SafetyCounter < 16;
		++SafetyCounter)
	{
		if (!TutorialPC->TryAdvanceTutorialDialogue())
		{
			return false;
		}
	}
	return TutorialPC->GetCurrentTutorialStep() != InitialStep;
}
}

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
	TestTrue(TEXT("start step has no dialogue"), TutorialPC->GetCurrentTutorialDialogueText().IsEmpty());

	TestTrue(TEXT("start presentation advances to world briefing"), TutorialPC->AdvanceTutorialStep());
	TestEqual(TEXT("world briefing follows start"), TutorialPC->GetCurrentTutorialStep(), ETutorialStep::WorldBriefing);
	TestEqual(TEXT("world briefing starts at first line"), TutorialPC->GetCurrentTutorialDialogueIndex(), 0);
	TestEqual(TEXT("world briefing first text"),
		TutorialPC->GetCurrentTutorialDialogueText().ToString(),
		FString(TEXT("어서오세요. 새로운 클리너님.")));
	TestFalse(TEXT("world briefing cannot be skipped before final dialogue"), TutorialPC->AdvanceTutorialStep());
	TestFalse(TEXT("attack input does not skip world briefing"), TutorialPC->NotifyTutorialAttackInput());
	TestEqual(TEXT("wrong input keeps world briefing"), TutorialPC->GetCurrentTutorialStep(), ETutorialStep::WorldBriefing);

	for (int32 DialogueIndex = 1; DialogueIndex < 5; ++DialogueIndex)
	{
		TestTrue(TEXT("Z advances world briefing"), TutorialPC->TryAdvanceTutorialDialogue());
		TestEqual(TEXT("world briefing dialogue index advances"), TutorialPC->GetCurrentTutorialDialogueIndex(), DialogueIndex);
	}
	TestTrue(TEXT("last world briefing Z advances to move"), TutorialPC->TryAdvanceTutorialDialogue());
	TestEqual(TEXT("move follows world briefing"), TutorialPC->GetCurrentTutorialStep(), ETutorialStep::Move);
	TestFalse(TEXT("dialogue advance cannot skip move"), TutorialPC->AdvanceTutorialStep());
	TestFalse(TEXT("movement is blocked before final instruction"), TutorialPC->NotifyTutorialMoveInput(FVector2D(1.0f, 0.0f)));
	TestTrue(TEXT("Z reveals final move instruction"), TutorialPC->TryAdvanceTutorialDialogue());
	TestTrue(TEXT("actual movement advances to attack"), TutorialPC->NotifyTutorialMoveInput(FVector2D(1.0f, 0.0f)));
	TestEqual(TEXT("movement completion enters attack"), TutorialPC->GetCurrentTutorialStep(), ETutorialStep::Attack);

	TestFalse(TEXT("attack is blocked before final instruction"), TutorialPC->NotifyTutorialAttackInput());
	TestTrue(TEXT("Z advances first attack instruction"), TutorialPC->TryAdvanceTutorialDialogue());
	TestTrue(TEXT("Z advances second attack instruction"), TutorialPC->TryAdvanceTutorialDialogue());
	TestTrue(TEXT("actual attack advances to dodge"), TutorialPC->NotifyTutorialAttackInput());
	TestEqual(TEXT("attack completion enters dodge"), TutorialPC->GetCurrentTutorialStep(), ETutorialStep::Dodge);

	TestFalse(TEXT("dodge is blocked before final instruction"), TutorialPC->NotifyTutorialDodgeInput(FVector2D(0.0f, 1.0f)));
	TestTrue(TEXT("Z advances first dodge instruction"), TutorialPC->TryAdvanceTutorialDialogue());
	TestTrue(TEXT("Z advances second dodge instruction"), TutorialPC->TryAdvanceTutorialDialogue());
	TestTrue(TEXT("actual dodge advances to combat briefing"), TutorialPC->NotifyTutorialDodgeInput(FVector2D(0.0f, 1.0f)));
	TestEqual(TEXT("dodge completion enters combat briefing"), TutorialPC->GetCurrentTutorialStep(), ETutorialStep::CombatBriefing);
	TestFalse(TEXT("combat briefing cannot be skipped before final dialogue"), TutorialPC->AdvanceTutorialStep());
	TestTrue(TEXT("Z advances combat briefing dialogue"), TutorialPC->TryAdvanceTutorialDialogue());
	TestTrue(TEXT("last combat briefing Z enters debris combat"), TutorialPC->TryAdvanceTutorialDialogue());
	TestEqual(TEXT("debris combat follows combat briefing"), TutorialPC->GetCurrentTutorialStep(), ETutorialStep::DebrisCombat);
	TestFalse(TEXT("dialogue advance cannot skip debris combat"), TutorialPC->AdvanceTutorialStep());
	TestTrue(TEXT("debris destruction advances to closing briefing"), TutorialPC->NotifyTutorialDebrisDestroyed());
	TestEqual(TEXT("closing briefing follows debris combat"), TutorialPC->GetCurrentTutorialStep(), ETutorialStep::ClosingBriefing);
	TestFalse(TEXT("tutorial is incomplete before closing briefing ends"), TutorialPC->IsTutorialComplete());
	TestFalse(TEXT("closing briefing cannot complete before final dialogue"), TutorialPC->AdvanceTutorialStep());

	TestTrue(TEXT("Z advances closing briefing dialogue"), TutorialPC->TryAdvanceTutorialDialogue());
	TestTrue(TEXT("last closing briefing Z completes tutorial"), TutorialPC->TryAdvanceTutorialDialogue());
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
	AdvanceDialogueStepToNext(TutorialPC);
	RevealFinalTutorialDialogue(TutorialPC);
	TutorialPC->NotifyTutorialMoveInput(FVector2D(-1.0f, 0.0f));
	RevealFinalTutorialDialogue(TutorialPC);
	TutorialPC->NotifyTutorialAttackInput();
	RevealFinalTutorialDialogue(TutorialPC);
	TutorialPC->NotifyTutorialDodgeInput(FVector2D(0.0f, 1.0f));
	AdvanceDialogueStepToNext(TutorialPC);
	TutorialPC->NotifyTutorialDebrisDestroyed();
	AdvanceDialogueStepToNext(TutorialPC);
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
	AdvanceDialogueStepToNext(TutorialPC);
	RevealFinalTutorialDialogue(TutorialPC);
	TutorialPC->NotifyTutorialMoveInput(FVector2D(1.0f, 0.0f));
	RevealFinalTutorialDialogue(TutorialPC);
	TutorialPC->NotifyTutorialAttackInput();
	RevealFinalTutorialDialogue(TutorialPC);
	TutorialPC->NotifyTutorialDodgeInput(FVector2D(0.0f, 1.0f));
	AdvanceDialogueStepToNext(TutorialPC);
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
	AdvanceDialogueStepToNext(TutorialPC);
	RevealFinalTutorialDialogue(TutorialPC);
	TutorialPC->NotifyTutorialMoveInput(FVector2D(1.0f, 0.0f));
	RevealFinalTutorialDialogue(TutorialPC);
	TutorialPC->NotifyTutorialAttackInput();
	RevealFinalTutorialDialogue(TutorialPC);
	TutorialPC->NotifyTutorialDodgeInput(FVector2D(0.0f, 1.0f));
	AdvanceDialogueStepToNext(TutorialPC);

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
	AdvanceDialogueStepToNext(TutorialPC);
	RevealFinalTutorialDialogue(TutorialPC);
	TestTrue(TEXT("real drone move input is accepted during move step"), Drone->ApplyMoveInputForServerForTest(FVector2D(1.0f, 0.0f)));
	TestTrue(TEXT("real drone movement is applied"), Drone->ApplyPendingServerMoveInputForTest(0.1f));
	TestEqual(TEXT("real movement advances tutorial"), TutorialPC->GetCurrentTutorialStep(), ETutorialStep::Attack);

	Drone->RequestAttackBoss();
	TestEqual(TEXT("first Z advances attack dialogue"), TutorialPC->GetCurrentTutorialDialogueIndex(), 1);
	Drone->RequestAttackBoss();
	TestEqual(TEXT("second Z reveals attack instruction"), TutorialPC->GetCurrentTutorialDialogueIndex(), 2);
	Drone->RequestAttackBoss();
	TestEqual(TEXT("real attack advances tutorial without a target"), TutorialPC->GetCurrentTutorialStep(), ETutorialStep::Dodge);
	TestTrue(TEXT("first Z advances dodge dialogue"), TutorialPC->TryAdvanceTutorialDialogue());
	TestTrue(TEXT("second Z reveals dodge instruction"), TutorialPC->TryAdvanceTutorialDialogue());
	TestTrue(TEXT("real dodge is accepted during dodge step"), Drone->RequestDodgeForServer(FVector2D(0.0f, 1.0f)));
	TestEqual(TEXT("real dodge advances tutorial"), TutorialPC->GetCurrentTutorialStep(), ETutorialStep::CombatBriefing);
	Drone->TickForTest(1.0f);

	Drone->RequestAttackBoss();
	TestEqual(TEXT("first combat Z advances briefing"), TutorialPC->GetCurrentTutorialDialogueIndex(), 1);
	Drone->RequestAttackBoss();
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDronePOR23LocalProfilePersistenceTest,
	"DroneProto.POR23.Profile.LocalPersistenceAndReport",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDronePOR23LocalProfilePersistenceTest::RunTest(const FString& Parameters)
{
	const FString TestSlot = TEXT("DroneProtoAutomation_POR23_LocalProfile");
	UGameplayStatics::DeleteGameInSlot(TestSlot, 0);

	UGameInstance* FirstGameInstance = NewObject<UGameInstance>();
	URaidSessionSubsystem* FirstSession = NewObject<URaidSessionSubsystem>(FirstGameInstance);
	TestNotNull(TEXT("first local profile session is created"), FirstSession);
	if (!FirstSession)
	{
		return false;
	}

	FirstSession->SetProfileSaveSlotForTest(TestSlot);
	TestFalse(TEXT("empty automation slot has no saved profile"), FirstSession->ReloadLocalProfileForTest());
	TestEqual(TEXT("new profile starts with AAA callsign"), FirstSession->GetCallsign(), FString(TEXT("AAA")));
	TestFalse(TEXT("new profile has not completed tutorial"), FirstSession->HasCompletedTutorial());
	TestEqual(TEXT("new profile routes to tutorial map"), FirstSession->GetPostLoginMapName(), FName(TEXT("TestMap")));
	TestFalse(TEXT("short callsign is rejected"), FirstSession->TryLoginWithCallsign(TEXT("AB")));
	TestFalse(TEXT("non-letter callsign is rejected"), FirstSession->TryLoginWithCallsign(TEXT("A1C")));
	TestTrue(TEXT("lowercase callsign is normalized and saved"), FirstSession->TryLoginWithCallsign(TEXT("aBc")));
	TestEqual(TEXT("saved callsign is uppercase"), FirstSession->GetCallsign(), FString(TEXT("ABC")));

	UGameInstance* SecondGameInstance = NewObject<UGameInstance>();
	URaidSessionSubsystem* SecondSession = NewObject<URaidSessionSubsystem>(SecondGameInstance);
	TestNotNull(TEXT("second local profile session is created"), SecondSession);
	if (!SecondSession)
	{
		UGameplayStatics::DeleteGameInSlot(TestSlot, 0);
		return false;
	}

	SecondSession->SetProfileSaveSlotForTest(TestSlot);
	TestTrue(TEXT("saved profile reloads"), SecondSession->ReloadLocalProfileForTest());
	TestEqual(TEXT("reloaded callsign is preserved"), SecondSession->GetCallsign(), FString(TEXT("ABC")));
	TestFalse(TEXT("tutorial completion remains false before completion"), SecondSession->HasCompletedTutorial());
	TestTrue(TEXT("tutorial completion is persisted"), SecondSession->MarkTutorialCompleted());
	TestEqual(TEXT("completed profile routes to lobby"), SecondSession->GetPostLoginMapName(), FName(TEXT("LobbyMap")));

	UGameInstance* ThirdGameInstance = NewObject<UGameInstance>();
	URaidSessionSubsystem* ThirdSession = NewObject<URaidSessionSubsystem>(ThirdGameInstance);
	TestNotNull(TEXT("third local profile session is created"), ThirdSession);
	if (ThirdSession)
	{
		ThirdSession->SetProfileSaveSlotForTest(TestSlot);
		TestTrue(TEXT("completed profile reloads"), ThirdSession->ReloadLocalProfileForTest());
		TestTrue(TEXT("tutorial completion survives reload"), ThirdSession->HasCompletedTutorial());

		FDroneReportData ReportData;
		ReportData.Callsign = ThirdSession->GetCallsign();
		UDroneReportWidget* ReportWidget = NewObject<UDroneReportWidget>();
		TestNotNull(TEXT("report widget is created"), ReportWidget);
		if (ReportWidget)
		{
			ReportWidget->RefreshReport(ReportData);
			TestEqual(TEXT("DroneReport displays the local callsign"), ReportWidget->GetCallsignText().ToString(), FString(TEXT("ABC")));
		}
	}

	UGameplayStatics::DeleteGameInSlot(TestSlot, 0);
	return true;
}

#endif
