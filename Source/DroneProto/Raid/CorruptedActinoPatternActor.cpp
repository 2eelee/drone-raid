#include "CorruptedActinoPatternActor.h"

#include "BossPatternComponent.h"
#include "Components/SceneComponent.h"
#include "Drone.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"

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
	if (!HasResolvedConfigSnapshot())
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
	TArray<FCorruptedBeamVisualSample> Result;
	Result.Reserve(InConfig.LaserCount);
	for (int32 Index = 0; Index < InConfig.LaserCount; ++Index)
	{
		const FCorruptedActinoLaserPreset& Preset = InConfig.Presets[Index];
		FCorruptedBeamVisualSample& Sample = Result.AddDefaulted_GetRef();
		Sample.AngleDegrees = EvaluateAngleDegrees(Preset, ElapsedSeconds, InConfig);
		Sample.ZCm = EvaluateZCm(Preset, ElapsedSeconds, InConfig);
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
		return;
	}

	const TArray<FCorruptedBeamVisualSample> VisualSamples = BuildVisualSamples(ElapsedSeconds, bTelegraphing, Config);
	TArray<FVector> BeamPositions;
	TArray<FVector> BeamRotations;
	TArray<float> InnerWidths;
	TArray<float> OuterWidths;
	BeamPositions.Reserve(VisualSamples.Num());
	BeamRotations.Reserve(VisualSamples.Num());
	InnerWidths.Reserve(VisualSamples.Num());
	OuterWidths.Reserve(VisualSamples.Num());
	for (const FCorruptedBeamVisualSample& Sample : VisualSamples)
	{
		const float AngleRadians = FMath::DegreesToRadians(Sample.AngleDegrees);
		const FVector Direction(FMath::Cos(AngleRadians), FMath::Sin(AngleRadians), 0.0f);
		BeamPositions.Add(Direction * (Sample.StartRadiusCm + Sample.LengthCm * 0.5f) + FVector::UpVector * Sample.ZCm);
		BeamRotations.Add(FVector(0.0f, Sample.AngleDegrees, 0.0f));
		InnerWidths.Add(Sample.InnerVisualFullWidthCm);
		OuterWidths.Add(Sample.OuterVisualFullWidthCm);
	}
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(PatternVFX, TEXT("BeamPosition"), BeamPositions);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(PatternVFX, TEXT("BeamRotation"), BeamRotations);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayFloat(PatternVFX, TEXT("BeamInnerWidth"), InnerWidths);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayFloat(PatternVFX, TEXT("BeamOuterWidth"), OuterWidths);
	PatternVFX->SetVariableFloat(TEXT("User.ElapsedSeconds"), ElapsedSeconds);
	PatternVFX->SetVariableFloat(TEXT("User.TelegraphAlpha"), bTelegraphing ? 1.0f : 0.0f);
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
#endif
