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
	virtual FServerEndpoint ResolveServer(const FString& RequestedSlot)
		PURE_VIRTUAL(URaidAssignmentBase::ResolveServer, return FServerEndpoint{};);
};
