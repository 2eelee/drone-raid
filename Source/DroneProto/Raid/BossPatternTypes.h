#pragma once

#include "CoreMinimal.h"
#include "BossPatternTypes.generated.h"

UENUM(BlueprintType)
enum class EBossPatternKind : uint8
{
	None,
	CorruptedActino,
	StellarRemnant
};

UENUM()
enum class EBossPatternServerState : uint8
{
	Stopped,
	FirstDelay,
	Telegraphing,
	Active,
	Intermission,
	PausedNoPlayers
};

UENUM(BlueprintType)
enum class EBossPatternLifecycleState : uint8
{
	Telegraphing,
	Active
};

USTRUCT(BlueprintType)
struct FBossPatternRepState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 InstanceID = 0;

	UPROPERTY(BlueprintReadOnly)
	EBossPatternKind PatternKind = EBossPatternKind::None;

	UPROPERTY(BlueprintReadOnly)
	EBossPatternLifecycleState LifecycleState = EBossPatternLifecycleState::Telegraphing;

	UPROPERTY(BlueprintReadOnly)
	float StartServerTime = 0.0f;
};

struct FBossPatternConfig
{
	float FirstDelaySeconds = 0.5f;
	float IntermissionSeconds = 2.0f;
	float GlobalHitLockSeconds = 0.7f;
	float CorruptedDurationSeconds = 5.0f;
	float CorruptedTelegraphSeconds = 1.0f;
	int32 CorruptedDamage = 20;
	float StellarDurationSeconds = 3.0f;
	float StellarTelegraphSeconds = 0.8f;
	int32 StellarDamage = 25;
};

struct FCorruptedActinoLaserPreset
{
	FCorruptedActinoLaserPreset() = default;

	FCorruptedActinoLaserPreset(
		const float InBaseAngleDegrees,
		const float InXYPhaseRadians,
		const float InZPhaseRadians)
		: BaseAngleDegrees(InBaseAngleDegrees)
		, XYPhaseRadians(InXYPhaseRadians)
		, ZPhaseRadians(InZPhaseRadians)
	{
	}

	float BaseAngleDegrees = 0.0f;
	float XYPhaseRadians = 0.0f;
	float ZPhaseRadians = 0.0f;
};

struct FCorruptedActinoConfig
{
	int32 LaserCount = 4;
	float StartRadiusCm = 800.0f;
	float EndRadiusCm = 5000.0f;
	float LengthCm = 4200.0f;
	float InnerCollisionFullWidthCm = 400.0f;
	float OuterCollisionFullWidthCm = 800.0f;
	float InnerVisualFullWidthCm = 500.0f;
	float OuterVisualFullWidthCm = 1200.0f;
	float CollisionFullHeightCm = 150.0f;
	float ZAmplitudeCm = 300.0f;
	float ZPeriodSeconds = 2.0f;
	float XYAmplitudeDegrees = 25.0f;
	float XYPeriodSeconds = 5.0f;
	FCorruptedActinoLaserPreset Presets[4] =
	{
		{0.0f, -HALF_PI, HALF_PI},
		{90.0f, HALF_PI, 0.0f},
		{180.0f, -HALF_PI, -HALF_PI},
		{270.0f, HALF_PI, PI}
	};
};

struct FStellarRemnantConfig
{
	int32 WaveCount = 2;
	int32 DamageProjectileCount = 32;
	int32 VisualProjectileCount = 16;
	int32 DamageProjectilesPerWave = 16;
	int32 VisualProjectilesPerWave = 8;
	float WaveIntervalSeconds = 0.5f;
	float StartRadiusCm = 800.0f;
	float EndRadiusCm = 5000.0f;
	float LengthCm = 4200.0f;
	float TravelSeconds = 2.5f;
	float SpeedCmPerSecond = 1680.0f;
	float CollisionRadiusCm = 70.0f;
	float DamageAngleStepDegrees = 22.5f;
	float SecondWaveOffsetDegrees = 11.25f;
	float VisualZOffsetCm = 300.0f;
	float VisualFullSizeMinCm = 100.0f;
	float VisualFullSizeMaxCm = 120.0f;
	int32 VisualDamage = 0;
};
