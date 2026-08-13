#include "CorruptedActinoPatternActor.h"

#include "BossPatternComponent.h"
#include "Components/SceneComponent.h"
#include "Drone.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "NiagaraComponent.h"

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
