#pragma once

#include "CoreMinimal.h"
#include "ServerEndpoint.generated.h"

USTRUCT(BlueprintType)
struct DRONEPROTO_API FServerEndpoint
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString SlotId;

	// true → UGameplayStatics::OpenLevel, false → ClientTravel(IP:Port)
	UPROPERTY(BlueprintReadOnly)
	FString TravelTarget;

	UPROPERTY(BlueprintReadOnly)
	bool bIsLevelName = true;
};
