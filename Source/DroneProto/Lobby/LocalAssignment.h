#pragma once

#include "CoreMinimal.h"
#include "RaidAssignmentBase.h"
#include "LocalAssignment.generated.h"

UCLASS()
class DRONEPROTO_API ULocalAssignment : public URaidAssignmentBase
{
	GENERATED_BODY()

public:
	ULocalAssignment();

	virtual FRaidAssignmentResult ResolveRaidAssignment(const FString& RequestedSlot) override;

	virtual bool IsSlotEnabled(const FString& SlotId) const override;

#if WITH_DEV_AUTOMATION_TESTS
	const TArray<FRaidServerCandidate>& GetCandidatesForTest() const { return Candidates; }
	void SetCandidatesForTest(const TArray<FRaidServerCandidate>& InCandidates);
	bool SetCandidateCurrentPlayersForTest(const FString& SlotId, int32 CurrentPlayers);
#endif

private:
	UPROPERTY()
	TArray<FRaidServerCandidate> Candidates;

	void ResetDefaultCandidates();
	FRaidServerCandidate NormalizeCandidateAvailability(const FRaidServerCandidate& Candidate) const;
	FRaidServerAvailability EvaluateAvailability(const FRaidServerCandidate& Candidate) const;
	FRaidAssignmentResult SelectCandidateByPriority(const FString& RequestedSlot) const;
	bool CanAttemptTravel(const FRaidServerCandidate& Candidate) const;
	void LogServerAvailability(const FRaidServerAvailability& Availability) const;
};
