#pragma once

#include "CoreMinimal.h"
#include "BossPatternActorBase.h"
#include "CorruptedActinoPatternActor.generated.h"

class UNiagaraComponent;
class UStaticMeshComponent;

struct FCorruptedBeamVisualSample
{
	float AngleDegrees = 0.0f;
	float ZCm = 0.0f;
	float StartRadiusCm = 0.0f;
	float EndRadiusCm = 0.0f;
	float LengthCm = 0.0f;
	float InnerVisualFullWidthCm = 0.0f;
	float OuterVisualFullWidthCm = 0.0f;
	float Intensity = 0.0f;
	bool bTelegraphing = false;
};

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
	static TArray<FCorruptedBeamVisualSample> BuildVisualSamples(
		float ElapsedSeconds,
		bool bTelegraphing,
		const FCorruptedActinoConfig& InConfig = FCorruptedActinoConfig());

#if WITH_DEV_AUTOMATION_TESTS
	int32 GetDamageAttemptCountForTest() const;
	void RefreshPatternVFXForTest(float ElapsedSeconds);
#endif

protected:
	virtual void OnResolvedConfigSnapshot() override;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Raid|BossPattern|VFX", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraComponent> PatternVFX = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Raid|BossPattern|VFX")
	TArray<TObjectPtr<UStaticMeshComponent>> BeamRenderers;

	UPROPERTY(EditDefaultsOnly, Category = "Raid|BossPattern|Debug")
	bool bEnableDebugVisualization = false;

	FCorruptedActinoConfig Config;
	FBossPatternConfig PatternConfig;
	int32 DamageAttemptCountForTest = 0;

	float GetServerWorldTimeSeconds() const;
	void ApplyDamageForServer(float ElapsedSeconds);
	void RefreshPatternVFX(float ElapsedSeconds);
	void DrawDebugPattern(float ElapsedSeconds) const;
	void DrawFilledTrapezoid(
		const FVector& StartCenter,
		const FVector& EndCenter,
		const FVector& RightWorld,
		float InnerHalfWidthCm,
		float OuterHalfWidthCm,
		const FColor& Color) const;
};
