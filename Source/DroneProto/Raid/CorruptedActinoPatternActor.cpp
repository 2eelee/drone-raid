#include "CorruptedActinoPatternActor.h"

#include "BossPatternComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Drone.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// SM_CorruptedBeamWedge는 아래 기준 치수로 제작돼 있다. 렌더러 스케일을 1로 두면
	// config가 바뀌어도 시각 폭이 따라가지 않으므로 항상 기준 대비 비율로 계산한다.
	constexpr float CorruptedBeamMeshBaseLengthCm = 4200.0f;
	constexpr float CorruptedBeamMeshBaseOuterFullWidthCm = 1200.0f;

	// 예고는 폭이 아니라 두께·명도로만 active와 구분한다.
	// 폭을 줄이면 예고가 서버 충돌 폭보다 좁아져 안전 구역 경계를 잘못 알려준다.
	constexpr float CorruptedTelegraphThicknessScale = 0.35f;
	// 예고를 낮은 명도로만 표현했더니 사용자 PIE에서 아예 보이지 않았다(렌더는 정상).
	// 밝기를 올려 가시성을 확보하되, active와의 구분은 명도가 아니라 점멸로 만든다.
	constexpr float CorruptedTelegraphIntensity = 14.0f;
	constexpr float CorruptedTelegraphOpacity = 0.6f;
	constexpr float CorruptedActiveIntensity = 18.0f;
	constexpr float CorruptedActiveOpacity = 0.72f;

	// 예고는 깜빡인다. 진행할수록 주기가 빨라져 발동 시점이 읽힌다.
	// 최대값이 active에 닿으면 전환을 구분할 수 없으므로 상한을 둔다.
	constexpr float CorruptedTelegraphMinPulse = 0.5f;
	constexpr float CorruptedTelegraphMaxPulse = 1.25f;
	constexpr float CorruptedTelegraphStartHz = 2.0f;
	constexpr float CorruptedTelegraphEndHz = 6.5f;
	static_assert(
		CorruptedTelegraphIntensity * CorruptedTelegraphMaxPulse < CorruptedActiveIntensity,
		"예고 최대 밝기는 active 밝기보다 낮아야 한다");

	// 서버 경과 시간만 쓰므로 클라이언트마다 같은 위상으로 깜빡인다.
	float EvaluateTelegraphPulse(float ElapsedSeconds, float TelegraphAlpha)
	{
		const float PulseHz = FMath::Lerp(CorruptedTelegraphStartHz, CorruptedTelegraphEndHz, TelegraphAlpha);
		const float Wave = 0.5f * (1.0f + FMath::Sin(TWO_PI * PulseHz * ElapsedSeconds));
		return FMath::Lerp(CorruptedTelegraphMinPulse, CorruptedTelegraphMaxPulse, Wave);
	}

	// 빔 메시는 평면 판이라 한 장만 그리면 부피감이 없다. 같은 메시를 빔 축 기준으로
	// 롤만 다르게 여러 겹 겹쳐 어느 각도에서 봐도 두께가 읽히게 한다.
	// 메시를 교체하지 않고 렌더러 배치만으로 해결하므로 에셋 작업이 필요 없다.
	// 3겹을 세워 봤더니 단면이 별 모양이 되어 각진 기둥처럼 보였다(사용자 PIE 확인).
	// 폭을 좁혀도 인상이 남아 1겹으로 되돌린다. 부피 표현은 메시가 아니라
	// 머티리얼(중심 core + 외곽 흐름) 쪽에서 푸는 것이 맞다.
	constexpr int32 CorruptedBeamLayerCount = 1;
	// 첫 겹이 주 평면이고 나머지는 부피 힌트다. additive 누적이 과해지지 않게 낮춰 둔다.
	constexpr float CorruptedBeamSecondaryLayerWeight = 0.55f;
	// 부 레이어까지 같은 폭으로 세우면 단면이 별 모양이 되어 각진 기둥처럼 보인다.
	// 주 레이어는 명세 폭 그대로 두고 부 레이어만 좁혀 중심 core 볼륨으로만 쓴다.
	constexpr float CorruptedBeamSecondaryLayerWidthScale = 0.32f;

	float GetBeamLayerRollDegrees(int32 LayerIndex)
	{
		// 판은 양면이므로 180도 범위를 균등 분할하면 겹침이 고르다.
		return 180.0f * static_cast<float>(LayerIndex) / static_cast<float>(CorruptedBeamLayerCount);
	}

	float GetBeamLayerWeight(int32 LayerIndex)
	{
		return LayerIndex == 0 ? 1.0f : CorruptedBeamSecondaryLayerWeight;
	}

	float GetBeamLayerWidthScale(int32 LayerIndex)
	{
		return LayerIndex == 0 ? 1.0f : CorruptedBeamSecondaryLayerWidthScale;
	}

	// ---- Petal loop ----
	// 한 프레임 안에 존재하는 out-and-back 곡선이다. 과거 시간 샘플을 모아 만들지 않는다.
	//   u=0   보스 근처 StartRadius
	//   u=0.5 가장 바깥 EndRadius
	//   u=1   다시 StartRadius
	// Radial = Start + Length * 0.5 * (1 - cos(2*pi*u)) 가 위 왕복을 만든다.
	constexpr int32 CorruptedPetalStrandCount = 4;      // Primary 1 + Secondary 3
	constexpr int32 CorruptedPetalSampleCount = 40;     // u 해상도
	// 꽃잎이 좌우로 벌어지는 폭. 기획의 시각 폭 5m->12m는 꽃잎 전체가 차지하는
	// envelope이지 리본 한 가닥의 두께가 아니다. 여기서 envelope을 만든다.
	constexpr float CorruptedPetalHalfWidthCm = 760.0f;
	// 꽃잎이 판정 길이 42m를 그대로 따라가면 카메라(보스에서 49m)를 지나쳐 감싼다.
	// 시각 envelope은 판정과 같을 필요가 없으므로 앞쪽에서 닫는다.
	constexpr float CorruptedPetalReachRatio = 0.72f;
	// 시작점과 끝점이 같은 좌표면 가닥이 한 점에 뭉친다. 살짝 벌려 벌어진 채로 닫는다.
	constexpr float CorruptedPetalMouthCm = 240.0f;
	// 리본 한 가닥의 실제 두께. envelope과 혼동하면 다시 색면이 된다.
	// ribbon 한 가닥의 body 두께다. 기획의 시각 폭 5m~12m는 petal 전체 envelope이고
	// 이 값으로 그 폭을 채우지 않는다. 가늘면 bloom이 폭 전체를 태워 흰 선이 된다.
	constexpr float CorruptedPrimaryRibbonWidthCm = 190.0f;
	// strand끼리 topology는 공유하고 작은 차이만 준다. 크게 흔들면 꽃잎이 깨진다.
	// 이 값이 크면 secondary가 primary와 별도 궤도처럼 갈라져 "빔 4발"로 읽힌다.
	// primary 주변에서 겹쳐 흐르도록 작게 잡는다.
	constexpr float CorruptedStrandLateralAmpCm = 42.0f;
	constexpr float CorruptedStrandZAmpCm = 34.0f;
	// 정적 실루엣부터 검증하기 위한 프로토타입 플래그.
	// 켜면 XY sweep을 멈추고 꽃잎을 카메라 옆으로 고정해 outer turn까지 화면에 들어온다.
	constexpr bool bCorruptedPetalPrototypeFreeze = true;
	constexpr float CorruptedStrandLateralFreq = 0.9f;
	constexpr float CorruptedStrandZFreq = 1.3f;

	float GetStrandPhase(int32 StrandIndex)
	{
		// 0, 2.0, 4.0, 6.0 라디안. 서로 겹치지 않게 벌린다.
		return 2.0f * static_cast<float>(StrandIndex);
	}

	float GetStrandAmplitudeScale(int32 StrandIndex)
	{
		// Primary(0)는 흔들리지 않아 꽃잎 형태의 기준선이 된다.
		return StrandIndex == 0 ? 0.0f : 1.0f;
	}

	// 폭이 전부 같으면 "동급 네온 선 4개"로 읽힌다. primary 대비 50~70%로 위계를 준다.
	float GetStrandRibbonWidthCm(int32 StrandIndex)
	{
		// Primary만 키우고 Secondary 비율은 유지한다. 같이 키우면 hierarchy가 무너진다.
		static constexpr float WidthCm[] = {190.0f, 88.0f, 72.0f, 64.0f};
		return WidthCm[StrandIndex % UE_ARRAY_COUNT(WidthCm)];
	}

	// ---- strand 비대칭 ----
	// secondary를 primary의 평행 복사본으로 두면 동심 타원이 포개진 모습이 된다.
	// Envelope(양 끝 0, 중앙 최대)으로 offset을 제한하면 뿌리에서는 한 점으로 모이고
	// 중간에서만 벌어져 서로 교차한다.
	// 진폭이 크면 secondary가 primary envelope 밖으로 나가 별도의 닫힌 고리를 만든다.
	// 이전 값의 40~45% 수준으로 낮춰 꽃잎 덩어리 안에 머물게 한다.
	float GetStrandBaseOffsetCm(int32 StrandIndex)
	{
		static constexpr float OffsetCm[] = {0.0f, -92.0f, 72.0f, -44.0f};
		return OffsetCm[StrandIndex % UE_ARRAY_COUNT(OffsetCm)];
	}

	float GetStrandWobbleCm(int32 StrandIndex)
	{
		static constexpr float WobbleCm[] = {0.0f, 112.0f, 128.0f, 94.0f};
		return WobbleCm[StrandIndex % UE_ARRAY_COUNT(WobbleCm)];
	}

	// 최소 하나는 중간에서 다른 리본의 위/아래를 지나가야 층이 읽힌다.
	// 다만 크게 주면 위로 솟아 별도 타원이 되므로 얕게만 준다.
	float GetStrandZWobbleCm(int32 StrandIndex)
	{
		static constexpr float ZWobbleCm[] = {0.0f, 74.0f, -102.0f, 56.0f};
		return ZWobbleCm[StrandIndex % UE_ARRAY_COUNT(ZWobbleCm)];
	}

	// secondary가 전부 u=0..1을 완주하면 결과는 필연적으로 "겹친 네온 타원"이 된다.
	// 완전한 폐곡선은 Primary 하나뿐이고 나머지는 자기 구간에서만 존재하는 부분 리본이다.
	void GetStrandURange(int32 StrandIndex, float& OutStart, float& OutEnd)
	{
		static constexpr float Ranges[][2] =
		{
			{0.00f, 1.00f},   // Primary: 유일한 closed petal
			{0.12f, 0.68f},   // Secondary A
			{0.30f, 0.87f},   // Secondary B
			{0.18f, 0.52f},   // Secondary C
		};
		const int32 Index = StrandIndex % UE_ARRAY_COUNT(Ranges);
		OutStart = Ranges[Index][0];
		OutEnd = Ranges[Index][1];
	}

	bool IsClosedStrand(int32 StrandIndex)
	{
		return StrandIndex == 0;
	}

	// ---- filament ----
	// 내부를 메우는 층이 아니다. 주 ribbon 사이에 드문드문 걸리는 짧은 방전이다.
	// 개수를 늘리면 바로 lightning mesh가 되므로 적게 유지한다.
	constexpr int32 CorruptedFilamentBoltCount = 5;
	constexpr int32 CorruptedFilamentSampleCount = 7;
	// ribbon보다 훨씬 가늘어야 한다. filament가 주 리본보다 눈에 띄면 실패다.
	constexpr float CorruptedFilamentWidthCm = 16.0f;

	// strand마다 위상이 달라야 일부 구간은 벌어지고 일부는 가까워진다.
	float GetStrandOffsetPhase(int32 StrandIndex)
	{
		static constexpr float PhaseRad[] = {0.0f, 0.6f, 2.4f, 4.1f};
		return PhaseRad[StrandIndex % UE_ARRAY_COUNT(PhaseRad)];
	}

	// 꽃잎을 XY 평면에만 두면 42m x 10m 납작한 타원이 바닥에 눕는다.
	// 전투 카메라는 드론 뒤 14m, 높이 3.8m라 거의 수평에서 보므로 그 타원이 선으로 압축된다.
	// strand마다 꽃잎 평면을 다른 각도로 기울여 세우면 어느 시점에서도 폭이 남고,
	// 서로 다른 평면의 꽃잎이 겹쳐 부피감이 생긴다.
	float GetStrandTiltRadians(int32 StrandIndex)
	{
		// 레퍼런스의 꽃잎은 대체로 수평으로 누워 보스 주위에 펼쳐진다. 크게 세우면
		// 하늘로 솟구쳐 화면을 벗어난다. 낮은 카메라에서 폭이 죽지 않을 만큼만 기울인다.
		// strand마다 다른 각도로 기울이면 꽃잎 평면이 회전 복사되어 동심 타원이 포개진다.
		// 평면은 하나로 통일하고, strand 차이는 envelope offset으로만 만든다.
		(void)StrandIndex;
		return FMath::DegreesToRadians(16.0f);
	}
}

