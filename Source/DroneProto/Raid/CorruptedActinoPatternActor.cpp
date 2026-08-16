#include "CorruptedActinoPatternActor.h"

#include "BossPatternComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Drone.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
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
			BeamRenderer->SetVisibility(true, true);
		}
	}
	// 위치는 Position 배열로 보낸다. UE 5는 Position과 Vector를 별도 타입으로 취급하므로
	// Vector로 보내면 Niagara에서 Convert 노드를 한 겹 끼워야 하고 LWC 경고가 따라붙는다.
	// 회전값은 위치가 아니므로 Vector 그대로 둔다.
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
