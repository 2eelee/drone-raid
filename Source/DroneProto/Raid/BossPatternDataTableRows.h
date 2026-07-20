#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "BossPatternDataTableRows.generated.h"

USTRUCT(BlueprintType)
struct FBossPatternSystemRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 PatternSystemID = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString PatternOrder;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float PatternInterval = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float FirstPatternDelay = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float HitInvincibleDuration = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 PlayerMaxHPReference = 0;
};

USTRUCT(BlueprintType)
struct FCorruptedActinoRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 PatternID = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString PatternName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Difficulty;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Damage = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Duration = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Telegraph = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 LaserCount = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float StartDistance = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float EndDistance = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Length = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float InnerHitWidth = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float OuterHitWidth = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float InnerVisualWidth = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float OuterVisualWidth = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float CollisionHeight = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ZAmplitude = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ZOscillationPeriod = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float AngleSweepRange = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool FirstUseTelegraph = false;
};

USTRUCT(BlueprintType)
struct FCorruptedActinoPresetRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 LaserIndex = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float BaseAngle = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString SweepDirection;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float XYPhase = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ZPhase = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString ZStartState;
};

USTRUCT(BlueprintType)
struct FStellarRemnantRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 PatternID = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString PatternName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Difficulty;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Damage = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Duration = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Telegraph = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 WaveCount = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 DamageCount = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 VisualCount = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 DamagePerWave = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 VisualPerWave = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float WaveInterval = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float StartDistance = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float EndDistance = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Length = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MoveDuration = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MoveSpeed = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float HitRadius = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float VisualSizeMin = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float VisualSizeMax = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float SecondWaveOffset = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float VisualZOffset = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 VisualDamage = 0;
};
