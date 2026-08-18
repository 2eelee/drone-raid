#include "RaidAssignmentBase.h"

void URaidAssignmentBase::ResolveRaidAssignmentAsync(
	const FString& RequestedSlot,
	double RemainingSeconds,
	FRaidAssignmentComplete OnComplete)
{
	OnComplete.ExecuteIfBound(ResolveRaidAssignment(RequestedSlot));
}

FServerEndpoint URaidAssignmentBase::ResolveServer(const FString& RequestedSlot)
{
	const FRaidAssignmentResult Result = ResolveRaidAssignment(RequestedSlot);
	return Result.Result == ERaidAssignmentResultType::Success ? Result.Endpoint : FServerEndpoint{};
}
