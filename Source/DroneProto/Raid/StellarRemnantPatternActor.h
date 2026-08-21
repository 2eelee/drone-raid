#pragma once

#include "CoreMinimal.h"
#include "BossPatternActorBase.h"
#include "StellarRemnantPatternActor.generated.h"

class UNiagaraComponent;
class UInstancedStaticMeshComponent;
class UStaticMeshComponent;

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

	/**
	 * 지금 떠 있는 Stellar wave 중 가장 최근 것의 인덱스. 활성 샘플이 없으면 INDEX_NONE.
	 *
	 * Wave 단계별 연출을 BP에서 붙이기 위한 조회 전용 진입점이다. 새 복제 상태나 RPC를 만들지 않는다 —
	 * 서버는 기존대로 PatternState만 결정하고, 클라이언트는 복제된 StartServerTime과
	 * 로컬 resolve된 config로 같은 값을 재구성한다. Tick의 RefreshPatternVFX와 완전히 같은 식이다.
	 *
	 * WaveIntervalSeconds(0.5)가 TravelSeconds(2.5)보다 짧아 두 wave가 동시에 활성일 수 있으므로
	 * 그중 최댓값을 돌려준다. 연출에서 의미 있는 것은 가장 최근 발사된 wave다.
	 * 피해 판정이 아니라 단계 표시가 목적이므로 visual-only 샘플도 포함한다.
	 */
	UFUNCTION(BlueprintPure, Category = "Raid|BossPattern")
	int32 GetActiveWaveIndexForVisual() const;

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
	void RefreshPatternVFXForTest(float ElapsedSeconds);
#endif

protected:
	virtual void OnResolvedConfigSnapshot() override;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Raid|BossPattern|VFX", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraComponent> PatternVFX = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Raid|BossPattern|VFX")
	TObjectPtr<UInstancedStaticMeshComponent> DamageShardRenderer = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Raid|BossPattern|VFX")
	TObjectPtr<UInstancedStaticMeshComponent> VisualShardRenderer = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Raid|BossPattern|VFX")
	TObjectPtr<UStaticMeshComponent> CoreRenderer = nullptr;

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
