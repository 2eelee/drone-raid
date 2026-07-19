#include "CorruptedActinoPatternActor.h"

#include "BossPatternComponent.h"
#include "Components/SceneComponent.h"
#include "Drone.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"

ACorruptedActinoPatternActor::ACorruptedActinoPatternActor()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	PrimaryActorTick.bCanEverTick = true;
}

void ACorruptedActinoPatternActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

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
		DrawDebugPattern(ElapsedSeconds);
	}
}

float ACorruptedActinoPatternActor::EvaluateAngleDegrees(
	const FCorruptedActinoLaserPreset& Preset,
	float ElapsedSeconds)
{
	const FCorruptedActinoConfig CanonicalConfig;
	const float Phase = TWO_PI * ElapsedSeconds / CanonicalConfig.XYPeriodSeconds + Preset.XYPhaseRadians;
	return Preset.BaseAngleDegrees + FMath::Sin(Phase) * CanonicalConfig.XYAmplitudeDegrees;
}

float ACorruptedActinoPatternActor::EvaluateZCm(
	const FCorruptedActinoLaserPreset& Preset,
	float ElapsedSeconds)
{
	const FCorruptedActinoConfig CanonicalConfig;
	const float Phase = TWO_PI * ElapsedSeconds / CanonicalConfig.ZPeriodSeconds + Preset.ZPhaseRadians;
	return FMath::Sin(Phase) * CanonicalConfig.ZAmplitudeCm;
}

bool ACorruptedActinoPatternActor::IsPointInsideLaser(
	const FVector& PointWorld,
	const FTransform& BossTransform,
	const FCorruptedActinoLaserPreset& Preset,
	float ElapsedSeconds)
{
	const FCorruptedActinoConfig CanonicalConfig;
	const FVector PointLocal = BossTransform.InverseTransformPosition(PointWorld);
	const float AngleRadians = FMath::DegreesToRadians(EvaluateAngleDegrees(Preset, ElapsedSeconds));
	const FVector Direction(FMath::Cos(AngleRadians), FMath::Sin(AngleRadians), 0.0f);
	const FVector Right(-Direction.Y, Direction.X, 0.0f);
	const float LongitudinalCm = FVector::DotProduct(PointLocal, Direction) - CanonicalConfig.StartRadiusCm;
	if (LongitudinalCm < -KINDA_SMALL_NUMBER || LongitudinalCm > CanonicalConfig.LengthCm + KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float Progress = FMath::Clamp(LongitudinalCm / CanonicalConfig.LengthCm, 0.0f, 1.0f);
	const float CollisionHalfWidthCm = 0.5f * FMath::Lerp(
		CanonicalConfig.InnerCollisionFullWidthCm,
		CanonicalConfig.OuterCollisionFullWidthCm,
		Progress);
	const float LateralCm = FVector::DotProduct(PointLocal, Right);
	const float VerticalOffsetCm = PointLocal.Z - EvaluateZCm(Preset, ElapsedSeconds);
	return FMath::Abs(LateralCm) <= CollisionHalfWidthCm + KINDA_SMALL_NUMBER
		&& FMath::Abs(VerticalOffsetCm) <= CanonicalConfig.CollisionFullHeightCm * 0.5f + KINDA_SMALL_NUMBER;
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
			if (IsPointInsideLaser(Drone->GetActorLocation(), GetActorTransform(), Config.Presets[Index], ElapsedSeconds))
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
		const float AngleRadians = FMath::DegreesToRadians(EvaluateAngleDegrees(Preset, ElapsedSeconds));
		const FVector LocalDirection(FMath::Cos(AngleRadians), FMath::Sin(AngleRadians), 0.0f);
		const FVector LocalRight(-LocalDirection.Y, LocalDirection.X, 0.0f);
		const FVector LocalStart = LocalDirection * Config.StartRadiusCm
			+ FVector::UpVector * EvaluateZCm(Preset, ElapsedSeconds);
		const FVector LocalEnd = LocalStart + LocalDirection * Config.LengthCm;
		const FTransform& BossStartTransform = GetActorTransform();
		const FVector StartWorld = BossStartTransform.TransformPosition(LocalStart);
		const FVector EndWorld = BossStartTransform.TransformPosition(LocalEnd);
		const FVector RightWorld = BossStartTransform.TransformVectorNoScale(LocalRight).GetSafeNormal();

		if (bTelegraphing)
		{
			DrawDashedDebugLine(StartWorld, EndWorld, FColor::Yellow, 10.0f);
			DrawTrapezoid(
				StartWorld,
				EndWorld,
				RightWorld,
				Config.InnerVisualFullWidthCm * 0.5f,
				Config.OuterVisualFullWidthCm * 0.5f,
				FColor::Yellow);
			continue;
		}

		DrawDashedDebugLine(StartWorld, EndWorld, FColor::Red, 12.0f);
		DrawTrapezoid(
			StartWorld,
			EndWorld,
			RightWorld,
			Config.InnerCollisionFullWidthCm * 0.5f,
			Config.OuterCollisionFullWidthCm * 0.5f,
			FColor::Cyan);
		DrawTrapezoid(
			StartWorld,
			EndWorld,
			RightWorld,
			Config.InnerVisualFullWidthCm * 0.5f,
			Config.OuterVisualFullWidthCm * 0.5f,
			FColor::Magenta);
	}
}

void ACorruptedActinoPatternActor::DrawTrapezoid(
	const FVector& StartCenter,
	const FVector& EndCenter,
	const FVector& RightWorld,
	float InnerHalfWidthCm,
	float OuterHalfWidthCm,
	const FColor& Color) const
{
	const FVector StartLeft = StartCenter - RightWorld * InnerHalfWidthCm;
	const FVector StartRight = StartCenter + RightWorld * InnerHalfWidthCm;
	const FVector EndLeft = EndCenter - RightWorld * OuterHalfWidthCm;
	const FVector EndRight = EndCenter + RightWorld * OuterHalfWidthCm;
	DrawDashedDebugLine(StartLeft, EndLeft, Color, 8.0f);
	DrawDashedDebugLine(StartRight, EndRight, Color, 8.0f);
	DrawDashedDebugLine(StartLeft, StartRight, Color, 8.0f);
	DrawDashedDebugLine(EndLeft, EndRight, Color, 8.0f);
}

#if WITH_DEV_AUTOMATION_TESTS
int32 ACorruptedActinoPatternActor::GetDamageAttemptCountForTest() const
{
	return DamageAttemptCountForTest;
}
#endif
