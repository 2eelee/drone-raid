#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Lobby/RemoteRaidAssignment.h"
#include "Lobby/RaidSessionSubsystem.h"
#include "Raid/RaidReservationLedger.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRaidReservationAtomicCapacityExpiryAndReplayTest,
	"DroneProto.RaidEntry.Reservation.AtomicCapacityExpiryAndReplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRaidReservationAtomicCapacityExpiryAndReplayTest::RunTest(const FString& Parameters)
{
	FRaidReservationLedger CapacityLedger(16, 10.0);
	for (int32 Index = 0; Index < 16; ++Index)
	{
		FString Token;
		TestTrue(TEXT("first sixteen reservations succeed"), CapacityLedger.TryReserve(100.0, Token));
		TestFalse(TEXT("issued token is not empty"), Token.IsEmpty());
	}

	FString OverflowToken;
	TestFalse(TEXT("seventeenth reservation is rejected"), CapacityLedger.TryReserve(100.0, OverflowToken));

	FRaidReservationLedger ClaimLedger(16, 10.0);
	FString ClaimToken;
	TestTrue(TEXT("reservation for claim succeeds"), ClaimLedger.TryReserve(200.0, ClaimToken));
	TestTrue(TEXT("first claim succeeds"), ClaimLedger.TryClaim(ClaimToken, 201.0));
	TestFalse(TEXT("replayed claim is rejected"), ClaimLedger.TryClaim(ClaimToken, 201.0));
	TestTrue(TEXT("claimed token commits"), ClaimLedger.TryCommitClaimed(ClaimToken));
	TestEqual(TEXT("commit converts reservation to active"), ClaimLedger.GetActivePlayers(), 1);
	TestTrue(TEXT("active player releases"), ClaimLedger.ReleaseActivePlayer());
	TestEqual(TEXT("release returns active count to zero"), ClaimLedger.GetActivePlayers(), 0);

	FRaidReservationLedger ExpiryLedger(1, 10.0);
	FString ExpiringToken;
	TestTrue(TEXT("expiring reservation succeeds"), ExpiryLedger.TryReserve(300.0, ExpiringToken));
	ExpiryLedger.Expire(310.01);
	TestEqual(TEXT("expired token returns capacity"), ExpiryLedger.GetReservedPlayers(310.01), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRaidRemoteAssignmentPriorityRaceAndFailureTest,
	"DroneProto.RaidEntry.Assignment.RemotePriorityRaceAndFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRaidRemoteAssignmentPriorityRaceAndFailureTest::RunTest(const FString& Parameters)
{
	URemoteRaidAssignment* Assignment = NewObject<URemoteRaidAssignment>();
	TestNotNull(TEXT("remote assignment is created"), Assignment);
	if (!Assignment)
	{
		return false;
	}

	Assignment->SetServersForTest({
		FRaidServerDefinition{TEXT("A"), 0, TEXT("http://127.0.0.1:7787/raid/reservations")},
		FRaidServerDefinition{TEXT("B"), 1, TEXT("http://127.0.0.1:7788/raid/reservations")},
		FRaidServerDefinition{TEXT("C"), 2, TEXT("http://127.0.0.1:7789/raid/reservations")},
	});

	const FString BSuccess = TEXT("{\"result\":\"success\",\"slot\":\"B\",\"gameEndpoint\":\"127.0.0.1:7778\",\"token\":\"token-b\",\"currentPlayers\":15,\"maxPlayers\":16,\"reason\":\"\"}");
	Assignment->SetResponsesForTest({
		FRaidAssignmentHttpTestResponse{true, 409, TEXT("{\"result\":\"full\",\"slot\":\"A\",\"reason\":\"Full\"}")},
		FRaidAssignmentHttpTestResponse{true, 201, BSuccess},
	});

	int32 CompletionCount = 0;
	FRaidAssignmentResult Result;
	Assignment->ResolveRaidAssignmentAsync(TEXT("A"), 10.0, FRaidAssignmentComplete::CreateLambda(
		[&CompletionCount, &Result](const FRaidAssignmentResult& InResult)
		{
			++CompletionCount;
			Result = InResult;
		}));
	TestEqual(TEXT("race fallback completes once"), CompletionCount, 1);
	TestEqual(TEXT("full A falls back to B"), Result.SelectedSlotId, FName(TEXT("B")));
	TestEqual(TEXT("remote success carries B endpoint"), Result.Endpoint.TravelTarget, FString(TEXT("127.0.0.1:7778")));
	TestEqual(TEXT("remote success carries reservation token"), Result.ReservationToken, FString(TEXT("token-b")));

	Assignment->SetResponsesForTest({
		FRaidAssignmentHttpTestResponse{true, 409, TEXT("{}")},
		FRaidAssignmentHttpTestResponse{true, 409, TEXT("{}")},
		FRaidAssignmentHttpTestResponse{true, 409, TEXT("{}")},
	});
	Assignment->ResolveRaidAssignmentAsync(TEXT("A"), 10.0, FRaidAssignmentComplete::CreateLambda(
		[&Result](const FRaidAssignmentResult& InResult) { Result = InResult; }));
	TestEqual(TEXT("all valid full responses enter waiting"), Result.Result, ERaidAssignmentResultType::Waiting);
	TestEqual(TEXT("all full preserves no server reason"), Result.FailReason, ERaidEntryFailReason::NoServerAvailable);

	Assignment->SetResponsesForTest({
		FRaidAssignmentHttpTestResponse{false, 0, TEXT("")},
		FRaidAssignmentHttpTestResponse{false, 0, TEXT("")},
		FRaidAssignmentHttpTestResponse{false, 0, TEXT("")},
	});
	Assignment->ResolveRaidAssignmentAsync(TEXT("A"), 10.0, FRaidAssignmentComplete::CreateLambda(
		[&Result](const FRaidAssignmentResult& InResult) { Result = InResult; }));
	TestEqual(TEXT("all transport failures fail assignment"), Result.Result, ERaidAssignmentResultType::Failed);
	TestEqual(TEXT("transport failure is server list failure"), Result.FailReason, ERaidEntryFailReason::ServerListFailed);

	const FString ASuccess = TEXT("{\"result\":\"success\",\"slot\":\"A\",\"gameEndpoint\":\"127.0.0.1:7777\",\"token\":\"token-a\",\"currentPlayers\":0,\"maxPlayers\":16,\"reason\":\"\"}");
	int32 StaleCompletionCount = 0;
	int32 CurrentCompletionCount = 0;
	Assignment->SetResponsesForTest({FRaidAssignmentHttpTestResponse{true, 201, ASuccess, true}});
	Assignment->ResolveRaidAssignmentAsync(TEXT("A"), 10.0, FRaidAssignmentComplete::CreateLambda(
		[&StaleCompletionCount](const FRaidAssignmentResult&) { ++StaleCompletionCount; }));
	const uint64 StaleGeneration = Assignment->GetRequestGenerationForTest();
	Assignment->SetResponsesForTest({FRaidAssignmentHttpTestResponse{true, 201, ASuccess, true}});
	Assignment->ResolveRaidAssignmentAsync(TEXT("A"), 10.0, FRaidAssignmentComplete::CreateLambda(
		[&CurrentCompletionCount](const FRaidAssignmentResult&) { ++CurrentCompletionCount; }));
	const uint64 CurrentGeneration = Assignment->GetRequestGenerationForTest();

	Assignment->DeliverResponseForTest(StaleGeneration, true, 201, ASuccess);
	TestEqual(TEXT("stale remote response does not consume the current callback"), CurrentCompletionCount, 0);
	TestEqual(TEXT("superseded callback remains canceled"), StaleCompletionCount, 0);
	Assignment->DeliverResponseForTest(CurrentGeneration, true, 201, ASuccess);
	TestEqual(TEXT("current remote response completes once"), CurrentCompletionCount, 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRaidRemoteSessionTokenizedTravelTest,
	"DroneProto.RaidEntry.Session.RemoteTokenizedTravel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRaidRemoteSessionTokenizedTravelTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	URaidSessionSubsystem* Session = NewObject<URaidSessionSubsystem>(GameInstance);
	URemoteRaidAssignment* Assignment = NewObject<URemoteRaidAssignment>(Session);
	TestNotNull(TEXT("remote session game instance is created"), GameInstance);
	TestNotNull(TEXT("remote session is created"), Session);
	TestNotNull(TEXT("remote assignment is created for session"), Assignment);
	if (!GameInstance || !Session || !Assignment)
	{
		return false;
	}

	Assignment->SetServersForTest({
		FRaidServerDefinition{TEXT("A"), 0, TEXT("http://127.0.0.1:7787/raid/reservations")},
	});
	Assignment->SetResponsesForTest({
		FRaidAssignmentHttpTestResponse{
			true,
			201,
			TEXT("{\"result\":\"success\",\"slot\":\"A\",\"gameEndpoint\":\"127.0.0.1:7777\",\"token\":\"token-a\",\"currentPlayers\":0,\"maxPlayers\":16,\"reason\":\"\"}")},
	});
	Session->SetAssignmentForTest(Assignment);
	Session->SetSuppressTravelForTest(true);

	Session->RequestRaidEntry(TEXT("A"));
	TestTrue(TEXT("remote reservation requests travel"), Session->WasTravelRequestedForTest());
	TestEqual(TEXT("remote reservation requests travel exactly once"), Session->GetTravelRequestCountForTest(), 1);
	TestEqual(TEXT("travel includes slot and single-use token"),
		Session->GetLastTravelTargetForTest(),
		FString(TEXT("127.0.0.1:7777?RaidSlot=A?RaidReservation=token-a")));

	return true;
}

#endif
