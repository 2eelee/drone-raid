#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RaidServerDirectorySettings.generated.h"

USTRUCT()
struct DRONEPROTO_API FRaidServerDefinition
{
	GENERATED_BODY()

	FRaidServerDefinition() = default;
	FRaidServerDefinition(FString InSlotId, int32 InPriority, FString InReservationUrl)
		: SlotId(MoveTemp(InSlotId))
		, Priority(InPriority)
		, ReservationUrl(MoveTemp(InReservationUrl))
	{
	}

	UPROPERTY(Config)
	FString SlotId;

	UPROPERTY(Config)
	int32 Priority = 0;

	UPROPERTY(Config)
	FString ReservationUrl;
};

UCLASS(Config=Engine, DefaultConfig)
class DRONEPROTO_API URaidServerDirectorySettings : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(Config)
	TArray<FRaidServerDefinition> Servers;
};
