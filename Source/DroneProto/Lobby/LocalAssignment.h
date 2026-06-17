#pragma once

#include "CoreMinimal.h"
#include "RaidAssignmentBase.h"
#include "LocalAssignment.generated.h"

UCLASS()
class DRONEPROTO_API ULocalAssignment : public URaidAssignmentBase
{
	GENERATED_BODY()

public:
	virtual FServerEndpoint ResolveServer(const FString& RequestedSlot) override;
};