ACorruptedActinoPatternActor::ACorruptedActinoPatternActor()
{
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	PatternVFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("PatternVFX"));
	PatternVFX->SetupAttachment(Root);
	PatternVFX->SetAutoActivate(false);
	PatternVFX->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PatternVFX->SetGenerateOverlapEvents(false);
	PatternVFX->SetCanEverAffectNavigation(false);
	PatternVFX->CastShadow = false;
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> PatternVFXAsset(
		TEXT("/Game/VFX/Boss/Telegraph01/NS_CorruptedBeam.NS_CorruptedBeam"));
	if (PatternVFXAsset.Succeeded())
	{
		PatternVFX->SetAsset(PatternVFXAsset.Object);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> BeamMeshAsset(
		TEXT("/Game/VFX/Boss/Telegraph01/SM_CorruptedBeamWedge.SM_CorruptedBeamWedge"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BeamMaterialAsset(
		TEXT("/Game/VFX/Boss/Telegraph01/M_CorruptedBeam.M_CorruptedBeam"));
	// 빔 하나가 렌더러 CorruptedBeamLayerCount개를 쓴다. 인덱스는 [빔 * 레이어수 + 레이어]다.
	BeamRenderers.Reserve(4 * CorruptedBeamLayerCount);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		for (int32 Layer = 0; Layer < CorruptedBeamLayerCount; ++Layer)
		{
			UStaticMeshComponent* BeamRenderer = CreateDefaultSubobject<UStaticMeshComponent>(
				*FString::Printf(TEXT("BeamRenderer_%02d_%d"), Index, Layer));
			BeamRenderer->SetupAttachment(Root);
			BeamRenderer->SetStaticMesh(BeamMeshAsset.Object);
			BeamRenderer->SetMaterial(0, BeamMaterialAsset.Object);
			BeamRenderer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			BeamRenderer->SetGenerateOverlapEvents(false);
			BeamRenderer->SetCanEverAffectNavigation(false);
			BeamRenderer->CastShadow = false;
			// Additive/translucent 머티리얼은 Nanite 경로를 쓸 수 없다. fallback을 강제해
			// 런타임 Nanite 경고를 막는다.
			BeamRenderer->bDisallowNanite = true;
			BeamRenderer->SetVisibility(false, true);
			BeamRenderers.Add(BeamRenderer);
		}
	}
	PrimaryActorTick.bCanEverTick = true;
}

void ACorruptedActinoPatternActor::OnResolvedConfigSnapshot()
{
	Config = GetResolvedConfigSnapshot().Corrupted;
	PatternConfig = GetResolvedConfigSnapshot().Common;
}

void ACorruptedActinoPatternActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasResolvedConfigSnapshot() && !TryAcquireResolvedConfigSnapshot())
	{
		return;
	}

	const FBossPatternRepState& State = GetPatternState();
	if (State.PatternKind != EBossPatternKind::CorruptedActino)
	{
		return;
	}

	const float ElapsedSeconds = FMath::Max(0.0f, GetServerWorldTimeSeconds() - State.StartServerTime);
	if (HasAuthority() && State.LifecycleState == EBossPatternLifecycleState::Active)
	{
		ApplyDamageForServer(ElapsedSeconds);
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		RefreshPatternVFX(ElapsedSeconds);
	}
	if (bEnableDebugVisualization && GetNetMode() != NM_DedicatedServer)
	{
		DrawDebugPattern(ElapsedSeconds);
	}
}

