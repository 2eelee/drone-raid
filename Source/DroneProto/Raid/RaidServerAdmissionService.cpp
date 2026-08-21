#include "RaidServerAdmissionService.h"

#include "RaidGameMode.h"
#include "HttpPath.h"
#include "HttpResultCallback.h"
#include "HttpServerConstants.h"
#include "HttpServerModule.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "IHttpRouter.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
constexpr int32 RaidReservationMaxPlayers = 16;
// 예약만 받고 접속하지 않는 클라이언트를 회수하는 시간.
// 2026-08-20 실환경 검증에서 10초는 부족했다 — 패키징 클라이언트가 예약 발급 후
// PreLogin에 도달하기까지 정상 5.2초, 메모리 압박 시 13.4초가 걸려 정상 플레이어가
// 만료로 거부됐다. 느린 환경까지 덮도록 30초로 둔다(정원이 묶이는 최악 시간도 30초).
constexpr double RaidReservationPendingLifetimeSeconds = 30.0;
// PreLogin 통과 후 클라이언트가 맵을 로드하는 동안 예약을 붙잡아 두는 시간. 연결이 실제로 끊기면
// ReleaseAbandonedClaims가 즉시 회수하므로 이 값은 알림이 오지 않는 경우의 backstop이다.
constexpr double RaidReservationClaimedLifetimeSeconds = 120.0;

TUniquePtr<FHttpServerResponse> MakeJsonResponse(
	EHttpServerResponseCodes Code,
	const FString& Result,
	const FString& SlotId,
	const FString& GameEndpoint,
	const FString& Token,
	int32 CurrentPlayers,
	const FString& Reason)
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("result"), Result);
	Json->SetStringField(TEXT("slot"), SlotId);
	Json->SetStringField(TEXT("gameEndpoint"), GameEndpoint);
	Json->SetStringField(TEXT("token"), Token);
	Json->SetNumberField(TEXT("currentPlayers"), CurrentPlayers);
	Json->SetNumberField(TEXT("maxPlayers"), 16);
	Json->SetStringField(TEXT("reason"), Reason);

	FString Body;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
	FJsonSerializer::Serialize(Json, Writer);
	TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(Body, TEXT("application/json"));
	Response->Code = Code;
	return Response;
}
}

bool URaidServerAdmissionService::Initialize(
	ARaidGameMode* InGameMode,
	const FString& InSlotId,
	uint32 InPort,
	const FString& InGameEndpoint)
{
	Shutdown();
	if (!InGameMode || InSlotId.TrimStartAndEnd().IsEmpty() || InPort == 0 || InGameEndpoint.TrimStartAndEnd().IsEmpty())
	{
		return false;
	}

	GameMode = InGameMode;
	SlotId = InSlotId.TrimStartAndEnd();
	GameEndpoint = InGameEndpoint.TrimStartAndEnd();
	Ledger = MakeUnique<FRaidReservationLedger>(
		RaidReservationMaxPlayers,
		RaidReservationPendingLifetimeSeconds,
		RaidReservationClaimedLifetimeSeconds);

	Router = FHttpServerModule::Get().GetHttpRouter(InPort, true);
	if (!Router.IsValid())
	{
		Shutdown();
		return false;
	}

	ReservationRoute = Router->BindRoute(
		FHttpPath(TEXT("/raid/reservations")),
		EHttpServerRequestVerbs::VERB_POST,
		FHttpRequestHandler::CreateUObject(this, &URaidServerAdmissionService::HandleReservationRequest));
	if (!ReservationRoute.IsValid())
	{
		Shutdown();
		return false;
	}

	FHttpServerModule::Get().StartAllListeners();
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] RaidAdmissionStarted Slot=%s ReservationPort=%u GameEndpoint=%s MaxPlayers=16"),
		*SlotId,
		InPort,
		*GameEndpoint);
	return true;
}

void URaidServerAdmissionService::Shutdown()
{
	if (Router.IsValid() && ReservationRoute.IsValid())
	{
		Router->UnbindRoute(ReservationRoute);
	}

	ReservationRoute.Reset();
	Router.Reset();
	PlayerTokens.Reset();
	AdmittedPlayers.Reset();
	Ledger.Reset();
	GameMode.Reset();
	SlotId.Reset();
	GameEndpoint.Reset();
}

void URaidServerAdmissionService::BeginDestroy()
{
	Shutdown();
	Super::BeginDestroy();
}

bool URaidServerAdmissionService::TryClaim(
	const FString& RequestedSlot,
	const FString& Token,
	double NowSeconds,
	FString& OutErrorMessage)
{
	OutErrorMessage.Reset();
	if (!Ledger)
	{
		OutErrorMessage = TEXT("RaidAdmissionUnavailable");
		return false;
	}
	if (RequestedSlot.IsEmpty() || Token.IsEmpty())
	{
		OutErrorMessage = TEXT("RaidReservationMissing");
		return false;
	}
	if (!RequestedSlot.Equals(SlotId, ESearchCase::IgnoreCase))
	{
		OutErrorMessage = TEXT("RaidReservationSlotMismatch");
		return false;
	}
	if (!Ledger->TryClaim(Token, NowSeconds))
	{
		OutErrorMessage = TEXT("RaidReservationInvalid");
		return false;
	}
	return true;
}

bool URaidServerAdmissionService::BindClaimedToken(APlayerController* PlayerController, const FString& Token)
{
	if (!PlayerController || Token.IsEmpty() || PlayerTokens.Contains(PlayerController))
	{
		return false;
	}
	PlayerTokens.Add(PlayerController, Token);
	return true;
}

