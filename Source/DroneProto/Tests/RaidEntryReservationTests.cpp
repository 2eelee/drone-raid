#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Lobby/LocalAssignment.h"
#include "Lobby/RaidServerDirectorySettings.h"
#include "Lobby/RemoteRaidAssignment.h"
#include "Lobby/RaidSessionSubsystem.h"
#include "Raid/RaidReservationLedger.h"
#include "Raid/RaidServerAdmissionService.h"

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
	FRaidReservationPendingLifetimeCoversClientColdStartTest,
	"DroneProto.RaidEntry.Reservation.PendingLifetimeCoversClientColdStart",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRaidReservationPendingLifetimeCoversClientColdStartTest::RunTest(const FString& Parameters)
{
	// 2026-08-20 Dedicated Server 실환경에서 예약 발급 -> PreLogin 도달까지
	// 정상 5.2초, 메모리 압박 시 13.4초가 걸렸다. 기본 pending 수명은 그 최악값을
	// 덮어야 한다 — 10초였을 때 정상 플레이어가 만료로 거부됐다.
	FRaidReservationLedger Ledger(1);

	FString Token;
	TestTrue(TEXT("reservation succeeds"), Ledger.TryReserve(100.0, Token));

	// 실측 최악값(13.4초) 시점에는 아직 살아 있어야 한다.
	Ledger.Expire(113.4);
	TestEqual(TEXT("pending reservation survives a 13.4s client cold start"),
		Ledger.GetReservedPlayers(113.4), 1);
	TestTrue(TEXT("claim still succeeds after the measured worst-case cold start"),
		Ledger.TryClaim(Token, 113.4));

	// 접속하지 않는 예약은 여전히 회수돼야 한다(정원이 영구히 묶이면 안 된다).
	FRaidReservationLedger AbandonedLedger(1);
	FString AbandonedToken;
	TestTrue(TEXT("abandoned reservation succeeds"),
		AbandonedLedger.TryReserve(200.0, AbandonedToken));
	AbandonedLedger.Expire(230.01);
	TestEqual(TEXT("abandoned reservation is reclaimed after the pending lifetime"),
		AbandonedLedger.GetReservedPlayers(230.01), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRaidReservationEntryDisconnectReleaseTest,
	"DroneProto.RaidEntry.Reservation.EntryDisconnectReleasesReservation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRaidReservationEntryDisconnectReleaseTest::RunTest(const FString& Parameters)
{
	// ENTRY-15: PreLogin claim과 PostLogin commit 사이는 클라이언트 맵 로드 구간이라
	// 서버에 PlayerController가 없고 Logout이 불리지 않는다. 그 구간에서 연결이 끊기면
	// 예약을 명시적으로 반납해야 정원이 새지 않는다.
	FRaidReservationLedger Ledger(1, 10.0, 120.0);
	FString Token;
	TestTrue(TEXT("reservation for the entry window succeeds"), Ledger.TryReserve(100.0, Token));
	TestTrue(TEXT("entry claim succeeds"), Ledger.TryClaim(Token, 101.0));

	// claim 수명은 pending 수명과 분리돼야 한다. 10초는 맵 로드를 덮지 못해
	// 정상 입장 중인 플레이어의 예약이 먼저 만료되고 PostLogin commit이 실패한다.
	TestEqual(TEXT("claimed reservation outlives the pending lifetime"), Ledger.GetReservedPlayers(160.0), 1);

	TestTrue(TEXT("abandoned claim is released"), Ledger.ReleaseReservation(Token));
	TestEqual(TEXT("release frees the reserved slot"), Ledger.GetReservedPlayers(161.0), 0);
	TestFalse(TEXT("releasing the same token twice is rejected"), Ledger.ReleaseReservation(Token));
	TestFalse(TEXT("releasing an unknown token is rejected"), Ledger.ReleaseReservation(TEXT("no-such-token")));

	FString NextToken;
	TestTrue(TEXT("released capacity accepts the next reservation"), Ledger.TryReserve(162.0, NextToken));

	URaidServerAdmissionService* Admission = NewObject<URaidServerAdmissionService>();
	TestNotNull(TEXT("admission service is created"), Admission);
	if (!Admission)
	{
		return false;
	}
	Admission->InitializeForTest(TEXT("A"));

	const double NowSeconds = 200.0;
	FString LiveToken;
	FString LostToken;
	FString ErrorMessage;
	TestTrue(TEXT("live reservation is issued"), Admission->IssueReservationForTest(NowSeconds, LiveToken));
	TestTrue(TEXT("lost reservation is issued"), Admission->IssueReservationForTest(NowSeconds, LostToken));
	TestTrue(TEXT("live reservation is claimed"), Admission->TryClaim(TEXT("A"), LiveToken, NowSeconds, ErrorMessage));
	TestTrue(TEXT("lost reservation is claimed"), Admission->TryClaim(TEXT("A"), LostToken, NowSeconds, ErrorMessage));

	// 살아 있는 연결이 들고 있는 토큰은 남기고 나머지만 반납한다.
	TSet<FString> LiveTokens;
	LiveTokens.Add(LiveToken);
	TestEqual(TEXT("only the abandoned claim is released"), Admission->ReleaseAbandonedClaims(LiveTokens), 1);
	TestEqual(TEXT("sweeping again releases nothing"), Admission->ReleaseAbandonedClaims(LiveTokens), 0);
	TestTrue(TEXT("surviving claim still commits after the sweep"), Admission->TryCommitClaimedForTest(LiveToken));

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRaidEntryNormalStageOrderTest,
	"DroneProto.RaidEntry.Session.NormalEntryStageOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRaidEntryNormalStageOrderTest::RunTest(const FString& Parameters)
{
	// ENTRY-19(원문 :156-158): 정상 입장은 서버 입장 성공 → 맵 로드 시작 → 맵 로드 완료 → 플레이어 생성
	// 순서로만 진행한다. 각 단계의 구현은 흩어져 있고 순서 자체를 고정하는 계약이 없었다.
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	URaidSessionSubsystem* Session = NewObject<URaidSessionSubsystem>(GameInstance);
	URemoteRaidAssignment* Assignment = NewObject<URemoteRaidAssignment>(Session);
	TestNotNull(TEXT("stage order session is created"), Session);
	TestNotNull(TEXT("stage order assignment is created"), Assignment);
	if (!GameInstance || !Session || !Assignment)
	{
		return false;
	}

	Assignment->SetServersForTest({
		FRaidServerDefinition{TEXT("A"), 0, TEXT("http://127.0.0.1:7787/raid/reservations")},
	});
	Session->SetAssignmentForTest(Assignment);
	Session->SetSuppressTravelForTest(true);

	// 1단계 경계: 서버 입장이 성공하지 않으면 맵 로드 단계로 넘어가지 않는다.
	Assignment->SetResponsesForTest({FRaidAssignmentHttpTestResponse{false, 0, TEXT("")}});
	Session->RequestRaidEntry(TEXT("A"));
	TestFalse(TEXT("failed admission does not start map load"), Session->WasTravelRequestedForTest());
	TestFalse(TEXT("failed admission does not arm the load watchdog"), Session->IsRaidLoadWatchdogActiveForTest());

	// 2단계 경계: 입장이 승인된 뒤에야 맵 로드가 시작되고 그 시작과 함께 감시가 켜진다.
	Session->ResetTravelRequestedForTest();
	Assignment->SetResponsesForTest({
		FRaidAssignmentHttpTestResponse{
			true,
			201,
			TEXT("{\"result\":\"success\",\"slot\":\"A\",\"gameEndpoint\":\"127.0.0.1:7777\",\"token\":\"token-a\",\"currentPlayers\":0,\"maxPlayers\":16,\"reason\":\"\"}")},
	});
	Session->RequestRaidEntry(TEXT("A"));
	TestTrue(TEXT("granted admission starts map load"), Session->WasTravelRequestedForTest());
	TestEqual(TEXT("granted admission starts map load exactly once"), Session->GetTravelRequestCountForTest(), 1);
	TestTrue(TEXT("map load start arms the watchdog"), Session->IsRaidLoadWatchdogActiveForTest());

	// 3단계 경계: 로드 완료 통지 전에는 감시가 유지되고, 완료가 감시를 해제한다.
	// 감시가 살아 있는 동안은 아직 플레이어 생성 단계가 아니다.
	TestEqual(TEXT("map load in flight reports no failure"), Session->GetRaidLoadFailureHandleCountForTest(), 0);
	Session->CompleteRaidLoadForTest();
	TestFalse(TEXT("map load completion disarms the watchdog"), Session->IsRaidLoadWatchdogActiveForTest());
	TestEqual(TEXT("normal stage order never reports a load failure"), Session->GetRaidLoadFailureHandleCountForTest(), 0);
	TestEqual(TEXT("completed load does not re-request travel"), Session->GetTravelRequestCountForTest(), 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRaidReservationRejectedRequestLogsReasonTest,
	"DroneProto.RaidEntry.Reservation.RejectedRequestLogsReason",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRaidReservationRejectedRequestLogsReasonTest::RunTest(const FString& Parameters)
{
	// 계획서 T2. 외부 테스터는 전체 로그가 아니라 srv_<slot>.log의 DR_SUMMARY 줄만 보낸다.
	// 거부가 왜 났는지 그 한 줄로 판정되지 않으면 Wave 1 보고를 재현 없이 분류할 수 없다.
	//
	// UE_LOG 자체는 HandleReservationRequest 안에 있고 그 함수는 FHttpServerRequest를 요구해
	// 자동화에서 호출할 수 없다. 그래서 두 축을 나눠 고정한다.
	//   (1) 사유 값 — TryIssueReservationForServer가 실제로 내주는 FName
	//   (2) 로그·응답 문자열 — 소스 마커(D5.ManualSummaryLogs.SourceMarkers와 같은 방식)
	// 닫힌 레이드 사유 4종(BossDead/BossClear/TimeOver/Full)은 IssueGateBlocksClosedRaid가,
	// RaidEnded는 EndStateRejectsEntry가 덮는다. 여기서는 남은 RaidAdmissionUnavailable을 본다.

	// GameMode가 붙지 않은 서비스는 게이트를 판정할 근거가 없다. 이때 토큰을 내주면
	// 정원·보스 상태를 아무도 확인하지 않은 채 입장이 열린다 — 열어 주는 대신 거부해야 한다.
	URaidServerAdmissionService* Unbound = NewObject<URaidServerAdmissionService>();
	TestNotNull(TEXT("unbound admission service is created"), Unbound);
	if (!Unbound)
	{
		return false;
	}
	Unbound->InitializeForTest(TEXT("A"));

	FString UnboundToken;
	FName RejectReason;
	TestFalse(TEXT("admission without a game mode refuses to issue"),
		Unbound->TryIssueReservationForServer(FPlatformTime::Seconds(), UnboundToken, RejectReason));
	TestEqual(TEXT("unbound reject reason is RaidAdmissionUnavailable"),
		RejectReason, FName(TEXT("RaidAdmissionUnavailable")));
	TestTrue(TEXT("unbound rejection returns no token"), UnboundToken.IsEmpty());

	// 실패 경로가 아무 사유도 남기지 않으면 로그에 Reason= 이 비어 나간다.
	TestFalse(TEXT("every rejection carries a reason"), RejectReason.IsNone());

	const FString SourceRoot = FPaths::ProjectDir() / TEXT("Source/DroneProto");
	FString AdmissionSource;
	const FString AdmissionPath = SourceRoot / TEXT("Raid/RaidServerAdmissionService.cpp");
	TestTrue(TEXT("admission service source loads"),
		FFileHelper::LoadFileToString(AdmissionSource, *AdmissionPath));
	if (AdmissionSource.IsEmpty())
	{
		return false;
	}

	// 거부 로그는 슬롯·사유·현재 인원 세 값을 모두 들고 있어야 한다.
	// 슬롯이 없으면 어느 프로세스인지, 인원이 없으면 Full 여부를 로그만으로 못 가른다.
	TestTrue(TEXT("reservation rejection summary log marker exists"),
		AdmissionSource.Contains(TEXT("[DR_SUMMARY] ReservationRejected Slot=%s Reason=%s CurrentPlayers=%d")));
	TestTrue(TEXT("reservation success summary log marker exists"),
		AdmissionSource.Contains(TEXT("[DR_SUMMARY] RaidReservationIssued Slot=%s CurrentPlayers=%d MaxPlayers=16")));

	// HTTP 응답도 같은 사유를 싣는다. 로그를 못 받는 상황에서 테스터 화면 쪽 단서가 이것뿐이다.
	TestTrue(TEXT("rejection response carries the reject reason"),
		AdmissionSource.Contains(TEXT("RejectReason.ToString()")));
	TestTrue(TEXT("unavailable rejection answers with a server error code"),
		AdmissionSource.Contains(TEXT("EHttpServerResponseCodes::ServerError")));
	TestTrue(TEXT("closed-raid rejection answers with a conflict code"),
		AdmissionSource.Contains(TEXT("EHttpServerResponseCodes::Conflict")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRaidServerDirectorySettingsAreLoadedFromConfigTest,
	"DroneProto.RaidEntry.ServerDirectory.SettingsAreLoadedFromConfig",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRaidServerDirectorySettingsAreLoadedFromConfigTest::RunTest(const FString& Parameters)
{
	// 계획서 T5(4-2절). 기존 원격 배정 테스트는 전부 SetServersForTest로 목록을 주입해
	// ini -> URaidServerDirectorySettings -> InitializeFromSettings 경로를 한 번도 타지 않는다.
	// 2026-08-20 실환경 검증도 커맨드라인 -RaidReservationPort=로 포트를 직접 줘서 같은 구멍이 남았다
	// (ini는 7787/7788/7789, 검증은 8777/8778/8779). 즉 클라이언트가 서버 목록을 얻는 실제 경로는
	// 지금까지 아무도 확인한 적이 없다. Servers가 비면 NoConfiguredRaidServers로 즉사한다.
	const URaidServerDirectorySettings* Settings = GetDefault<URaidServerDirectorySettings>();
	TestNotNull(TEXT("server directory settings CDO exists"), Settings);
	if (!Settings)
	{
		return false;
	}

	TestEqual(TEXT("config declares three raid servers"), Settings->Servers.Num(), 3);
	if (Settings->Servers.Num() != 3)
	{
		return false;
	}

	const TCHAR* ExpectedSlots[] = {TEXT("A"), TEXT("B"), TEXT("C")};
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const FRaidServerDefinition& Server = Settings->Servers[Index];
		TestEqual(FString::Printf(TEXT("config slot %d id"), Index), Server.SlotId, FString(ExpectedSlots[Index]));
		TestEqual(FString::Printf(TEXT("config slot %d priority"), Index), Server.Priority, Index);
		// URL이 비면 IsSlotEnabled가 그 슬롯을 조용히 떨어뜨려 후보에서 사라진다.
		TestFalse(FString::Printf(TEXT("config slot %d has a reservation url"), Index),
			Server.ReservationUrl.TrimStartAndEnd().IsEmpty());
		TestTrue(FString::Printf(TEXT("config slot %d url targets the reservation endpoint"), Index),
			Server.ReservationUrl.Contains(TEXT("/raid/reservations")));
	}

	URemoteRaidAssignment* Assignment = NewObject<URemoteRaidAssignment>();
	TestNotNull(TEXT("config-driven assignment is created"), Assignment);
	if (!Assignment)
	{
		return false;
	}

	// 주입 없이 설정만으로 후보가 채워져야 한다.
	Assignment->InitializeFromSettings();
	TestEqual(TEXT("settings populate the candidate list"), Assignment->GetServersForTest().Num(), 3);
	TestTrue(TEXT("slot A is enabled from config"), Assignment->IsSlotEnabled(TEXT("A")));
	TestTrue(TEXT("slot B is enabled from config"), Assignment->IsSlotEnabled(TEXT("B")));
	TestTrue(TEXT("slot C is enabled from config"), Assignment->IsSlotEnabled(TEXT("C")));
	TestFalse(TEXT("an unconfigured slot stays disabled"), Assignment->IsSlotEnabled(TEXT("D")));

	// 후보 순서는 Priority 오름차순이다. ini가 이미 정렬돼 있어도 그 순서에 기대지 않는다.
	Assignment->SetServersForTest({
		FRaidServerDefinition{TEXT("C"), 2, TEXT("http://127.0.0.1:7789/raid/reservations")},
		FRaidServerDefinition{TEXT("A"), 0, TEXT("http://127.0.0.1:7787/raid/reservations")},
		FRaidServerDefinition{TEXT("B"), 1, TEXT("http://127.0.0.1:7788/raid/reservations")},
	});
	const TArray<FRaidServerDefinition>& Sorted = Assignment->GetServersForTest();
	TestEqual(TEXT("shuffled input keeps every candidate"), Sorted.Num(), 3);
	if (Sorted.Num() == 3)
	{
		TestEqual(TEXT("priority 0 is tried first"), Sorted[0].SlotId, FString(TEXT("A")));
		TestEqual(TEXT("priority 1 is tried second"), Sorted[1].SlotId, FString(TEXT("B")));
		TestEqual(TEXT("priority 2 is tried last"), Sorted[2].SlotId, FString(TEXT("C")));
	}

	// URL이 빈 슬롯은 설정에 남아 있어도 후보로 쓰지 않는다.
	Assignment->SetServersForTest({
		FRaidServerDefinition{TEXT("A"), 0, TEXT("   ")},
	});
	TestFalse(TEXT("a blank reservation url disables the slot"), Assignment->IsSlotEnabled(TEXT("A")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRaidLocalAssignmentDevelopmentSwitchTest,
	"DroneProto.RaidEntry.Assignment.LocalDevelopmentSwitch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRaidLocalAssignmentDevelopmentSwitchTest::RunTest(const FString& Parameters)
{
	// 예약 서비스는 NM_DedicatedServer에서만 뜨므로 단일 PIE의 로비 입장은 항상 NoServerAvailable로 막힌다.
	// 이 스위치는 그 구간만 우회한다. 기본은 꺼짐이어야 하고, Shipping에서는 켜도 무시돼야 한다.
	URaidServerDirectorySettings* Settings = GetMutableDefault<URaidServerDirectorySettings>();
	TestNotNull(TEXT("raid server directory settings are available"), Settings);
	if (!Settings)
	{
		return false;
	}

	const bool bOriginal = Settings->bUseLocalAssignment;

	Settings->bUseLocalAssignment = false;
	TestFalse(
		TEXT("local assignment stays off by default"),
		URaidSessionSubsystem::ShouldUseLocalAssignment());

	Settings->bUseLocalAssignment = true;
	TestTrue(
		TEXT("enabling the development setting selects local assignment"),
		URaidSessionSubsystem::ShouldUseLocalAssignment());

	Settings->bUseLocalAssignment = bOriginal;

	// Shipping 가드는 컴파일 타임 분기라 런타임으로 관측할 수 없다. 소스에 가드가 남아 있는지로 확인한다.
	FString SubsystemSource;
	const bool bSourceLoaded = FFileHelper::LoadFileToString(
		SubsystemSource,
		*(FPaths::ProjectDir() / TEXT("Source/DroneProto/Lobby/RaidSessionSubsystem.cpp")));
	TestTrue(TEXT("raid session subsystem source loads"), bSourceLoaded);
	if (bSourceLoaded)
	{
		TestTrue(
			TEXT("shipping builds are guarded from the local assignment switch"),
			SubsystemSource.Contains(TEXT("UE_BUILD_SHIPPING")));
	}

	// 로컬 후보는 로비가 실제로 이동할 수 있는 대상을 줘야 한다.
	ULocalAssignment* LocalAssignment = NewObject<ULocalAssignment>();
	TestNotNull(TEXT("local assignment is constructible"), LocalAssignment);
	if (LocalAssignment)
	{
		const FRaidAssignmentResult Assigned = LocalAssignment->ResolveRaidAssignment(FString());
		TestEqual(
			TEXT("local assignment resolves a candidate without a reservation server"),
			static_cast<int32>(Assigned.Result),
			static_cast<int32>(ERaidAssignmentResultType::Success));
		TestEqual(
			TEXT("local candidate travels to the raid map"),
			Assigned.Endpoint.TravelTarget,
			FString(TEXT("TestMap")));
	}

	return true;
}

#endif