float ACorruptedActinoPatternActor::EvaluateAngleDegrees(
	const FCorruptedActinoLaserPreset& Preset,
	float ElapsedSeconds,
	const FCorruptedActinoConfig& InConfig)
{
	const float Phase = TWO_PI * ElapsedSeconds / InConfig.XYPeriodSeconds + Preset.XYPhaseRadians;
	return Preset.BaseAngleDegrees + FMath::Sin(Phase) * InConfig.XYAmplitudeDegrees;
}

float ACorruptedActinoPatternActor::EvaluateZCm(
	const FCorruptedActinoLaserPreset& Preset,
	float ElapsedSeconds,
	const FCorruptedActinoConfig& InConfig)
{
	const float Phase = TWO_PI * ElapsedSeconds / InConfig.ZPeriodSeconds + Preset.ZPhaseRadians;
	return FMath::Sin(Phase) * InConfig.ZAmplitudeCm;
}

bool ACorruptedActinoPatternActor::IsPointInsideLaser(
	const FVector& PointWorld,
	const FTransform& BossTransform,
	const FCorruptedActinoLaserPreset& Preset,
	float ElapsedSeconds,
	const FCorruptedActinoConfig& InConfig)
{
	const FVector PointLocal = BossTransform.InverseTransformPosition(PointWorld);
	const float AngleRadians = FMath::DegreesToRadians(EvaluateAngleDegrees(Preset, ElapsedSeconds, InConfig));
	const FVector Direction(FMath::Cos(AngleRadians), FMath::Sin(AngleRadians), 0.0f);
	const FVector Right(-Direction.Y, Direction.X, 0.0f);
	const float LongitudinalCm = FVector::DotProduct(PointLocal, Direction) - InConfig.StartRadiusCm;
	if (LongitudinalCm < -KINDA_SMALL_NUMBER || LongitudinalCm > InConfig.LengthCm + KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float Progress = FMath::Clamp(LongitudinalCm / InConfig.LengthCm, 0.0f, 1.0f);
	const float CollisionHalfWidthCm = 0.5f * FMath::Lerp(
		InConfig.InnerCollisionFullWidthCm,
		InConfig.OuterCollisionFullWidthCm,
		Progress);
	const float LateralCm = FVector::DotProduct(PointLocal, Right);
	const float VerticalOffsetCm = PointLocal.Z - EvaluateZCm(Preset, ElapsedSeconds, InConfig);
	return FMath::Abs(LateralCm) <= CollisionHalfWidthCm + KINDA_SMALL_NUMBER
		&& FMath::Abs(VerticalOffsetCm) <= InConfig.CollisionFullHeightCm * 0.5f + KINDA_SMALL_NUMBER;
}

TArray<FCorruptedBeamVisualSample> ACorruptedActinoPatternActor::BuildVisualSamples(
	float ElapsedSeconds,
	bool bTelegraphing,
	const FCorruptedActinoConfig& InConfig)
{
	// 예고는 자기 경과 시간이 아니라 active 시작 자세(t=0)를 그린다.
	// lifecycle 전환에서 StartServerTime이 다시 잡히므로 예고를 자기 경과 시간으로 그리면
	// 발동 순간 빔이 예고 종료 각도에서 active 시작 각도로 순간 이동해
	// 예고가 가리킨 위치와 실제 공격 위치가 어긋난다.
	const float SampleTimeSeconds = bTelegraphing ? 0.0f : ElapsedSeconds;
	TArray<FCorruptedBeamVisualSample> Result;
	Result.Reserve(InConfig.LaserCount);
	for (int32 Index = 0; Index < InConfig.LaserCount; ++Index)
	{
		const FCorruptedActinoLaserPreset& Preset = InConfig.Presets[Index];
		FCorruptedBeamVisualSample& Sample = Result.AddDefaulted_GetRef();
		Sample.AngleDegrees = EvaluateAngleDegrees(Preset, SampleTimeSeconds, InConfig);
		Sample.ZCm = EvaluateZCm(Preset, SampleTimeSeconds, InConfig);
		Sample.StartRadiusCm = InConfig.StartRadiusCm;
		Sample.EndRadiusCm = InConfig.StartRadiusCm + InConfig.LengthCm;
		Sample.LengthCm = InConfig.LengthCm;
		Sample.InnerVisualFullWidthCm = InConfig.InnerVisualFullWidthCm;
		Sample.OuterVisualFullWidthCm = InConfig.OuterVisualFullWidthCm;
		Sample.Intensity = bTelegraphing ? 0.32f : 1.0f;
		Sample.bTelegraphing = bTelegraphing;
	}
	return Result;
}

TArray<FCorruptedPetalSample> ACorruptedActinoPatternActor::BuildPetalSamples(
	float ElapsedSeconds,
	int32 PresetIndex,
	const FCorruptedActinoConfig& InConfig,
	TOptional<float> OverrideAngleDegrees)
{
	TArray<FCorruptedPetalSample> Result;
	if (!InConfig.Presets || PresetIndex < 0 || PresetIndex >= InConfig.LaserCount)
	{
		return Result;
	}

	const FCorruptedActinoLaserPreset& Preset = InConfig.Presets[PresetIndex];
	// 꽃잎 전체가 향하는 방향과 높이는 기존 sweep을 그대로 쓴다. 판정과 같은 식이다.
	// 프로토타입 동안에는 sweep을 멈추고 방향을 밖에서 지정해 outer turn까지 화면에 들어오게 한다.
	const float SweepSeconds = bCorruptedPetalPrototypeFreeze ? 0.0f : ElapsedSeconds;
	const float BaseAngleRadians = FMath::DegreesToRadians(
		bCorruptedPetalPrototypeFreeze && OverrideAngleDegrees.IsSet()
			? OverrideAngleDegrees.GetValue()
			: EvaluateAngleDegrees(Preset, SweepSeconds, InConfig));
	const FVector Forward(FMath::Cos(BaseAngleRadians), FMath::Sin(BaseAngleRadians), 0.0f);
	const FVector Right(-Forward.Y, Forward.X, 0.0f);
	const float BaseZCm = EvaluateZCm(Preset, SweepSeconds, InConfig);

	auto EvaluatePoint = [&](int32 Strand, float SampleFraction) -> FVector
	{
		const float Phase = GetStrandPhase(Strand);
		const float AmpScale = GetStrandAmplitudeScale(Strand);
		{
			// 부분 리본은 자기 구간만 훑는다. Primary만 0..1 전체를 돈다.
			float URangeStart = 0.0f;
			float URangeEnd = 1.0f;
			GetStrandURange(Strand, URangeStart, URangeEnd);
			const float U = FMath::Lerp(URangeStart, URangeEnd, SampleFraction);
			const float Turn = TWO_PI * U;

			// u=0 -> Start, u=0.5 -> Start+Length, u=1 -> Start. 한 프레임 안의 왕복이다.
			const float RadialCm = InConfig.StartRadiusCm
				+ InConfig.LengthCm * CorruptedPetalReachRatio * 0.5f * (1.0f - FMath::Cos(Turn));
			// 나갈 때와 들어올 때가 갈라져 꽃잎 폭이 생긴다.
			// sin(Turn)만 쓰면 u=0과 u=1이 정확히 같은 점이라 가닥이 뭉친다.
			// cos 항을 더해 입구를 벌린 채로 닫는다.
			float OpenCm = CorruptedPetalHalfWidthCm * FMath::Sin(Turn)
				+ CorruptedPetalMouthCm * (1.0f - FMath::Cos(Turn)) * 0.5f
				- CorruptedPetalMouthCm * 0.5f;

			// 꽃잎이 벌어지는 방향을 기울여 세운다. 평면은 모든 strand가 공유한다.
			const float Tilt = GetStrandTiltRadians(Strand);
			const float LateralCm = OpenCm * FMath::Cos(Tilt);
			const float PetalUpCm = OpenCm * FMath::Sin(Tilt);

			// 자기 구간 안에서 0 -> 최대 -> 0. 부분 리본은 구간 중간에서 벌어졌다가
			// 양끝에서 primary 곡선으로 되돌아오며 사라진다.
			const float Envelope = FMath::Sin(PI * SampleFraction);
			const float OffsetPhase = GetStrandOffsetPhase(Strand);
			// base와 wobble의 위상이 strand마다 달라 일부 구간은 벌어지고 일부는 겹친다.
			const float StrandLateralCm =
				(GetStrandBaseOffsetCm(Strand)
					+ FMath::Sin(U * 3.0f * PI + OffsetPhase) * GetStrandWobbleCm(Strand))
				* Envelope;
			// Z는 부호가 서로 달라 어떤 strand는 위로, 어떤 strand는 아래로 지나간다.
			const float StrandZCm =
				FMath::Sin(U * 2.0f * PI + OffsetPhase + SweepSeconds * CorruptedStrandZFreq)
				* GetStrandZWobbleCm(Strand) * Envelope;

			// 미세 흔들림은 topology를 깨지 않을 만큼만 남긴다.
			const float MicroLateralCm =
				FMath::Sin(U * 4.0f * PI + SweepSeconds * CorruptedStrandLateralFreq + Phase)
				* CorruptedStrandLateralAmpCm * AmpScale;

			const float ZCm = BaseZCm + PetalUpCm + StrandZCm
				+ FMath::Sin(U * 3.0f * PI + SweepSeconds * CorruptedStrandZFreq + Phase)
					* CorruptedStrandZAmpCm * AmpScale;

			return Forward * RadialCm
				+ Right * (LateralCm + StrandLateralCm + MicroLateralCm)
				+ FVector::UpVector * ZCm;
		}
	};

	Result.Reserve(CorruptedPetalStrandCount * CorruptedPetalSampleCount);
	for (int32 Strand = 0; Strand < CorruptedPetalStrandCount; ++Strand)
	{
		float URangeStart = 0.0f;
		float URangeEnd = 1.0f;
		GetStrandURange(Strand, URangeStart, URangeEnd);
		for (int32 Sample = 0; Sample < CorruptedPetalSampleCount; ++Sample)
		{
			const float SampleFraction =
				static_cast<float>(Sample) / static_cast<float>(CorruptedPetalSampleCount - 1);
			FCorruptedPetalSample& Out = Result.AddDefaulted_GetRef();
			Out.LocalPosition = EvaluatePoint(Strand, SampleFraction);
			// 구간 양끝에서 폭이 0이 되어 자연스럽게 fade out 한다.
			// 길이 방향으로 약한 두께 변화를 줘 균일한 네온 튜브처럼 보이지 않게 한다.
			const float Ripple = 0.78f + 0.22f * FMath::Sin(
				SampleFraction * 9.0f * PI + GetStrandOffsetPhase(Strand) * 1.7f);
			Out.RibbonWidthCm =
				GetStrandRibbonWidthCm(Strand) * FMath::Sin(PI * SampleFraction) * Ripple;
			Out.LinkOrder = FMath::Lerp(URangeStart, URangeEnd, SampleFraction);
			Out.StrandIndex = Strand;
		}
	}

	// ---- filament: 주 ribbon 사이에 드문드문 걸리는 짧은 방전 ----
	// 내부를 메우지 않는다. 균일 간격도, 진행 방향 평행선도 금지다.
	// 결정론적 해시라 프레임마다 튀지 않는다.
	auto Hash01 = [](int32 Seed) -> float
	{
		const uint32 X = static_cast<uint32>(Seed) * 747796405u + 2891336453u;
		const uint32 Word = ((X >> ((X >> 28) + 4)) ^ X) * 277803737u;
		return static_cast<float>((Word >> 22) ^ Word) / 4294967295.0f;
	};

	for (int32 Bolt = 0; Bolt < CorruptedFilamentBoltCount; ++Bolt)
	{
		// 가까운 u끼리 이으면 같은 rail 위에서 짧게 꼬여 outer turn 근처에 뭉친다.
		// petal은 u와 1-u가 같은 radial의 반대쪽 rail이므로, 그 둘을 이어야
		// filament가 outbound rail에서 inbound rail로 실제로 횡단한다.
		const float AnchorU = FMath::Lerp(0.20f, 0.44f, Hash01(Bolt * 13 + 1));
		const float TargetU = FMath::Clamp(
			1.0f - AnchorU + (Hash01(Bolt * 29 + 7) - 0.5f) * 0.10f, 0.52f, 0.86f);

		// 양 끝을 Primary rail에서 뽑는다. Primary만 u=0..1 전체를 돌아 항상 유효하다.
		const FVector From = EvaluatePoint(0, AnchorU);
		const FVector To = EvaluatePoint(0, TargetU);
		const float SagCm = FMath::Lerp(50.0f, 155.0f, Hash01(Bolt * 67 + 5));
		// 모두 같은 방향으로 처지면 규칙적으로 보인다.
		const float SagSign = Hash01(Bolt * 71 + 13) < 0.5f ? -1.0f : 1.0f;

		for (int32 Sample = 0; Sample < CorruptedFilamentSampleCount; ++Sample)
		{
			const float T =
				static_cast<float>(Sample) / static_cast<float>(CorruptedFilamentSampleCount - 1);
			// 가운데가 늘어져 거미줄처럼 걸린다. 직선으로 두면 기하학적으로 보인다.
			const float Sag = FMath::Sin(PI * T) * SagCm * SagSign;
			const float Jitter = FMath::Sin(T * 5.0f * PI + Bolt * 1.9f) * 26.0f;

			FCorruptedPetalSample& Out = Result.AddDefaulted_GetRef();
			Out.LocalPosition = FMath::Lerp(From, To, T)
				+ FVector::UpVector * (Sag + Jitter);
			// ribbon보다 훨씬 가늘다. 눈에 띄면 실패다.
			Out.RibbonWidthCm = CorruptedFilamentWidthCm * FMath::Sin(PI * T);
			// 6가닥을 한 리본에 순서대로 담으므로 LinkOrder는 전역 순서여야 한다.
			// bolt 경계는 width가 0이라 이어지는 구간이 보이지 않는다.
			Out.LinkOrder = static_cast<float>(Bolt * CorruptedFilamentSampleCount + Sample)
				/ static_cast<float>(CorruptedFilamentBoltCount * CorruptedFilamentSampleCount - 1);
			Out.StrandIndex = CorruptedPetalStrandCount + Bolt;
		}
	}
	return Result;
}

void ACorruptedActinoPatternActor::RefreshPatternVFX(float ElapsedSeconds)
{
	if (!PatternVFX)
	{
		return;
	}

	const EBossPatternLifecycleState Lifecycle = GetPatternState().LifecycleState;
	const bool bTelegraphing = Lifecycle == EBossPatternLifecycleState::Telegraphing;
	const bool bActive = Lifecycle == EBossPatternLifecycleState::Active;

	if (!bTelegraphing && !bActive)
	{
		PatternVFX->DeactivateImmediate();
		for (UStaticMeshComponent* BeamRenderer : BeamRenderers)
		{
			if (BeamRenderer)
			{
				BeamRenderer->SetVisibility(false, true);
			}
		}
		return;
	}

	// 예고 진행률은 발동 시점을 읽게 하는 유일한 단서다. 종료 직전 pulse로 밝기를 끌어올린다.
	const float TelegraphAlpha = bTelegraphing
		? FMath::Clamp(
			ElapsedSeconds / FMath::Max(PatternConfig.CorruptedTelegraphSeconds, KINDA_SMALL_NUMBER),
			0.0f,
			1.0f)
		: 0.0f;
	const float TelegraphPulse = bTelegraphing
		? EvaluateTelegraphPulse(ElapsedSeconds, TelegraphAlpha)
		: 1.0f;

	const TArray<FCorruptedBeamVisualSample> VisualSamples = BuildVisualSamples(ElapsedSeconds, bTelegraphing, Config);
	TArray<FVector> BeamPositions;
	TArray<FVector> BeamRotations;
	TArray<float> InnerWidths;
	TArray<float> OuterWidths;
	BeamPositions.Reserve(VisualSamples.Num());
	BeamRotations.Reserve(VisualSamples.Num());
	InnerWidths.Reserve(VisualSamples.Num());
	OuterWidths.Reserve(VisualSamples.Num());
	for (int32 Index = 0; Index < VisualSamples.Num(); ++Index)
	{
		const FCorruptedBeamVisualSample& Sample = VisualSamples[Index];
		const float AngleRadians = FMath::DegreesToRadians(Sample.AngleDegrees);
		const FVector Direction(FMath::Cos(AngleRadians), FMath::Sin(AngleRadians), 0.0f);
		BeamPositions.Add(Direction * (Sample.StartRadiusCm + Sample.LengthCm * 0.5f) + FVector::UpVector * Sample.ZCm);
		BeamRotations.Add(FVector(0.0f, Sample.AngleDegrees, 0.0f));
		InnerWidths.Add(Sample.InnerVisualFullWidthCm);
		OuterWidths.Add(Sample.OuterVisualFullWidthCm);
		const FVector BeamOrigin = Direction * Sample.StartRadiusCm + FVector::UpVector * Sample.ZCm;
		const FVector BeamScale(
			Sample.LengthCm / CorruptedBeamMeshBaseLengthCm,
			Sample.OuterVisualFullWidthCm / CorruptedBeamMeshBaseOuterFullWidthCm,
			bTelegraphing ? CorruptedTelegraphThicknessScale : 1.0f);
		const float BeamIntensity =
			(bTelegraphing ? CorruptedTelegraphIntensity : CorruptedActiveIntensity) * TelegraphPulse;
		// 불투명도도 같이 흔들어야 점멸이 확실히 읽힌다.
		const float BeamOpacity = bTelegraphing
			? FMath::Clamp(CorruptedTelegraphOpacity * TelegraphPulse, 0.0f, CorruptedActiveOpacity)
			: CorruptedActiveOpacity;

		for (int32 Layer = 0; Layer < CorruptedBeamLayerCount; ++Layer)
		{
			const int32 RendererIndex = Index * CorruptedBeamLayerCount + Layer;
			if (!BeamRenderers.IsValidIndex(RendererIndex) || !BeamRenderers[RendererIndex])
			{
				continue;
			}

			UStaticMeshComponent* BeamRenderer = BeamRenderers[RendererIndex];
			const float LayerWeight = GetBeamLayerWeight(Layer);
			BeamRenderer->SetRelativeLocation(BeamOrigin);
			// Roll이 먼저 적용되므로 빔이 자기 축을 중심으로 돈 뒤 Yaw로 방향이 정해진다.
			BeamRenderer->SetRelativeRotation(
				FRotator(0.0f, Sample.AngleDegrees, GetBeamLayerRollDegrees(Layer)));
			BeamRenderer->SetRelativeScale3D(FVector(
				BeamScale.X,
				BeamScale.Y * GetBeamLayerWidthScale(Layer),
				BeamScale.Z));
			BeamRenderer->SetScalarParameterValueOnMaterials(
				TEXT("VFXIntensity"), BeamIntensity * LayerWeight);
			BeamRenderer->SetScalarParameterValueOnMaterials(
				TEXT("VFXOpacity"), BeamOpacity * LayerWeight);
			// 머티리얼이 예고와 active를 색·패턴으로 갈라 쓸 수 있게 상태와 실측 치수를 넘긴다.
			BeamRenderer->SetScalarParameterValueOnMaterials(
				TEXT("VFXTelegraph"), bTelegraphing ? 1.0f : 0.0f);
			BeamRenderer->SetScalarParameterValueOnMaterials(TEXT("VFXTelegraphAlpha"), TelegraphAlpha);
			BeamRenderer->SetScalarParameterValueOnMaterials(TEXT("VFXBeamLength"), Sample.LengthCm);
			BeamRenderer->SetScalarParameterValueOnMaterials(TEXT("VFXInnerWidth"), Sample.InnerVisualFullWidthCm);
			BeamRenderer->SetScalarParameterValueOnMaterials(TEXT("VFXOuterWidth"), Sample.OuterVisualFullWidthCm);
			// SM_CorruptedBeamWedge는 42m x 12m 꽉 찬 판이다. telegraph/active 어느 쪽에서도
			// 플레이어에게 보여주지 않고 개발용 debug 시각화에서만 쓴다.
			// 공격 표현은 petal ribbon이 맡는다.
			BeamRenderer->SetVisibility(bEnableDebugVisualization, true);
		}
	}
	// 위치는 Position 배열로 보낸다. UE 5는 Position과 Vector를 별도 타입으로 취급하므로
	// Vector로 보내면 Niagara에서 Convert 노드를 한 겹 끼워야 하고 LWC 경고가 따라붙는다.
	// 회전값은 위치가 아니므로 Vector 그대로 둔다.
	// petal loop. strand마다 별도 Emitter가 하나의 리본을 그리므로 RibbonID 배열은 보내지 않는다.
	// UE 5.7.4 실측: Particles.RibbonID는 FNiagaraID이고 배열 setter가 없다.
	// Particles.RibbonLinkOrder와 Particles.RibbonWidth는 float이라 그대로 전달할 수 있다.
	// 프로토타입 동안에는 꽃잎을 카메라 옆으로 세워 outer turn까지 화면에 들어오게 한다.
	// 카메라는 로컬 드론 뒤에서 보스를 보므로, 드론 방향의 90도 옆이 곧 화면 가로 방향이다.
	TOptional<float> PetalAngleOverride;
	if (bCorruptedPetalPrototypeFreeze)
	{
		if (const UWorld* World = GetWorld())
		{
			if (const APlayerController* PC = World->GetFirstPlayerController())
			{
				if (const APawn* LocalPawn = PC->GetPawn())
				{
					const FVector ToPawn =
						GetActorTransform().InverseTransformPosition(LocalPawn->GetActorLocation());
					if (!ToPawn.IsNearlyZero())
					{
						PetalAngleOverride =
							FMath::RadiansToDegrees(FMath::Atan2(ToPawn.Y, ToPawn.X)) + 90.0f;
					}
				}
			}
		}
	}
	const TArray<FCorruptedPetalSample> PetalSamples =
		BuildPetalSamples(ElapsedSeconds, 0, Config, PetalAngleOverride);
	for (int32 Strand = 0; Strand < CorruptedPetalStrandCount; ++Strand)
	{
		TArray<FVector> StrandPositions;
		TArray<float> StrandWidths;
		TArray<float> StrandLinkOrders;
		StrandPositions.Reserve(CorruptedPetalSampleCount);
		StrandWidths.Reserve(CorruptedPetalSampleCount);
		StrandLinkOrders.Reserve(CorruptedPetalSampleCount);
		for (const FCorruptedPetalSample& Petal : PetalSamples)
		{
			if (Petal.StrandIndex != Strand)
			{
				continue;
			}
			StrandPositions.Add(Petal.LocalPosition);
			// 예고는 같은 형태를 얇고 어둡게만 보여준다.
			StrandWidths.Add(Petal.RibbonWidthCm * (bTelegraphing ? 0.35f : 1.0f));
			StrandLinkOrders.Add(Petal.LinkOrder);
		}

		const FString Suffix = FString::FromInt(Strand);
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayPosition(
			PatternVFX, *(TEXT("PetalPosition") + Suffix), StrandPositions);
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayFloat(
			PatternVFX, *(TEXT("PetalWidth") + Suffix), StrandWidths);
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayFloat(
			PatternVFX, *(TEXT("PetalLinkOrder") + Suffix), StrandLinkOrders);
	}
	PatternVFX->SetVariableInt(TEXT("User.PetalSampleCount"), CorruptedPetalSampleCount);

	// filament는 StrandIndex가 CorruptedPetalStrandCount 이상이다. 한 Emitter가 전부 담는다.
	{
		TArray<FVector> FilamentPositions;
		TArray<float> FilamentWidths;
		TArray<float> FilamentLinkOrders;
		for (const FCorruptedPetalSample& Petal : PetalSamples)
		{
			if (Petal.StrandIndex < CorruptedPetalStrandCount)
			{
				continue;
			}
			FilamentPositions.Add(Petal.LocalPosition);
			FilamentWidths.Add(Petal.RibbonWidthCm * (bTelegraphing ? 0.35f : 1.0f));
			FilamentLinkOrders.Add(Petal.LinkOrder);
		}
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayPosition(
			PatternVFX, TEXT("FilamentPosition"), FilamentPositions);
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayFloat(
			PatternVFX, TEXT("FilamentWidth"), FilamentWidths);
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayFloat(
			PatternVFX, TEXT("FilamentLinkOrder"), FilamentLinkOrders);
	}

	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayPosition(PatternVFX, TEXT("BeamPosition"), BeamPositions);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(PatternVFX, TEXT("BeamRotation"), BeamRotations);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayFloat(PatternVFX, TEXT("BeamInnerWidth"), InnerWidths);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayFloat(PatternVFX, TEXT("BeamOuterWidth"), OuterWidths);
	PatternVFX->SetVariableFloat(TEXT("User.ElapsedSeconds"), ElapsedSeconds);
	PatternVFX->SetVariableFloat(TEXT("User.TelegraphAlpha"), TelegraphAlpha);
	PatternVFX->SetVariableFloat(TEXT("User.TelegraphPulse"), TelegraphPulse);
	PatternVFX->SetVariableBool(TEXT("User.IsTelegraphing"), bTelegraphing);
	PatternVFX->SetVariableBool(TEXT("User.IsActive"), bActive);
	if (!PatternVFX->IsActive() && PatternVFX->GetAsset())
	{
		PatternVFX->Activate(true);
	}
}

float ACorruptedActinoPatternActor::GetServerWorldTimeSeconds() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0f);
}

