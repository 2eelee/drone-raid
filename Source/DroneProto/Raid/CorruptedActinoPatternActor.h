#pragma once

#include "CoreMinimal.h"
#include "BossPatternActorBase.h"
#include "CorruptedActinoPatternActor.generated.h"

UCLASS()
class DRONEPROTO_API ACorruptedActinoPatternActor : public ABossPatternActorBase
{
	GENERATED_BODY()

public:
	ACorruptedActinoPatternActor();

	virtual void Tick(float DeltaSeconds) override;

	static float EvaluateAngleDegrees(
		const FCorruptedActinoLaserPreset& Preset,
		float ElapsedSeconds,
		const FCorruptedActinoConfig& InConfig = FCorruptedActinoConfig());
	static float EvaluateZCm(
		const FCorruptedActinoLaserPreset& Preset,
		float ElapsedSeconds,
		const FCorruptedActinoConfig& InConfig = FCorruptedActinoConfig());
	static bool IsPointInsideLaser(
		const FVector& PointWorld,
		const FTransform& BossTransform,
		const FCorruptedActinoLaserPreset& Preset,
		float ElapsedSeconds,
		const FCorruptedActinoConfig& InConfig = FCorruptedActinoConfig());

#if WITH_DEV_AUTOMATION_TESTS
	int32 GetDamageAttemptCountForTest() const;
#endif

protected:
	virtual void OnResolvedConfigSnapshot() override;

private:
	FCorruptedActinoConfig Config;
	FBossPatternConfig PatternConfig;
	int32 DamageAttemptCountForTest = 0;

	float GetServerWorldTimeSeconds() const;
	void ApplyDamageForServer(float ElapsedSeconds);
	void DrawDebugPattern(float ElapsedSeconds) const;
	void DrawFilledTrapezoid(
		const FVector& StartCenter,
		const FVector& EndCenter,
		const FVector& RightWorld,
		float InnerHalfWidthCm,
		float OuterHalfWidthCm,
		const FColor& Color) const;
};
