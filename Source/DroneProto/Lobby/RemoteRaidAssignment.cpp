#include "RemoteRaidAssignment.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

void URemoteRaidAssignment::InitializeFromSettings()
{
	Servers = GetDefault<URaidServerDirectorySettings>()->Servers;
	SortServers();
}

FRaidAssignmentResult URemoteRaidAssignment::ResolveRaidAssignment(const FString& RequestedSlot)
{
	return FRaidAssignmentResult::Failed(
		ERaidEntryFailReason::ServerListFailed,
		FString::Printf(TEXT("RemoteAssignmentRequiresAsync RequestedSlot=%s"), *RequestedSlot));
}

void URemoteRaidAssignment::ResolveRaidAssignmentAsync(
	const FString& RequestedSlot,
	double RemainingSeconds,
	FRaidAssignmentComplete OnComplete)
{
	PendingRequestedSlot = RequestedSlot;
	Completion = MoveTemp(OnComplete);
	const uint64 Generation = ++RequestGeneration;
	NextServerIndex = 0;
	bSawValidServerResponse = false;
	RequestTimeoutSeconds = FMath::Clamp(RemainingSeconds, 0.1, 1.0);
#if WITH_DEV_AUTOMATION_TESTS
	NextTestResponseIndex = 0;
#endif

	if (Servers.IsEmpty())
	{
		Finish(FRaidAssignmentResult::Failed(
			ERaidEntryFailReason::ServerListFailed,
			TEXT("NoConfiguredRaidServers")));
		return;
	}

	StartNextCandidate(Generation);
}

bool URemoteRaidAssignment::IsSlotEnabled(const FString& SlotId) const
{
	return Servers.ContainsByPredicate([&SlotId](const FRaidServerDefinition& Server)
	{
		return Server.SlotId.Equals(SlotId, ESearchCase::IgnoreCase)
			&& !Server.ReservationUrl.TrimStartAndEnd().IsEmpty();
	});
}

#if WITH_DEV_AUTOMATION_TESTS
void URemoteRaidAssignment::SetServersForTest(const TArray<FRaidServerDefinition>& InServers)
{
	Servers = InServers;
	SortServers();
}

void URemoteRaidAssignment::SetResponsesForTest(const TArray<FRaidAssignmentHttpTestResponse>& InResponses)
{
	bUseTestResponses = true;
	TestResponses = InResponses;
	NextTestResponseIndex = 0;
}

void URemoteRaidAssignment::DeliverResponseForTest(
	uint64 Generation,
	bool bTransportSucceeded,
	int32 ResponseCode,
	const FString& Body)
{
	HandleCandidateResponse(Generation, bTransportSucceeded, ResponseCode, Body);
}
#endif

void URemoteRaidAssignment::SortServers()
{
	Servers.Sort([](const FRaidServerDefinition& Left, const FRaidServerDefinition& Right)
	{
		return Left.Priority < Right.Priority;
	});
}

void URemoteRaidAssignment::StartNextCandidate(uint64 Generation)
{
	if (Generation != RequestGeneration || !Completion.IsBound())
	{
		return;
	}
	if (!Servers.IsValidIndex(NextServerIndex))
	{
		Finish(bSawValidServerResponse
			? FRaidAssignmentResult::Waiting(
				ERaidEntryFailReason::NoServerAvailable,
				FString::Printf(TEXT("AllRaidServersFull RequestedSlot=%s"), *PendingRequestedSlot))
			: FRaidAssignmentResult::Failed(
				ERaidEntryFailReason::ServerListFailed,
				FString::Printf(TEXT("AllRaidServerRequestsFailed RequestedSlot=%s"), *PendingRequestedSlot)));
		return;
	}

	const FRaidServerDefinition Server = Servers[NextServerIndex++];
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseTestResponses)
	{
		if (!TestResponses.IsValidIndex(NextTestResponseIndex))
		{
			HandleCandidateResponse(Generation, false, 0, TEXT(""));
			return;
		}
		const FRaidAssignmentHttpTestResponse Response = TestResponses[NextTestResponseIndex++];
		if (!Response.bDeferred)
		{
			HandleCandidateResponse(Generation, Response.bTransportSucceeded, Response.ResponseCode, Response.Body);
		}
		return;
	}
#endif

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Server.ReservationUrl);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->SetTimeout(static_cast<float>(RequestTimeoutSeconds));
	Request->OnProcessRequestComplete().BindWeakLambda(this,
		[this, Generation](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
		{
			HandleCandidateResponse(
				Generation,
				bSucceeded && HttpResponse.IsValid(),
				HttpResponse.IsValid() ? HttpResponse->GetResponseCode() : 0,
				HttpResponse.IsValid() ? HttpResponse->GetContentAsString() : FString());
		});
	if (!Request->ProcessRequest())
	{
		HandleCandidateResponse(Generation, false, 0, TEXT(""));
	}
}

void URemoteRaidAssignment::HandleCandidateResponse(
	uint64 Generation,
	bool bTransportSucceeded,
	int32 ResponseCode,
	const FString& Body)
{
	if (Generation != RequestGeneration || !Completion.IsBound())
	{
		return;
	}
	if (bTransportSucceeded && ResponseCode == 201)
	{
		TSharedPtr<FJsonObject> Json;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
		if (FJsonSerializer::Deserialize(Reader, Json) && Json.IsValid())
		{
			FString ResultText;
			FString Slot;
			FString Endpoint;
			FString Token;
			if (Json->TryGetStringField(TEXT("result"), ResultText)
				&& ResultText == TEXT("success")
				&& Json->TryGetStringField(TEXT("slot"), Slot)
				&& Json->TryGetStringField(TEXT("gameEndpoint"), Endpoint)
				&& Json->TryGetStringField(TEXT("token"), Token)
				&& !Slot.IsEmpty() && !Endpoint.IsEmpty() && !Token.IsEmpty())
			{
				FRaidServerCandidate Candidate;
				Candidate.Endpoint.SlotId = Slot;
				Candidate.Endpoint.TravelTarget = Endpoint;
				Candidate.Endpoint.bIsLevelName = false;
				Candidate.Availability.SlotId = Slot;
				Candidate.Availability.ServerState = ERaidServerState::Online;
				Candidate.Availability.MaxPlayers = 16;
				double CurrentPlayers = 0.0;
				Json->TryGetNumberField(TEXT("currentPlayers"), CurrentPlayers);
				Candidate.Availability.CurrentPlayers = static_cast<int32>(CurrentPlayers);
				Candidate.Availability.bAcceptsPlayers = true;
				Candidate.Availability.DebugReason = TEXT("DedicatedReservation");
				Finish(FRaidAssignmentResult::Success(Candidate, TEXT("DedicatedReservationSelected"), Token));
				return;
			}
		}
	}
	else if (bTransportSucceeded && ResponseCode == 409)
	{
		bSawValidServerResponse = true;
	}

	StartNextCandidate(Generation);
}

void URemoteRaidAssignment::Finish(const FRaidAssignmentResult& Result)
{
	FRaidAssignmentComplete Callback = MoveTemp(Completion);
	Completion.Unbind();
	Callback.ExecuteIfBound(Result);
}
