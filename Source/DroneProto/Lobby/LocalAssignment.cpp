#include "LocalAssignment.h"

namespace
{
FRaidServerCandidate MakeDefaultRaidCandidate(const FString& SlotId)
{
	FRaidServerCandidate Candidate;
	Candidate.Endpoint.SlotId = SlotId;
	Candidate.Endpoint.TravelTarget = TEXT("TestMap");
	Candidate.Endpoint.bIsLevelName = true;
	Candidate.CurrentPlayers = 0;
	Candidate.MaxPlayers = 16;
	Candidate.bIsOnline = true;
	Candidate.bAcceptsPlayers = true;
	Candidate.Availability.SlotId = SlotId;
	Candidate.Availability.CurrentPlayers = 0;
	Candidate.Availability.MaxPlayers = 16;
	Candidate.Availability.ServerState = ERaidServerState::Online;
	Candidate.Availability.bAcceptsPlayers = true;
	Candidate.Availability.DebugReason = TEXT("LocalPrototypeDefault");
	return Candidate;
}
}

ULocalAssignment::ULocalAssignment()
{
	ResetDefaultCandidates();
}

FRaidAssignmentResult ULocalAssignment::ResolveRaidAssignment(const FString& RequestedSlot)
{
	if (Candidates.Num() == 0)
	{
		return FRaidAssignmentResult::Failed(
			ERaidEntryFailReason::ServerListFailed,
			FString::Printf(TEXT("NoCandidates RequestedSlot=%s"), *RequestedSlot));
	}

	return SelectCandidateByPriority(RequestedSlot);
}

FRaidAssignmentResult ULocalAssignment::SelectCandidateByPriority(const FString& RequestedSlot) const
{
	for (const FRaidServerCandidate& Candidate : Candidates)
	{
		const FRaidServerCandidate EvaluatedCandidate = NormalizeCandidateAvailability(Candidate);
		LogServerAvailability(EvaluatedCandidate.Availability);

		if (!CanAttemptTravel(EvaluatedCandidate))
		{
			continue;
		}

		if (EvaluatedCandidate.Endpoint.TravelTarget.TrimStartAndEnd().IsEmpty())
		{
			return FRaidAssignmentResult::Failed(
				ERaidEntryFailReason::MapLoadFailed,
				FString::Printf(TEXT("InvalidTravelTarget RequestedSlot=%s CandidateSlot=%s"), *RequestedSlot, *EvaluatedCandidate.Endpoint.SlotId),
				EvaluatedCandidate.Endpoint,
				EvaluatedCandidate.Availability);
		}

		return FRaidAssignmentResult::Success(
			EvaluatedCandidate,
			FString::Printf(TEXT("Selected RequestedSlot=%s CandidateSlot=%s ServerState=%s"),
				*RequestedSlot,
				*EvaluatedCandidate.Endpoint.SlotId,
				ToRaidServerStateText(EvaluatedCandidate.Availability.ServerState)));
	}

	return FRaidAssignmentResult::Waiting(
		ERaidEntryFailReason::NoServerAvailable,
		FString::Printf(TEXT("NoAvailableCandidate RequestedSlot=%s"), *RequestedSlot));
}

bool ULocalAssignment::IsSlotEnabled(const FString& SlotId) const
{
	for (const FRaidServerCandidate& Candidate : Candidates)
	{
		if (Candidate.Endpoint.SlotId == SlotId)
		{
			const FRaidServerAvailability Availability = EvaluateAvailability(Candidate);
			return (Availability.ServerState == ERaidServerState::Online
				|| Availability.ServerState == ERaidServerState::Full)
				&& Availability.bAcceptsPlayers;
		}
	}

	return false;
}

FRaidServerCandidate ULocalAssignment::NormalizeCandidateAvailability(const FRaidServerCandidate& Candidate) const
{
	FRaidServerCandidate Normalized = Candidate;
	Normalized.Availability = EvaluateAvailability(Candidate);
	return Normalized;
}

