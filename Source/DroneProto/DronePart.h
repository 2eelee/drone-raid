#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "DronePart.generated.h"

class ADrone;

UENUM(BlueprintType)
enum class EPartSlot : uint8
{
	Core,
	LeftWeapon,
	RightWeapon
};

USTRUCT(BlueprintType)
struct FDronePartStats
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly)
	int32 HealthBonus = 0;

	UPROPERTY(EditDefaultsOnly)
	int32 AttackBonus = 0;
};

UCLASS(Abstract, EditInlineNew, DefaultToInstanced)
class DRONEPROTO_API UDronePart : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly)
	FDronePartStats StatContribution;

	UPROPERTY(EditDefaultsOnly)
	EPartSlot Slot = EPartSlot::Core;

	// stub — D6~8에서 발사 로직 구현
	virtual void ServerFire(ADrone* Owner) {}
};
