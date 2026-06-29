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

	for (const FRaidServerCandidate& Candidate : Candidates)
	{
		if (!Candidate.bIsOnline)
		{
			continue;
		}

		if (!Candidate.bAcceptsPlayers)
		{
			continue;
		}

		if (Candidate.CurrentPlayers >= Candidate.MaxPlayers)
		{
			continue;
		}

		if (Candidate.Endpoint.TravelTarget.TrimStartAndEnd().IsEmpty())
		{
			return FRaidAssignmentResult::Failed(
				ERaidEntryFailReason::MapLoadFailed,
				FString::Printf(TEXT("InvalidTravelTarget RequestedSlot=%s CandidateSlot=%s"), *RequestedSlot, *Candidate.Endpoint.SlotId),
				Candidate.Endpoint);
		}

		return FRaidAssignmentResult::Success(
			Candidate,
			FString::Printf(TEXT("Selected RequestedSlot=%s CandidateSlot=%s"), *RequestedSlot, *Candidate.Endpoint.SlotId));
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
			return Candidate.bIsOnline && Candidate.bAcceptsPlayers;
		}
	}

	return false;
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
	Candidates = InCandidates;
}

bool ULocalAssignment::SetCandidateCurrentPlayersForTest(const FString& SlotId, int32 CurrentPlayers)
{
	for (FRaidServerCandidate& Candidate : Candidates)
	{
		if (Candidate.Endpoint.SlotId == SlotId)
		{
			Candidate.CurrentPlayers = CurrentPlayers;
			return true;
		}
	}

	return false;
}
#endif
