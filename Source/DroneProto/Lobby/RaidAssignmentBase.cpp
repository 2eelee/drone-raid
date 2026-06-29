#include "RaidAssignmentBase.h"

FServerEndpoint URaidAssignmentBase::ResolveServer(const FString& RequestedSlot)
{
	const FRaidAssignmentResult Result = ResolveRaidAssignment(RequestedSlot);
	return Result.Result == ERaidAssignmentResultType::Success ? Result.Endpoint : FServerEndpoint{};
}
