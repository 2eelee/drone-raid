#pragma once

#include "CoreMinimal.h"
#include "RaidAssignmentBase.h"
#include "RaidServerDirectorySettings.h"
#include "RemoteRaidAssignment.generated.h"

#if WITH_DEV_AUTOMATION_TESTS
struct FRaidAssignmentHttpTestResponse
{
	bool bTransportSucceeded = false;
	int32 ResponseCode = 0;
	FString Body;
	bool bDeferred = false;
};
#endif

UCLASS()
class DRONEPROTO_API URemoteRaidAssignment : public URaidAssignmentBase
{
	GENERATED_BODY()

public:
	void InitializeFromSettings();
	virtual FRaidAssignmentResult ResolveRaidAssignment(const FString& RequestedSlot) override;
	virtual void ResolveRaidAssignmentAsync(
		const FString& RequestedSlot,
		double RemainingSeconds,
		FRaidAssignmentComplete OnComplete) override;
	virtual bool IsSlotEnabled(const FString& SlotId) const override;

#if WITH_DEV_AUTOMATION_TESTS
	void SetServersForTest(const TArray<FRaidServerDefinition>& InServers);
	void SetResponsesForTest(const TArray<FRaidAssignmentHttpTestResponse>& InResponses);
	uint64 GetRequestGenerationForTest() const { return RequestGeneration; }
	void DeliverResponseForTest(uint64 Generation, bool bTransportSucceeded, int32 ResponseCode, const FString& Body);
#endif

private:
	void SortServers();
	void StartNextCandidate(uint64 Generation);
	void HandleCandidateResponse(uint64 Generation, bool bTransportSucceeded, int32 ResponseCode, const FString& Body);
	void Finish(const FRaidAssignmentResult& Result);

	TArray<FRaidServerDefinition> Servers;
	int32 NextServerIndex = 0;
	bool bSawValidServerResponse = false;
	double RequestTimeoutSeconds = 1.0;
	FString PendingRequestedSlot;
	FRaidAssignmentComplete Completion;
	uint64 RequestGeneration = 0;

#if WITH_DEV_AUTOMATION_TESTS
	bool bUseTestResponses = false;
	TArray<FRaidAssignmentHttpTestResponse> TestResponses;
	int32 NextTestResponseIndex = 0;
#endif
};
