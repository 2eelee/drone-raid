#include "CoreMinimal.h"
#include "Drone.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Raid/BalanceTelemetryComponent.h"
#include "Raid/RaidGameMode.h"
#include "Raid/RaidGameState.h"
#include "Raid/RaidPlayerController.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBalanceTelemetryFormatSchemaTest,
	"DroneProto.Telemetry.Format.SchemaAndSequence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBalanceTelemetryFormatSchemaTest::RunTest(const FString& Parameters)
{
	const TArray<FBalanceTelemetryField> Fields{
		{TEXT("Player"), TEXT("P1")},
		{TEXT("AppliedDamage"), TEXT("25.00")},
	};
	FString Line;

	TestTrue(TEXT("safe fields produce a telemetry line"),
		FBalanceTelemetryFormatter::TryFormat(
			TEXT("SESSION"),
			7,
			1.25,
			TEXT("PIE"),
			TEXT("Dev"),
			TEXT("B1"),
			TEXT("AttackResolved"),
			Fields,
			Line));
	TestTrue(TEXT("schema version is explicit"), Line.Contains(TEXT("Telemetry Schema=1")));
	TestTrue(TEXT("sequence is explicit"), Line.Contains(TEXT("Seq=7")));
	TestTrue(TEXT("safe player alias is preserved"), Line.Contains(TEXT("Player=P1")));
	TestEqual(TEXT("non-ASCII tokens are replaced"), FBalanceTelemetryFormatter::SanitizeToken(TEXT("한글 value")), FString(TEXT("___value")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBalanceTelemetryPrivacyTest,
	"DroneProto.Telemetry.Format.RejectsIdentityFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBalanceTelemetryPrivacyTest::RunTest(const FString& Parameters)
{
	const TArray<FBalanceTelemetryField> UnsafeFields{
		{TEXT("Callsign"), TEXT("PlayerName")},
	};
	FString Line = TEXT("unchanged");

	TestFalse(TEXT("identity fields are rejected"),
		FBalanceTelemetryFormatter::TryFormat(
			TEXT("SESSION"),
			8,
			1.30,
			TEXT("PIE"),
			TEXT("Dev"),
			TEXT("B1"),
			TEXT("PlayerJoined"),
			UnsafeFields,
			Line));
	TestTrue(TEXT("rejected events do not leak the identity value"), !Line.Contains(TEXT("PlayerName")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBalanceTelemetrySessionAliasTest,
	"DroneProto.Telemetry.Session.StableAliasesAndSequence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBalanceTelemetrySessionAliasTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("BalanceTelemetrySessionWorld"));
	TestNotNull(TEXT("world is created"), World);
	if (!World)
	{
		return false;
	}

	ARaidGameMode* GameMode = World->SpawnActor<ARaidGameMode>();
	ARaidPlayerController* FirstPlayer = World->SpawnActor<ARaidPlayerController>();
	ARaidPlayerController* SecondPlayer = World->SpawnActor<ARaidPlayerController>();
	TestNotNull(TEXT("raid game mode is spawned"), GameMode);
	TestNotNull(TEXT("first player is spawned"), FirstPlayer);
	TestNotNull(TEXT("second player is spawned"), SecondPlayer);
	if (!GameMode || !FirstPlayer || !SecondPlayer)
	{
		World->DestroyWorld(false);
		return false;
	}

	UBalanceTelemetryComponent* Telemetry = GameMode->GetBalanceTelemetryForServer();
	TestNotNull(TEXT("game mode owns telemetry"), Telemetry);
	if (Telemetry)
	{
		Telemetry->StartSessionForServer(TEXT("Local"), TEXT("TestMap"), TEXT("B1"));
		TestEqual(TEXT("first player receives P1"), Telemetry->GetOrAssignPlayerAliasForServer(FirstPlayer), FString(TEXT("P1")));
		TestEqual(TEXT("same player keeps P1"), Telemetry->GetOrAssignPlayerAliasForServer(FirstPlayer), FString(TEXT("P1")));
		TestEqual(TEXT("second player receives P2"), Telemetry->GetOrAssignPlayerAliasForServer(SecondPlayer), FString(TEXT("P2")));

		Telemetry->EmitForServer(TEXT("PlayerJoined"), {{TEXT("Player"), TEXT("P1")}});
		Telemetry->EmitForServer(TEXT("PlayerJoined"), {{TEXT("Player"), TEXT("P2")}});
		const TArray<FString>& Lines = Telemetry->GetEmittedLinesForTest();
		TestEqual(TEXT("session start and two joins are emitted"), Lines.Num(), 3);
		if (Lines.Num() == 3)
		{
			TestTrue(TEXT("first join sequence follows session start"), Lines[1].Contains(TEXT("Seq=2")));
			TestTrue(TEXT("second join sequence increments"), Lines[2].Contains(TEXT("Seq=3")));
		}
	}

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBalanceTelemetryPlayerJoinLifecycleTest,
	"DroneProto.Telemetry.Lifecycle.PlayerJoinEnvironment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBalanceTelemetryPlayerJoinLifecycleTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("BalanceTelemetryJoinWorld"));
	TestNotNull(TEXT("world is created"), World);
	if (!World)
	{
		return false;
	}

	ARaidGameMode* GameMode = World->SpawnActor<ARaidGameMode>();
	ARaidPlayerController* FirstPlayer = World->SpawnActor<ARaidPlayerController>();
	ARaidPlayerController* LatePlayer = World->SpawnActor<ARaidPlayerController>();
	TestNotNull(TEXT("game mode is spawned"), GameMode);
	if (!GameMode || !FirstPlayer || !LatePlayer)
	{
		World->DestroyWorld(false);
		return false;
	}

	UBalanceTelemetryComponent* Telemetry = GameMode->GetBalanceTelemetryForServer();
	Telemetry->StartSessionForServer(TEXT("Local"), TEXT("TestMap"), TEXT("B1"));
	Telemetry->RecordPlayerJoinedForServer(FirstPlayer, false, 1);
	Telemetry->RecordPlayerJoinedForServer(LatePlayer, true, 2);

	const TArray<FString>& Lines = Telemetry->GetEmittedLinesForTest();
	TestEqual(TEXT("session start and two accepted joins are emitted"), Lines.Num(), 3);
	if (Lines.Num() == 3)
	{
		TestTrue(TEXT("first accepted player is not a late join"), Lines[1].Contains(TEXT("Event=PlayerJoined")) && Lines[1].Contains(TEXT("LateJoin=0")));
		TestTrue(TEXT("battle player is a late join"), Lines[2].Contains(TEXT("Event=PlayerJoined")) && Lines[2].Contains(TEXT("LateJoin=1")));
		TestTrue(TEXT("environment is recorded"), Lines[2].Contains(TEXT("Environment=Automation")));
	}

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBalanceTelemetryCombatResultsTest,
	"DroneProto.Telemetry.Combat.AuthoritativeResults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBalanceTelemetryCombatResultsTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("BalanceTelemetryCombatWorld"));
	TestNotNull(TEXT("world is created"), World);
	if (!World)
	{
		return false;
	}

	ARaidGameState* GameState = World->SpawnActor<ARaidGameState>();
	ARaidGameMode* GameMode = World->SpawnActor<ARaidGameMode>();
	ARaidPlayerController* Player = World->SpawnActor<ARaidPlayerController>();
	ADrone* Drone = World->SpawnActor<ADrone>();
	TestNotNull(TEXT("game state is spawned"), GameState);
	TestNotNull(TEXT("game mode is spawned"), GameMode);
	TestNotNull(TEXT("player is spawned"), Player);
	TestNotNull(TEXT("drone is spawned"), Drone);
	if (!GameState || !GameMode || !Player || !Drone)
	{
		World->DestroyWorld(false);
		return false;
	}

	World->SetGameState(GameState);
	Player->Possess(Drone);
	UBalanceTelemetryComponent* Telemetry = GameMode->GetBalanceTelemetryForServer();
	Telemetry->StartSessionForServer(TEXT("Local"), TEXT("TestMap"), TEXT("B1"));
	Telemetry->GetOrAssignPlayerAliasForServer(Player);

	Drone->RequestAttackBoss();
	Drone->ApplyDamageForServer(10, TEXT("Automation"));
	Drone->RequestDodgeForServer(FVector2D::ZeroVector);

	bool bFoundAttempt = false;
	bool bFoundRejectedAttack = false;
	bool bFoundDamage = false;
	bool bFoundRejectedDodge = false;
	for (const FString& Line : Telemetry->GetEmittedLinesForTest())
	{
		bFoundAttempt |= Line.Contains(TEXT("Event=AttackAttempted"));
		bFoundRejectedAttack |= Line.Contains(TEXT("Event=AttackResolved")) && Line.Contains(TEXT("Reason=NotInBattle"));
		bFoundDamage |= Line.Contains(TEXT("Event=PlayerDamageResolved")) && Line.Contains(TEXT("AppliedDamage=10.000"));
		bFoundRejectedDodge |= Line.Contains(TEXT("Event=DodgeResolved")) && Line.Contains(TEXT("Reason=NoDirection"));
	}
	TestTrue(TEXT("attack attempt is recorded once authority accepts the caller"), bFoundAttempt);
	TestTrue(TEXT("rejected attack records its server reason"), bFoundRejectedAttack);
	TestTrue(TEXT("actual player damage is recorded"), bFoundDamage);
	TestTrue(TEXT("rejected dodge records its server reason"), bFoundRejectedDodge);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBalanceTelemetrySessionEndTest,
	"DroneProto.Telemetry.Session.EndIsFinalAndIdempotent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBalanceTelemetrySessionEndTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("BalanceTelemetryEndWorld"));
	TestNotNull(TEXT("world is created"), World);
	if (!World)
	{
		return false;
	}

	ARaidGameMode* GameMode = World->SpawnActor<ARaidGameMode>();
	UBalanceTelemetryComponent* Telemetry = GameMode ? GameMode->GetBalanceTelemetryForServer() : nullptr;
	TestNotNull(TEXT("telemetry is available"), Telemetry);
	if (Telemetry)
	{
		Telemetry->StartSessionForServer(TEXT("Local"), TEXT("TestMap"), TEXT("B1"));
		Telemetry->EndSessionForServer(TEXT("RaidTimeLimit"), 1, 25.0f);
		Telemetry->EndSessionForServer(TEXT("BossDefeated"), 1, 0.0f);
		Telemetry->EmitForServer(TEXT("PlayerLeft"), {{TEXT("Player"), TEXT("P1")}});

		const TArray<FString>& Lines = Telemetry->GetEmittedLinesForTest();
		TestEqual(TEXT("session end remains the final event"), Lines.Num(), 2);
		if (Lines.Num() >= 2)
		{
			TestTrue(TEXT("time limit outcome is normalized"), Lines[1].Contains(TEXT("Event=RaidEnded")) && Lines[1].Contains(TEXT("Outcome=TimeOver")));
		}
	}

	World->DestroyWorld(false);
	return true;
}

#endif
