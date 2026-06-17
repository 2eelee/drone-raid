#pragma once

#include "CoreMinimal.h"
#include "DronePart.h"
#include "DummyParts.generated.h"

// ---- 슬롯별 더미 부품 3종 (D3 합산 검증용 placeholder) ----

UCLASS()
class DRONEPROTO_API UCorePart : public UDronePart
{
	GENERATED_BODY()
public:
	UCorePart();
};

UCLASS()
class DRONEPROTO_API ULeftWeaponPart : public UDronePart
{
	GENERATED_BODY()
public:
	ULeftWeaponPart();
};

UCLASS()
class DRONEPROTO_API URightWeaponPart : public UDronePart
{
	GENERATED_BODY()
public:
	URightWeaponPart();
};
