#pragma once

#include "CoreMinimal.h"
#include "DronePart.h"
#include "Raid/DroneCombatTypes.h"
#include "Engine/DataTable.h"
#include "DroneDataTableRows.generated.h"

class UStaticMesh;

USTRUCT(BlueprintType)
struct DRONEPROTO_API FDronePartCountRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone Data|Part")
	FName PartID = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone Data|Part")
	FText Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone Data|Part")
	EDronePartType Type = EDronePartType::Core;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone Data|Part")
	int32 MaxCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone Data|Part")
	bool IsSelectable = true;
};

USTRUCT(BlueprintType)
struct DRONEPROTO_API FDroneCoreRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone Data|Core")
	FName CoreID = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone Data|Core")
	float AttackModifier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone Data|Core")
	float MoveSpeedModifier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone Data|Core")
	FName EffectType = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone Data|Core")
	float EffectValue01 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone Data|Core")
	float EffectValue02 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone Data|Core")
	float EffectMaxValue = 0.0f;
};

USTRUCT(BlueprintType)
struct DRONEPROTO_API FDroneWeaponRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone Data|Weapon")
	FName WeaponID = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone Data|Weapon")
	float BaseDamage = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone Data|Weapon")
	FName SpecialEffectType = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone Data|Weapon")
	float SpecialValue01 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone Data|Weapon")
	float SpecialValue02 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone Data|Weapon")
	float SpecialMaxValue = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone Data|Weapon")
	int32 HitCount = 0;
};

/**
 * DataTable row used by BP_Drone to resolve an equipped weapon Part ID into
 * a display-only mesh and the per-side mount transforms. The DataTable row
 * name is the weapon Part ID (for example WEAPON_001), so this row deliberately
 * does not duplicate that key in a WeaponID field.
 */
USTRUCT(BlueprintType)
struct DRONEPROTO_API FDroneWeaponVisualRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone Data|Weapon Visual")
	TObjectPtr<UStaticMesh> WeaponMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone Data|Weapon Visual")
	FTransform LeftRelativeTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone Data|Weapon Visual")
	FTransform RightRelativeTransform = FTransform::Identity;
};

USTRUCT(BlueprintType)
struct DRONEPROTO_API FDroneBonusRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone Data|Bonus")
	int32 BonusID = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone Data|Bonus")
	FName BonusName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone Data|Bonus")
	FText BonusDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone Data|Bonus")
	int32 BonusScore = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone Data|Bonus")
	int32 SecondaryBonusScore = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone Data|Bonus")
	float MinCombatDuration = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone Data|Bonus")
	float SecondaryMinCombatDuration = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone Data|Bonus")
	float MinBossDamageRatio = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone Data|Bonus")
	float SecondaryMinBossDamageRatio = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone Data|Bonus")
	float PrimaryMinDamagePerMinute = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone Data|Bonus")
	float SecondaryMinDamagePerMinute = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone Data|Bonus")
	float PrimaryMinMoveDistance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone Data|Bonus")
	float SecondaryMinMovePerMinute = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone Data|Bonus")
	float PrimaryMinHealAmount = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone Data|Bonus")
	float SecondaryMinHealAmount = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone Data|Bonus")
	float LateJoinBossHPThresholdRatio = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone Data|Bonus")
	int32 MaxDamageTakenCount = -1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone Data|Bonus")
	bool bRequiresBossDefeated = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone Data|Bonus")
	bool bRequiresAlive = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone Data|Bonus")
	int32 MaxScore = 0;
};

USTRUCT(BlueprintType)
struct DRONEPROTO_API FDroneReportSettingsRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone Data|Report")
	int32 BonusScoreCap = 0;
};

USTRUCT(BlueprintType)
struct DRONEPROTO_API FDroneGradeRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone Data|Grade")
	EDroneReportGrade Grade = EDroneReportGrade::C;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone Data|Grade")
	float MinScore = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone Data|Grade")
	float MaxScore = 0.0f;
};
