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

	static float EvaluateAngleDegrees(const FCorruptedActinoLaserPreset& Preset, float ElapsedSeconds);
	static float EvaluateZCm(const FCorruptedActinoLaserPreset& Preset, float ElapsedSeconds);
	static bool IsPointInsideLaser(
		const FVector& PointWorld,
		const FTransform& BossTransform,
		const FCorruptedActinoLaserPreset& Preset,
		float ElapsedSeconds);

#if WITH_DEV_AUTOMATION_TESTS
	int32 GetDamageAttemptCountForTest() const;
#endif

private:
	FCorruptedActinoConfig Config;
	FBossPatternConfig PatternConfig;
	int32 DamageAttemptCountForTest = 0;

	float GetServerWorldTimeSeconds() const;
	void ApplyDamageForServer(float ElapsedSeconds);
	void DrawDebugPattern(float ElapsedSeconds) const;
	void DrawTrapezoid(
		const FVector& StartCenter,
		const FVector& EndCenter,
		const FVector& RightWorld,
		float InnerHalfWidthCm,
		float OuterHalfWidthCm,
		const FColor& Color) const;
};
