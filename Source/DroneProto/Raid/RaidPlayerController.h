#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RaidPlayerController.generated.h"

UCLASS()
class DRONEPROTO_API ARaidPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
};
