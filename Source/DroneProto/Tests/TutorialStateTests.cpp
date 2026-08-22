#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Tutorial/TutorialGameMode.h"
#include "Tutorial/TutorialDebris.h"
#include "Tutorial/TutorialHUDWidget.h"
#include "Tutorial/TutorialPlayerController.h"
#include "Tutorial/TutorialTypes.h"
#include "Drone.h"
#include "Lobby/RaidLobbyWidget.h"
#include "Lobby/RaidSessionSubsystem.h"
#include "Raid/DroneReportWidget.h"
#include "Raid/RaidBoss.h"
#include "Raid/RaidGameState.h"
#include "Raid/RaidPlayerController.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Blueprint/WidgetTree.h"
#include "Components/EditableTextBox.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextBlock.h"
#include "GameFramework/WorldSettings.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SWindow.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UnrealType.h"

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

bool CompleteTutorialMoveStep(ATutorialPlayerController* TutorialPC)
{
	if (!TutorialPC)
	{
		return false;
	}

	TutorialPC->NotifyTutorialMoveInput(FVector2D(1.0f, 0.0f), 1.5f);
	return TutorialPC->NotifyTutorialMoveInput(FVector2D::ZeroVector, 0.0f);
}

bool CompleteTutorialAttackStep(ATutorialPlayerController* TutorialPC)
{
	return TutorialPC
		&& TutorialPC->NotifyTutorialAttackInput()
		&& TutorialPC->NotifyTutorialAttackInputReleased();
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDronePOR23AssetContractTest,
	"DroneProto.POR23.Assets.MapAndWidgetContracts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDronePOR23AssetContractTest::RunTest(const FString& Parameters)
{
	UWorld* LobbyMap = LoadObject<UWorld>(nullptr, TEXT("/Game/LobbyMap.LobbyMap"));
	UWorld* TestMap = LoadObject<UWorld>(nullptr, TEXT("/Game/TestMap.TestMap"));
	TestNotNull(TEXT("LobbyMap asset loads"), LobbyMap);
	TestNotNull(TEXT("TestMap asset loads"), TestMap);

	if (LobbyMap)
	{
		const AWorldSettings* LobbyWorldSettings = LobbyMap->GetWorldSettings();
		TestNotNull(TEXT("LobbyMap has world settings"), LobbyWorldSettings);
		if (LobbyWorldSettings)
		{
			TestEqual(
				TEXT("LobbyMap keeps its lobby game mode"),
				GetNameSafe(LobbyWorldSettings->DefaultGameMode),
				FString(TEXT("BP_LobbyGameMode_C")));
		}
	}

	if (TestMap)
	{
		const AWorldSettings* TestWorldSettings = TestMap->GetWorldSettings();
		TestNotNull(TEXT("TestMap has world settings"), TestWorldSettings);
		if (TestWorldSettings)
		{
			TestEqual(
				TEXT("TestMap keeps its raid game mode for normal raid entry"),
				GetNameSafe(TestWorldSettings->DefaultGameMode),
				FString(TEXT("BP_RaidGameMode_C")));
		}
	}

	UClass* TutorialHUDClass = LoadClass<UTutorialHUDWidget>(
		nullptr,
		TEXT("/Game/UI/WBP_TutorialHUD.WBP_TutorialHUD_C"));
	UClass* DroneReportClass = LoadClass<UDroneReportWidget>(
		nullptr,
		TEXT("/Game/UI/WBP_DroneReport.WBP_DroneReport_C"));
	TestNotNull(TEXT("tutorial HUD Blueprint class loads"), TutorialHUDClass);
	TestNotNull(TEXT("DroneReport Blueprint class loads"), DroneReportClass);

	UWorld* WidgetWorld = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("POR23AssetContractWorld")));
	TestNotNull(TEXT("asset contract widget world is created"), WidgetWorld);
	if (!WidgetWorld)
	{
		return false;
	}

	if (TutorialHUDClass)
	{
		UTutorialHUDWidget* TutorialHUD = CreateWidget<UTutorialHUDWidget>(WidgetWorld, TutorialHUDClass);
		TestNotNull(TEXT("tutorial HUD Blueprint instance is created"), TutorialHUD);
		if (TutorialHUD && TutorialHUD->WidgetTree)
		{
			TestNotNull(TEXT("tutorial HUD binds DialoguePanel"), TutorialHUD->WidgetTree->FindWidget(TEXT("DialoguePanel")));
			TestNotNull(TEXT("tutorial HUD binds DialogueText"), TutorialHUD->WidgetTree->FindWidget(TEXT("DialogueText")));
			TestNotNull(TEXT("tutorial HUD binds StepText"), TutorialHUD->WidgetTree->FindWidget(TEXT("StepText")));
			TestNotNull(TEXT("tutorial HUD binds InputHintText"), TutorialHUD->WidgetTree->FindWidget(TEXT("InputHintText")));
		}
	}

	if (DroneReportClass)
	{
		UDroneReportWidget* DroneReport = CreateWidget<UDroneReportWidget>(WidgetWorld, DroneReportClass);
		TestNotNull(TEXT("DroneReport Blueprint instance is created"), DroneReport);
		if (DroneReport && DroneReport->WidgetTree)
		{
			TestNotNull(TEXT("DroneReport binds CallsignText"), DroneReport->WidgetTree->FindWidget(TEXT("CallsignText")));
		}
	}

	WidgetWorld->DestroyWorld(false);
	return true;
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
	TestFalse(TEXT("short movement does not end the move step"), TutorialPC->NotifyTutorialMoveInput(FVector2D(1.0f, 0.0f), 1.0f));
	TestFalse(TEXT("releasing before enough movement keeps the move step"), TutorialPC->NotifyTutorialMoveInput(FVector2D::ZeroVector, 0.0f));
	TestFalse(TEXT("enough movement waits for input release"), TutorialPC->NotifyTutorialMoveInput(FVector2D(1.0f, 0.0f), 0.5f));
	TestTrue(TEXT("releasing after 1.5 meters advances to attack"), TutorialPC->NotifyTutorialMoveInput(FVector2D::ZeroVector, 0.0f));
	TestEqual(TEXT("movement release enters attack"), TutorialPC->GetCurrentTutorialStep(), ETutorialStep::Attack);

	TestFalse(TEXT("attack is blocked before final instruction"), TutorialPC->NotifyTutorialAttackInput());
	TestTrue(TEXT("Z advances first attack instruction"), TutorialPC->TryAdvanceTutorialDialogue());
	TestTrue(TEXT("Z advances second attack instruction"), TutorialPC->TryAdvanceTutorialDialogue());
	TestTrue(TEXT("actual attack is accepted"), TutorialPC->NotifyTutorialAttackInput());
	TestEqual(TEXT("attack press keeps the attack step"), TutorialPC->GetCurrentTutorialStep(), ETutorialStep::Attack);
	TestTrue(TEXT("attack release advances to dodge"), TutorialPC->NotifyTutorialAttackInputReleased());
	TestEqual(TEXT("attack release enters dodge"), TutorialPC->GetCurrentTutorialStep(), ETutorialStep::Dodge);

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
	CompleteTutorialMoveStep(TutorialPC);
	RevealFinalTutorialDialogue(TutorialPC);
	CompleteTutorialAttackStep(TutorialPC);
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
	UStaticMeshComponent* DebrisMesh = Debris ? Debris->FindComponentByClass<UStaticMeshComponent>() : nullptr;
	FProperty* HitCountProperty = FindFProperty<FProperty>(ATutorialDebris::StaticClass(), TEXT("TutorialHitCount"));
	FProperty* DestroyedProperty = FindFProperty<FProperty>(ATutorialDebris::StaticClass(), TEXT("bTutorialDebrisDestroyed"));
	TestNotNull(TEXT("tutorial controller is spawned"), TutorialPC);
	TestNotNull(TEXT("tutorial debris is spawned"), Debris);
	TestNotNull(TEXT("tutorial debris has a visible mesh component"), DebrisMesh);
	if (DebrisMesh)
	{
		TestNotNull(TEXT("tutorial debris mesh component has a mesh asset"), DebrisMesh->GetStaticMesh().Get());
	}
	TestNotNull(TEXT("tutorial hit count is reflected"), HitCountProperty);
	TestNotNull(TEXT("tutorial destroyed state is reflected"), DestroyedProperty);
	if (HitCountProperty)
	{
		TestEqual(TEXT("tutorial hit effects use a client RepNotify"),
			HitCountProperty->RepNotifyFunc, FName(TEXT("OnRep_TutorialHitCount")));
	}
	if (DestroyedProperty)
	{
		TestEqual(TEXT("tutorial destroy effects use a client RepNotify"),
			DestroyedProperty->RepNotifyFunc, FName(TEXT("OnRep_TutorialDebrisDestroyed")));
	}
	if (!TutorialPC || !Debris)
	{
		World->DestroyWorld(false);
		return false;
	}

	TutorialPC->StartTutorial();
	TutorialPC->AdvanceTutorialStep();
	AdvanceDialogueStepToNext(TutorialPC);
	RevealFinalTutorialDialogue(TutorialPC);
	CompleteTutorialMoveStep(TutorialPC);
	RevealFinalTutorialDialogue(TutorialPC);
	CompleteTutorialAttackStep(TutorialPC);
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
	TestFalse(TEXT("third hit waits for the attack visual before destroying debris"), Debris->IsTutorialDebrisDestroyed());
	TestEqual(TEXT("attack visual window keeps debris combat active"),
		TutorialPC->GetCurrentTutorialStep(),
		ETutorialStep::DebrisCombat);
	TestFalse(TEXT("pending destruction rejects extra hits"), Debris->ApplyTutorialHitForServer(TutorialPC));
	Debris->Tick(0.44f);
	TestFalse(TEXT("debris remains during the attack visual"), Debris->IsTutorialDebrisDestroyed());
	Debris->Tick(0.02f);
	TestTrue(TEXT("debris is destroyed after the attack visual"), Debris->IsTutorialDebrisDestroyed());
	TestTrue(TEXT("destroyed debris is hidden after the attack visual"), Debris->IsHidden());
	TestEqual(TEXT("debris destruction then enters closing briefing"),
		TutorialPC->GetCurrentTutorialStep(),
		ETutorialStep::ClosingBriefing);

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
	TestTrue(TEXT("tutorial game mode uses the configured playable drone class"),
		GameMode->DefaultPawnClass
			&& GameMode->DefaultPawnClass != ADrone::StaticClass()
			&& GameMode->DefaultPawnClass->IsChildOf(ADrone::StaticClass()));
	TestNull(TEXT("tutorial starts without debris"), GameMode->GetTutorialDebris());

	GameMode->StartTutorialForController(TutorialPC);
	GameMode->AdvanceTutorialForController(TutorialPC);
	AdvanceDialogueStepToNext(TutorialPC);
	RevealFinalTutorialDialogue(TutorialPC);
	CompleteTutorialMoveStep(TutorialPC);
	RevealFinalTutorialDialogue(TutorialPC);
	CompleteTutorialAttackStep(TutorialPC);
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
	const UFunction* MoveReleaseRPC =
		ADrone::StaticClass()->FindFunctionByName(FName(TEXT("Server_FinishTutorialMoveInput")));
	TestNotNull(TEXT("tutorial move release has a dedicated RPC"), MoveReleaseRPC);
	if (MoveReleaseRPC)
	{
		TestTrue(TEXT("tutorial move release RPC is server-authoritative"),
			MoveReleaseRPC->HasAnyFunctionFlags(FUNC_NetServer));
		TestTrue(TEXT("tutorial move release RPC is reliable"),
			MoveReleaseRPC->HasAnyFunctionFlags(FUNC_NetReliable));
	}
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
	for (int32 MoveSample = 0; MoveSample < 4; ++MoveSample)
	{
		TestTrue(TEXT("real drone move input is accepted during move step"), Drone->ApplyMoveInputForServerForTest(FVector2D(1.0f, 0.0f)));
		TestTrue(TEXT("real drone movement is applied"), Drone->ApplyPendingServerMoveInputForTest(0.1f));
	}
	TestEqual(TEXT("enough real movement waits for release"), TutorialPC->GetCurrentTutorialStep(), ETutorialStep::Move);
	TestFalse(TEXT("move release is not itself movement"), Drone->ApplyMoveInputForServerForTest(FVector2D::ZeroVector));
	TestEqual(TEXT("real movement release advances tutorial"), TutorialPC->GetCurrentTutorialStep(), ETutorialStep::Attack);

	Drone->RequestAttackBoss();
	TestEqual(TEXT("first Z advances attack dialogue"), TutorialPC->GetCurrentTutorialDialogueIndex(), 1);
	Drone->RequestAttackBoss();
	TestEqual(TEXT("second Z reveals attack instruction"), TutorialPC->GetCurrentTutorialDialogueIndex(), 2);
	Drone->RequestAttackBoss();
	TestEqual(TEXT("real attack waits for Z release"), TutorialPC->GetCurrentTutorialStep(), ETutorialStep::Attack);
	Drone->FinishTutorialAttackInputForTest();
	TestEqual(TEXT("real attack release advances tutorial without a target"), TutorialPC->GetCurrentTutorialStep(), ETutorialStep::Dodge);
	TestTrue(TEXT("first Z advances dodge dialogue"), TutorialPC->TryAdvanceTutorialDialogue());
	TestTrue(TEXT("second Z reveals dodge instruction"), TutorialPC->TryAdvanceTutorialDialogue());
	TestTrue(TEXT("real dodge is accepted during dodge step"), Drone->RequestDodgeForServer(FVector2D(0.0f, 1.0f)));
	TestEqual(TEXT("dodge start keeps the dodge step"), TutorialPC->GetCurrentTutorialStep(), ETutorialStep::Dodge);
	Drone->TickForTest(0.25f);
	TestEqual(TEXT("real dodge completion advances tutorial"), TutorialPC->GetCurrentTutorialStep(), ETutorialStep::CombatBriefing);

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
		TestFalse(TEXT("third real attack keeps debris visible for the attack effect"), Debris->IsTutorialDebrisDestroyed());
		TestEqual(TEXT("third real attack keeps debris combat active"),
			TutorialPC->GetCurrentTutorialStep(),
			ETutorialStep::DebrisCombat);
		Debris->Tick(0.45f);
		TestTrue(TEXT("three real attacks destroy tutorial debris after the effect"), Debris->IsTutorialDebrisDestroyed());
	}
	TestEqual(TEXT("real debris combat enters closing briefing"), TutorialPC->GetCurrentTutorialStep(), ETutorialStep::ClosingBriefing);

	const int32 HealthBeforeDamage = Drone->GetHealth();
	Drone->ApplyDamageForServer(50, FName(TEXT("TutorialAutomation")));
	TestEqual(TEXT("tutorial drone is invulnerable"), Drone->GetHealth(), HealthBeforeDamage);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDronePOR23TutorialHUDPresentationTest,
	"DroneProto.POR23.Tutorial.HUDPresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDronePOR23TutorialHUDPresentationTest::RunTest(const FString& Parameters)
{
	UTutorialHUDWidget* Widget = NewObject<UTutorialHUDWidget>();
	TestNotNull(TEXT("tutorial HUD widget is created"), Widget);
	if (!Widget)
	{
		return false;
	}

	Widget->RefreshTutorial(
		ETutorialStep::Move,
		1,
		FText::FromString(TEXT("원하시는 방향으로 [방향키]를 입력해주세요.")),
		true);
	TestEqual(TEXT("dialogue text is cached"),
		Widget->GetDialogueText().ToString(),
		FString(TEXT("원하시는 방향으로 [방향키]를 입력해주세요.")));
	TestEqual(TEXT("move step label is cached"),
		Widget->GetStepText().ToString(),
		FString(TEXT("3. 이동")));
	TestEqual(TEXT("move hint uses the real move input"),
		Widget->GetInputHintText().ToString(),
		FString(TEXT("방향키 : 이동")));

	Widget->RefreshTutorial(
		ETutorialStep::Attack,
		0,
		FText::FromString(TEXT("공격 설명")),
		false);
	TestEqual(TEXT("unfinished instruction uses dialogue advance hint"),
		Widget->GetInputHintText().ToString(),
		FString(TEXT("Z : 다음")));

	Widget->RefreshTutorial(
		ETutorialStep::DebrisCombat,
		0,
		FText::GetEmpty(),
		false);
	TestEqual(TEXT("debris combat shows three-hit attack hint"),
		Widget->GetInputHintText().ToString(),
		FString(TEXT("Z : 잔해 공격 (3회)")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDronePOR23CallsignTypingFilterTest,
	"DroneProto.POR23.Profile.CallsignAcceptsOnlyThreeAsciiLettersWhileTyping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDronePOR23CallsignTypingFilterTest::RunTest(const FString& Parameters)
{
	URaidLobbyWidget* Widget = NewObject<URaidLobbyWidget>();
	UEditableTextBox* Input = NewObject<UEditableTextBox>(Widget);
	UTextBlock* ErrorText = NewObject<UTextBlock>(Widget);
	TestNotNull(TEXT("raid lobby widget is created"), Widget);
	TestNotNull(TEXT("callsign input is created"), Input);
	TestNotNull(TEXT("callsign error text is created"), ErrorText);
	if (!Widget || !Input || !ErrorText)
	{
		return false;
	}

	Input->TakeWidget();
	Widget->SetCallsignInputForTest(Input);
	ErrorText->SetVisibility(ESlateVisibility::Hidden);
	Widget->SetCallsignErrorTextForTest(ErrorText);
	TestTrue(TEXT("callsign input change handler is bound"), Input->OnTextChanged.IsBound());

	TSharedRef<SEditableTextBox> SlateInput =
		StaticCastSharedRef<SEditableTextBox>(Input->TakeWidget());
	TSharedRef<SWindow> TestWindow = SNew(SWindow)
		.ClientSize(FVector2D(320.0f, 120.0f))
		[SlateInput];
	FSlateApplication::Get().AddWindow(TestWindow, false);
	const FModifierKeysState Modifiers;
	TestTrue(TEXT("callsign input receives keyboard focus"),
		FSlateApplication::Get().SetKeyboardFocus(SlateInput, EFocusCause::SetDirectly));
	TestTrue(TEXT("lowercase key is handled before Slate inserts it"),
		FSlateApplication::Get().ProcessKeyCharEvent(
			FCharacterEvent(TEXT('a'), Modifiers, 0, false)));
	TestEqual(TEXT("lowercase key is displayed as uppercase immediately"),
		Input->GetText().ToString(),
		FString(TEXT("A")));
	TestTrue(TEXT("non-ASCII key is blocked before Slate inserts it"),
		FSlateApplication::Get().ProcessKeyCharEvent(
			FCharacterEvent(TEXT('한'), Modifiers, 0, false)));
	TestEqual(TEXT("non-ASCII key does not change the callsign"),
		Input->GetText().ToString(),
		FString(TEXT("A")));
	TestEqual(TEXT("blocked non-ASCII input shows the English-only error"),
		ErrorText->GetText().ToString(),
		FString(TEXT("영문만 입력할 수 있습니다.")));
	TestEqual(TEXT("blocked non-ASCII input makes the error visible"),
		ErrorText->GetVisibility(),
		ESlateVisibility::HitTestInvisible);
	TestTrue(TEXT("lowercase key after non-ASCII input is still handled"),
		FSlateApplication::Get().ProcessKeyCharEvent(
			FCharacterEvent(TEXT('b'), Modifiers, 0, false)));
	TestEqual(TEXT("lowercase input still becomes uppercase after non-ASCII input"),
		Input->GetText().ToString(),
		FString(TEXT("AB")));
	TestEqual(TEXT("valid input hides the previous error"),
		ErrorText->GetVisibility(),
		ESlateVisibility::Hidden);
	FSlateApplication::Get().ProcessKeyCharEvent(
		FCharacterEvent(TEXT('c'), Modifiers, 0, false));
	FSlateApplication::Get().ProcessKeyCharEvent(
		FCharacterEvent(TEXT('d'), Modifiers, 0, false));
	TestEqual(TEXT("fourth letter shows the three-letter limit error"),
		ErrorText->GetText().ToString(),
		FString(TEXT("영문 3자까지만 입력할 수 있습니다.")));
	TestEqual(TEXT("fourth letter makes the error visible"),
		ErrorText->GetVisibility(),
		ESlateVisibility::HitTestInvisible);
	const FCharacterEvent BackspaceEvent(TCHAR(8), Modifiers, 0, false);
	FSlateApplication::Get().ProcessKeyCharEvent(BackspaceEvent);
	FSlateApplication::Get().ProcessKeyCharEvent(BackspaceEvent);
	FSlateApplication::Get().ProcessKeyCharEvent(BackspaceEvent);
	TestEqual(TEXT("deleting all letters empties the callsign"),
		Input->GetText().ToString(),
		FString());
	TestEqual(TEXT("deleting all letters hides the previous error"),
		ErrorText->GetVisibility(),
		ESlateVisibility::Hidden);
	FSlateApplication::Get().ProcessKeyCharEvent(
		FCharacterEvent(TEXT('e'), Modifiers, 0, false));
	TestEqual(TEXT("typing works again after deleting all letters"),
		Input->GetText().ToString(),
		FString(TEXT("E")));

	Input->SetText(FText::GetEmpty());
	TSharedRef<SEditableTextBox> ActiveImeSink =
		SNew(SEditableTextBox)
		.OnKeyCharHandler_Lambda(
			[](const FGeometry&, const FCharacterEvent&)
			{
				return FReply::Handled();
			});
	TSharedRef<SWindow> ActiveImeWindow = SNew(SWindow)
		.ClientSize(FVector2D(320.0f, 120.0f))
		[ActiveImeSink];
	FSlateApplication::Get().AddWindow(ActiveImeWindow, false);
	TestTrue(TEXT("active IME sink receives keyboard focus"),
		FSlateApplication::Get().SetKeyboardFocus(ActiveImeSink, EFocusCause::SetDirectly));
	Widget->HandleCallsignKeyCharForTest(
		FCharacterEvent(TEXT('s'), Modifiers, 0, false));
	Widget->HandleCallsignKeyCharForTest(
		FCharacterEvent(TEXT('d'), Modifiers, 0, false));
	Widget->HandleCallsignKeyCharForTest(
		FCharacterEvent(TEXT('f'), Modifiers, 0, false));
	Widget->HandleCallsignKeyCharForTest(
		FCharacterEvent(TEXT('g'), Modifiers, 0, false));
	TestEqual(TEXT("active IME cannot swallow accepted letters or bypass the three-letter limit"),
		Input->GetText().ToString(),
		FString(TEXT("SDF")));
	Widget->HandleCallsignKeyCharForTest(BackspaceEvent);
	Widget->HandleCallsignKeyCharForTest(BackspaceEvent);
	Widget->HandleCallsignKeyCharForTest(BackspaceEvent);
	TestEqual(TEXT("active IME backspace removes all accepted letters"),
		Input->GetText().ToString(),
		FString());
	Widget->HandleCallsignKeyCharForTest(
		FCharacterEvent(TEXT('a'), Modifiers, 0, false));
	TestEqual(TEXT("typing resumes after active IME backspace removes all letters"),
		Input->GetText().ToString(),
		FString(TEXT("A")));
	FSlateApplication::Get().RequestDestroyWindow(ActiveImeWindow);
	FSlateApplication::Get().RequestDestroyWindow(TestWindow);

	Input->SetText(FText::GetEmpty());
	const FText MixedInput = FText::FromString(TEXT("a한1b-cD"));
	Input->SetText(MixedInput);
	Input->OnTextChanged.Broadcast(MixedInput);
	TestEqual(TEXT("paste or IME input is sanitized before the change handler returns"),
		Input->GetText().ToString(),
		FString(TEXT("ABC")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDronePOR23CallsignAutoLoginTest,
	"DroneProto.POR23.Profile.CallsignEntryStartsEmptyThenAutoLogsIn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDronePOR23CallsignAutoLoginTest::RunTest(const FString& Parameters)
{
	const FString TestSlot = TEXT("DroneProtoAutomation_POR23_AutoLogin");
	UGameplayStatics::DeleteGameInSlot(TestSlot, 0);

	UGameInstance* GameInstance = NewObject<UGameInstance>();
	URaidSessionSubsystem* Session = NewObject<URaidSessionSubsystem>(GameInstance);
	URaidLobbyWidget* Widget = NewObject<URaidLobbyWidget>();
	UEditableTextBox* Input = NewObject<UEditableTextBox>(Widget);
	UTextBlock* ErrorText = NewObject<UTextBlock>(Widget);
	TestNotNull(TEXT("auto-login session is created"), Session);
	TestNotNull(TEXT("auto-login widget is created"), Widget);
	TestNotNull(TEXT("auto-login input is created"), Input);
	if (!Session || !Widget || !Input || !ErrorText)
	{
		return false;
	}

	Session->SetProfileSaveSlotForTest(TestSlot);
	TestTrue(TEXT("completed profile is saved for lobby-only auto-login"), Session->MarkTutorialCompleted());
	Widget->SetRaidSubsystemForTest(Session);
	Widget->SetCallsignErrorTextForTest(ErrorText);
	Widget->SetCallsignInputForTest(Input);

	TestEqual(TEXT("callsign entry initially starts empty"),
		Input->GetText().ToString(),
		FString());
	TestEqual(TEXT("callsign entry shows AAA as hint text"),
		Input->GetHintText().ToString(),
		FString(TEXT("AAA")));
	const FModifierKeysState Modifiers;
	Widget->HandleCallsignKeyCharForTest(FCharacterEvent(TEXT('b'), Modifiers, 0, false));
	TestEqual(TEXT("first new letter fills the first empty slot"),
		Input->GetText().ToString(),
		FString(TEXT("B")));
	TestFalse(TEXT("one new letter does not log in"), Session->IsCallsignIdentified());
	Widget->HandleCallsignKeyCharForTest(FCharacterEvent(TEXT('c'), Modifiers, 0, false));
	TestFalse(TEXT("two new letters do not log in"), Session->IsCallsignIdentified());
	Widget->HandleCallsignKeyCharForTest(FCharacterEvent(TEXT('d'), Modifiers, 0, false));
	TestFalse(TEXT("third new letter remains visible before automatic login"), Session->IsCallsignIdentified());
	TestTrue(TEXT("third new letter schedules automatic login"), Widget->IsCallsignAutoSubmitPendingForTest());
	Widget->HandleCallsignKeyCharForTest(FCharacterEvent(TCHAR(8), Modifiers, 0, false));
	TestFalse(TEXT("editing the completed callsign cancels automatic login"), Widget->IsCallsignAutoSubmitPendingForTest());
	Widget->CompleteCallsignAutoSubmitDelayForTest();
	TestFalse(TEXT("cancelled automatic login cannot complete"), Session->IsCallsignIdentified());
	Widget->HandleCallsignKeyCharForTest(FCharacterEvent(TEXT('d'), Modifiers, 0, false));
	TestTrue(TEXT("completed callsign reschedules automatic login"), Widget->IsCallsignAutoSubmitPendingForTest());
	Widget->CompleteCallsignAutoSubmitDelayForTest();
	TestTrue(TEXT("automatic login completes after the confirmation delay"), Session->IsCallsignIdentified());
	TestEqual(TEXT("automatic login saves the three new uppercase letters"),
		Session->GetCallsign(),
		FString(TEXT("BCD")));

	UGameplayStatics::DeleteGameInSlot(TestSlot, 0);
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
	TestFalse(TEXT("callsign is not identified before submit"), FirstSession->IsCallsignIdentified());
	TestEqual(TEXT("new profile routes to tutorial map"), FirstSession->GetPostLoginMapName(), FName(TEXT("TestMap")));
	TestEqual(TEXT("tutorial route overrides TestMap raid mode"),
		FirstSession->GetPostLoginTravelOptions(),
		FString(TEXT("game=/Script/DroneProto.TutorialGameMode")));
	TestFalse(TEXT("short callsign is rejected"), FirstSession->TryLoginWithCallsign(TEXT("AB")));
	TestFalse(TEXT("non-letter callsign is rejected"), FirstSession->TryLoginWithCallsign(TEXT("A1C")));
	TestTrue(TEXT("lowercase callsign is normalized and saved"), FirstSession->TryLoginWithCallsign(TEXT("aBc")));
	TestTrue(TEXT("valid callsign identifies this session"), FirstSession->IsCallsignIdentified());
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
	TestTrue(TEXT("completed profile needs no game mode override"), SecondSession->GetPostLoginTravelOptions().IsEmpty());

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