FRaidServerAvailability ULocalAssignment::EvaluateAvailability(const FRaidServerCandidate& Candidate) const
{
	FRaidServerAvailability Availability = Candidate.Availability;
	if (Availability.SlotId.IsEmpty())
	{
		Availability.SlotId = Candidate.Endpoint.SlotId;
	}

	const bool bUsesLegacyAvailability =
		Availability.ServerState == ERaidServerState::Unknown
		&& Availability.CurrentPlayers == 0
		&& Availability.MaxPlayers == 16
		&& Availability.bAcceptsPlayers
		&& Availability.DebugReason.IsEmpty();

	if (bUsesLegacyAvailability)
	{
		Availability.CurrentPlayers = Candidate.CurrentPlayers;
		Availability.MaxPlayers = Candidate.MaxPlayers;
		Availability.bAcceptsPlayers = Candidate.bAcceptsPlayers;
	}

	if (Availability.MaxPlayers <= 0)
	{
		Availability.MaxPlayers = Candidate.MaxPlayers > 0 ? Candidate.MaxPlayers : 16;
	}

	if (Availability.ServerState == ERaidServerState::Unknown)
	{
		if (bUsesLegacyAvailability && !Candidate.bIsOnline)
		{
			Availability.ServerState = ERaidServerState::Offline;
		}
		else if (!Availability.bAcceptsPlayers)
		{
			Availability.ServerState = ERaidServerState::Unavailable;
		}
		else if (Availability.CurrentPlayers >= Availability.MaxPlayers)
		{
			Availability.ServerState = ERaidServerState::Full;
		}
		else
		{
			Availability.ServerState = ERaidServerState::Online;
		}
	}
	else if (Availability.ServerState == ERaidServerState::Online)
	{
		if (!Availability.bAcceptsPlayers)
		{
			Availability.ServerState = ERaidServerState::Unavailable;
		}
		else if (Availability.CurrentPlayers >= Availability.MaxPlayers)
		{
			Availability.ServerState = ERaidServerState::Full;
		}
	}

	if (Availability.DebugReason.IsEmpty())
	{
		Availability.DebugReason = FString::Printf(TEXT("LocalPrototypeAvailability Slot=%s State=%s"),
			*Availability.SlotId,
			ToRaidServerStateText(Availability.ServerState));
	}

	return Availability;
}

bool ULocalAssignment::CanAttemptTravel(const FRaidServerCandidate& Candidate) const
{
	const FRaidServerAvailability& Availability = Candidate.Availability;
	return Availability.ServerState == ERaidServerState::Online
		&& Availability.bAcceptsPlayers
		&& Availability.CurrentPlayers < Availability.MaxPlayers;
}

void ULocalAssignment::LogServerAvailability(const FRaidServerAvailability& Availability) const
{
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] RaidServerState Slot=%s State=%s CurrentPlayers=%d MaxPlayers=%d bAcceptsPlayers=%d AuthorityModel=LocalPrototype DebugReason=%s"),
		*Availability.SlotId,
		ToRaidServerStateText(Availability.ServerState),
		Availability.CurrentPlayers,
		Availability.MaxPlayers,
		Availability.bAcceptsPlayers ? 1 : 0,
		*Availability.DebugReason);
}

void ULocalAssignment::ResetDefaultCandidates()
{
	// Prototype-only local availability model. A server-authoritative backend should supply these candidates later.
	Candidates.Reset();
	Candidates.Add(MakeDefaultRaidCandidate(TEXT("A")));
	Candidates.Add(MakeDefaultRaidCandidate(TEXT("B")));
	Candidates.Add(MakeDefaultRaidCandidate(TEXT("C")));
}

#if WITH_DEV_AUTOMATION_TESTS
void ULocalAssignment::SetCandidatesForTest(const TArray<FRaidServerCandidate>& InCandidates)
{
	Candidates.Reset(InCandidates.Num());
	for (const FRaidServerCandidate& Candidate : InCandidates)
	{
		Candidates.Add(NormalizeCandidateAvailability(Candidate));
	}
}

bool ULocalAssignment::SetCandidateCurrentPlayersForTest(const FString& SlotId, int32 CurrentPlayers)
{
	for (FRaidServerCandidate& Candidate : Candidates)
	{
		if (Candidate.Endpoint.SlotId == SlotId)
		{
			Candidate.CurrentPlayers = CurrentPlayers;
			Candidate.Availability.CurrentPlayers = CurrentPlayers;
			Candidate.Availability.ServerState =
				CurrentPlayers >= Candidate.Availability.MaxPlayers ? ERaidServerState::Full : ERaidServerState::Online;
			return true;
		}
	}

	return false;
}
#endif
