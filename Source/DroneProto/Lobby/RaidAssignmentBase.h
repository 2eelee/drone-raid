#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ServerEndpoint.h"
#include "RaidAssignmentBase.generated.h"

UCLASS(Abstract)
class DRONEPROTO_API URaidAssignmentBase : public UObject
{
	GENERATED_BODY()

public:
	virtual FRaidAssignmentResult ResolveRaidAssignment(const FString& RequestedSlot)
		PURE_VIRTUAL(URaidAssignmentBase::ResolveRaidAssignment, return FRaidAssignmentResult::Failed(ERaidEntryFailReason::ServerListFailed, TEXT("UnimplementedAssignment")););

	virtual FServerEndpoint ResolveServer(const FString& RequestedSlot);
};
