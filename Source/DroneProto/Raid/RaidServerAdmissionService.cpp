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
	Ledger = MakeUnique<FRaidReservationLedger>(16, 10.0);

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

int32 URaidServerAdmissionService::GetActivePlayers() const
{
	return Ledger ? Ledger->GetActivePlayers() : 0;
}

#if WITH_DEV_AUTOMATION_TESTS
void URaidServerAdmissionService::InitializeForTest(const FString& InSlotId)
{
	Shutdown();
	SlotId = InSlotId;
	Ledger = MakeUnique<FRaidReservationLedger>(16, 10.0);
}

bool URaidServerAdmissionService::IssueReservationForTest(double NowSeconds, FString& OutToken)
{
	return Ledger && Ledger->TryReserve(NowSeconds, OutToken);
}
#endif

bool URaidServerAdmissionService::HandleReservationRequest(
	const FHttpServerRequest& Request,
	const FHttpResultCallback& OnComplete)
{
	ARaidGameMode* RaidGameMode = GameMode.Get();
	if (!RaidGameMode || !Ledger)
	{
		OnComplete(MakeJsonResponse(
			EHttpServerResponseCodes::ServerError,
			TEXT("error"), SlotId, GameEndpoint, TEXT(""), GetActivePlayers(), TEXT("RaidAdmissionUnavailable")));
		return true;
	}

	FName RejectReason;
	if (!RaidGameMode->CanAcceptRaidJoinForServer(RejectReason, false))
	{
		OnComplete(MakeJsonResponse(
			EHttpServerResponseCodes::Conflict,
			TEXT("unavailable"), SlotId, GameEndpoint, TEXT(""), GetActivePlayers(), RejectReason.ToString()));
		return true;
	}

	FString Token;
	if (!Ledger->TryReserve(FPlatformTime::Seconds(), Token))
	{
		OnComplete(MakeJsonResponse(
			EHttpServerResponseCodes::Conflict,
			TEXT("full"), SlotId, GameEndpoint, TEXT(""), GetActivePlayers(), TEXT("Full")));
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