void ACorruptedActinoPatternActor::ApplyDamageForServer(float ElapsedSeconds)
{
	DamageAttemptCountForTest = 0;
	UBossPatternComponent* PatternComponent = GetOwner()
		? GetOwner()->FindComponentByClass<UBossPatternComponent>()
		: nullptr;
	UWorld* World = GetWorld();
	if (!PatternComponent || !World)
	{
		return;
	}

	for (TActorIterator<ADrone> It(World); It; ++It)
	{
		ADrone* Drone = *It;
		if (!Drone || Drone->IsActorBeingDestroyed())
		{
			continue;
		}

		for (int32 Index = 0; Index < Config.LaserCount; ++Index)
		{
			if (IsPointInsideLaser(Drone->GetActorLocation(), GetActorTransform(), Config.Presets[Index], ElapsedSeconds, Config))
			{
				++DamageAttemptCountForTest;
				PatternComponent->TryApplyPatternDamageForServer(Drone, PatternConfig.CorruptedDamage);
				break;
			}
		}
	}
}

void ACorruptedActinoPatternActor::DrawDebugPattern(float ElapsedSeconds) const
{
	const bool bTelegraphing = GetPatternState().LifecycleState == EBossPatternLifecycleState::Telegraphing;
	for (int32 Index = 0; Index < Config.LaserCount; ++Index)
	{
		const FCorruptedActinoLaserPreset& Preset = Config.Presets[Index];
		const float AngleRadians = FMath::DegreesToRadians(EvaluateAngleDegrees(Preset, ElapsedSeconds, Config));
		const FVector LocalDirection(FMath::Cos(AngleRadians), FMath::Sin(AngleRadians), 0.0f);
		const FVector LocalRight(-LocalDirection.Y, LocalDirection.X, 0.0f);
		const FVector LocalStart = LocalDirection * Config.StartRadiusCm
			+ FVector::UpVector * EvaluateZCm(Preset, ElapsedSeconds, Config);
		const FVector LocalEnd = LocalStart + LocalDirection * Config.LengthCm;
		const FTransform& BossStartTransform = GetActorTransform();
		const FVector StartWorld = BossStartTransform.TransformPosition(LocalStart);
		const FVector EndWorld = BossStartTransform.TransformPosition(LocalEnd);
		const FVector RightWorld = BossStartTransform.TransformVectorNoScale(LocalRight).GetSafeNormal();

		const FColor SectorColor = bTelegraphing
			? FColor(255, 196, 0, 96)
			: FColor(255, 32, 32, 112);
		DrawFilledTrapezoid(
			StartWorld,
			EndWorld,
			RightWorld,
			Config.InnerVisualFullWidthCm * 0.5f,
			Config.OuterVisualFullWidthCm * 0.5f,
			SectorColor);
	}
}

void ACorruptedActinoPatternActor::DrawFilledTrapezoid(
	const FVector& StartCenter,
	const FVector& EndCenter,
	const FVector& RightWorld,
	float InnerHalfWidthCm,
	float OuterHalfWidthCm,
	const FColor& Color) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const TArray<FVector> Vertices = {
		StartCenter - RightWorld * InnerHalfWidthCm,
		EndCenter - RightWorld * OuterHalfWidthCm,
		EndCenter + RightWorld * OuterHalfWidthCm,
		StartCenter + RightWorld * InnerHalfWidthCm,
	};
	const TArray<int32> Indices = {
		0, 1, 2, 0, 2, 3,
		0, 2, 1, 0, 3, 2,
	};
	DrawDebugMesh(World, Vertices, Indices, Color, false, 0.0f, 1);
}

#if WITH_DEV_AUTOMATION_TESTS
int32 ACorruptedActinoPatternActor::GetDamageAttemptCountForTest() const
{
	return DamageAttemptCountForTest;
}

void ACorruptedActinoPatternActor::RefreshPatternVFXForTest(float ElapsedSeconds)
{
	RefreshPatternVFX(ElapsedSeconds);
}
#endif
