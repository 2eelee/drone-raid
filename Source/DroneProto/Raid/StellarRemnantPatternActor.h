#pragma once

#include "CoreMinimal.h"
#include "BossPatternActorBase.h"
#include "StellarRemnantPatternActor.generated.h"

struct FStellarRemnantSample
{
	int32 WaveIndex = 0;
	float AngleDegrees = 0.0f;
	float StartTimeSeconds = 0.0f;
	int32 Damage = 0;
	bool bVisualOnly = false;
	float VisualZOffsetCm = 0.0f;
	float VisualFullSizeCm = 0.0f;
};

UCLASS()
class DRONEPROTO_API AStellarRemnantPatternActor : public ABossPatternActorBase
{
	GENERATED_BODY()

public:
	AStellarRemnantPatternActor();

	virtual void Tick(float DeltaSeconds) override;

	static TArray<FStellarRemnantSample> BuildLogicalSamples();
	static bool IsSampleActive(const FStellarRemnantSample& Sample, float ElapsedSeconds);
	static FVector EvaluateLocalPosition(const FStellarRemnantSample& Sample, float ElapsedSeconds);
	static bool IsPointInsideSweptSample(
		const FVector& PointWorld,
		const FTransform& BossTransform,
		const FStellarRemnantSample& Sample,
		float PreviousElapsedSeconds,
		float CurrentElapsedSeconds);

#if WITH_DEV_AUTOMATION_TESTS
	void ApplyDamageForServerForTest(float PreviousElapsedSeconds, float CurrentElapsedSeconds);
	int32 GetLogicalSampleCountForTest() const;
#endif

private:
	FStellarRemnantConfig Config;
	FBossPatternConfig PatternConfig;
	TArray<FStellarRemnantSample> Samples;
	float PreviousActiveElapsedSeconds = 0.0f;
	bool bHasPreviousActiveTime = false;

	float GetServerWorldTimeSeconds() const;
	void ApplyDamageForServer(float PreviousElapsedSeconds, float CurrentElapsedSeconds);
	void DrawDebugPattern(float ElapsedSeconds) const;
};
