#pragma once

#include "CoreMinimal.h"
#include "BossPatternActorBase.h"
#include "StellarRemnantPatternActor.generated.h"

class UNiagaraComponent;

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

struct FStellarRemnantVisualFrame
{
	FVector Position = FVector::ZeroVector;
	float AngleDegrees = 0.0f;
	float SizeCm = 0.0f;
	int32 WaveIndex = 0;
	bool bActive = false;
	bool bVisualOnly = false;
};

struct FStellarTelegraphVisualFrame
{
	float GatherAlpha = 0.0f;
	float CoreIntensity = 0.0f;
};

UCLASS()
class DRONEPROTO_API AStellarRemnantPatternActor : public ABossPatternActorBase
{
	GENERATED_BODY()

public:
	AStellarRemnantPatternActor();

	virtual void Tick(float DeltaSeconds) override;

	static TArray<FStellarRemnantSample> BuildLogicalSamples(
		const FStellarRemnantConfig& InConfig = FStellarRemnantConfig(),
		const FBossPatternConfig& InPatternConfig = FBossPatternConfig());
	static bool IsSampleActive(
		const FStellarRemnantSample& Sample,
		float ElapsedSeconds,
		const FStellarRemnantConfig& InConfig = FStellarRemnantConfig());
	static FVector EvaluateLocalPosition(
		const FStellarRemnantSample& Sample,
		float ElapsedSeconds,
		const FStellarRemnantConfig& InConfig = FStellarRemnantConfig());
	static TArray<FStellarRemnantVisualFrame> BuildVisualFrames(
		float ElapsedSeconds,
		const FStellarRemnantConfig& InConfig = FStellarRemnantConfig(),
		const FBossPatternConfig& InPatternConfig = FBossPatternConfig());
	static FStellarTelegraphVisualFrame BuildTelegraphVisualFrame(
		float ElapsedSeconds,
		float TelegraphDurationSeconds);
	static bool IsPointInsideSweptSample(
		const FVector& PointWorld,
		const FTransform& BossTransform,
		const FStellarRemnantSample& Sample,
		float PreviousElapsedSeconds,
		float CurrentElapsedSeconds,
		const FStellarRemnantConfig& InConfig = FStellarRemnantConfig(),
		float TargetRadiusCm = 0.0f);

#if WITH_DEV_AUTOMATION_TESTS
	void ApplyDamageForServerForTest(float PreviousElapsedSeconds, float CurrentElapsedSeconds);
	int32 GetLogicalSampleCountForTest() const;
#endif

protected:
	virtual void OnResolvedConfigSnapshot() override;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Raid|BossPattern|VFX", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraComponent> PatternVFX = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Raid|BossPattern|Debug")
	bool bEnableDebugVisualization = false;

	FStellarRemnantConfig Config;
	FBossPatternConfig PatternConfig;
	TArray<FStellarRemnantSample> Samples;
	float PreviousActiveElapsedSeconds = 0.0f;
	bool bHasPreviousActiveTime = false;

	float GetServerWorldTimeSeconds() const;
	void ApplyDamageForServer(float PreviousElapsedSeconds, float CurrentElapsedSeconds);
	void RefreshPatternVFX(float ElapsedSeconds);
	void DrawDebugPattern(float ElapsedSeconds) const;
};