bool URaidServerAdmissionService::CommitForPlayer(APlayerController* PlayerController)
{
	if (!Ledger || !PlayerController || AdmittedPlayers.Contains(PlayerController))
	{
		return false;
	}

	const FString* Token = PlayerTokens.Find(PlayerController);
	if (!Token || !Ledger->TryCommitClaimed(*Token))
	{
		return false;
	}

	PlayerTokens.Remove(PlayerController);
	AdmittedPlayers.Add(PlayerController);
	return true;
}

bool URaidServerAdmissionService::ReleasePlayer(AController* Controller)
{
	if (!Ledger || !Controller || AdmittedPlayers.Remove(Controller) == 0)
	{
		return false;
	}
	return Ledger->ReleaseActivePlayer();
}

int32 URaidServerAdmissionService::ReleaseAbandonedClaims(const TSet<FString>& LiveReservationTokens)
{
	// ENTRY-15: claim 상태 예약은 PreLogin을 통과했지만 아직 PostLogin commit 전인 입장 중 연결의 것이다.
	// 그 구간에는 PlayerController가 없어 Logout이 불리지 않으므로, 살아 있는 연결이 더 이상 들고 있지
	// 않은 claim은 여기서 반납해야 정원이 회수된다. commit을 마친 플레이어는 원장에서 이미 사라졌으므로
	// 이 쓸기의 대상이 아니다 — 그쪽 회수는 Logout의 ReleasePlayer가 맡는다.
	if (!Ledger)
	{
		return 0;
	}

	TArray<FString> ClaimedTokens;
	Ledger->GetClaimedTokens(ClaimedTokens);

	int32 ReleasedCount = 0;
	for (const FString& Token : ClaimedTokens)
	{
		if (LiveReservationTokens.Contains(Token))
		{
			continue;
		}
		if (!Ledger->ReleaseReservation(Token))
		{
			continue;
		}

		++ReleasedCount;
		for (auto It = PlayerTokens.CreateIterator(); It; ++It)
		{
			if (It.Value() == Token)
			{
				It.RemoveCurrent();
			}
		}
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] RaidReservationCanceled Slot=%s Scope=EntryDisconnect CurrentPlayers=%d MaxPlayers=16"),
			*SlotId,
			GetActivePlayers());
	}
	return ReleasedCount;
}

int32 URaidServerAdmissionService::GetActivePlayers() const
{
	return Ledger ? Ledger->GetActivePlayers() : 0;
}

#if WITH_DEV_AUTOMATION_TESTS
void URaidServerAdmissionService::InitializeForTest(const FString& InSlotId, ARaidGameMode* InGameMode)
{
	Shutdown();
	SlotId = InSlotId;
	GameMode = InGameMode;
	Ledger = MakeUnique<FRaidReservationLedger>(
		RaidReservationMaxPlayers,
		RaidReservationPendingLifetimeSeconds,
		RaidReservationClaimedLifetimeSeconds);
}

bool URaidServerAdmissionService::IssueReservationForTest(double NowSeconds, FString& OutToken)
{
	return Ledger && Ledger->TryReserve(NowSeconds, OutToken);
}


bool URaidServerAdmissionService::TryCommitClaimedForTest(const FString& Token)
{
	return Ledger && Ledger->TryCommitClaimed(Token);
}
#endif

bool URaidServerAdmissionService::TryIssueReservationForServer(
	double NowSeconds,
	FString& OutToken,
	FName& OutRejectReason)
{
	OutToken.Reset();
	OutRejectReason = NAME_None;

	ARaidGameMode* RaidGameMode = GameMode.Get();
	if (!RaidGameMode || !Ledger)
	{
		OutRejectReason = FName(TEXT("RaidAdmissionUnavailable"));
		return false;
	}

	// 입장 게이트가 정원보다 먼저다. 닫힌 레이드(보스 Dead/Clear·시간 종료·End)는
	// 자리가 남아 있어도 토큰을 내주지 않는다(ENTRY-13).
	if (!RaidGameMode->CanAcceptRaidJoinForServer(OutRejectReason, false))
	{
		return false;
	}

	if (!Ledger->TryReserve(NowSeconds, OutToken))
	{
		OutRejectReason = FName(TEXT("Full"));
		return false;
	}

	return true;
}

bool URaidServerAdmissionService::HandleReservationRequest(
	const FHttpServerRequest& Request,
	const FHttpResultCallback& OnComplete)
{
	FString Token;
	FName RejectReason;
	if (!TryIssueReservationForServer(FPlatformTime::Seconds(), Token, RejectReason))
	{
		// 거부 사유는 서버 로그에도 남긴다. 종전에는 HTTP 응답 JSON에만 있어
		// 운영자가 srv_<slot>.log만 보고는 왜 막혔는지 알 수 없었다(계획서 9-2 "부분").
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ReservationRejected Slot=%s Reason=%s CurrentPlayers=%d"),
			*SlotId,
			*RejectReason.ToString(),
			GetActivePlayers());

		const bool bUnavailable = RejectReason == FName(TEXT("RaidAdmissionUnavailable"));
		const bool bFull = RejectReason == FName(TEXT("Full"));
		OnComplete(MakeJsonResponse(
			bUnavailable ? EHttpServerResponseCodes::ServerError : EHttpServerResponseCodes::Conflict,
			bUnavailable ? TEXT("error") : (bFull ? TEXT("full") : TEXT("unavailable")),
			SlotId, GameEndpoint, TEXT(""), GetActivePlayers(), RejectReason.ToString()));
		return true;
	}

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] RaidReservationIssued Slot=%s CurrentPlayers=%d MaxPlayers=16"),
		*SlotId,
		GetActivePlayers());
	OnComplete(MakeJsonResponse(
		EHttpServerResponseCodes::Created,
		TEXT("success"), SlotId, GameEndpoint, Token, GetActivePlayers(), TEXT("")));
	return true;
}
