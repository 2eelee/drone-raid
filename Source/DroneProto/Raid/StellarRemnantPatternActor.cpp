#include "StellarRemnantPatternActor.h"

#include "BossPatternComponent.h"
#include "Components/SceneComponent.h"
#include "Drone.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"

AStellarRemnantPatternActor::AStellarRemnantPatternActor()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	PrimaryActorTick.bCanEverTick = true;
}

void AStellarRemnantPatternActor::OnResolvedConfigSnapshot()
{
	Config = GetResolvedConfigSnapshot().Stellar;
	PatternConfig = GetResolvedConfigSnapshot().Common;
	Samples = BuildLogicalSamples(Config, PatternConfig);
}

void AStellarRemnantPatternActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasResolvedConfigSnapshot())
	{
		return;
	}

	const FBossPatternRepState& State = GetPatternState();
	if (State.PatternKind != EBossPatternKind::StellarRemnant)
	{
		return;
	}

	const float ElapsedSeconds = FMath::Max(0.0f, GetServerWorldTimeSeconds() - State.StartServerTime);
	if (HasAuthority() && State.LifecycleState == EBossPatternLifecycleState::Active)
	{
		if (!bHasPreviousActiveTime || ElapsedSeconds < PreviousActiveElapsedSeconds)
		{
			PreviousActiveElapsedSeconds = 0.0f;
			bHasPreviousActiveTime = true;
		}
		ApplyDamageForServer(PreviousActiveElapsedSeconds, ElapsedSeconds);
		PreviousActiveElapsedSeconds = ElapsedSeconds;
	}
	else
	{
		bHasPreviousActiveTime = false;
	}

	if (GetNetMode() != NM_DedicatedServer)
	{
		DrawDebugPattern(ElapsedSeconds);
	}
}

TArray<FStellarRemnantSample> AStellarRemnantPatternActor::BuildLogicalSamples(
	const FStellarRemnantConfig& InConfig,
	const FBossPatternConfig& InPatternConfig)
{
	TArray<FStellarRemnantSample> Result;
	Result.Reserve(InConfig.DamageProjectileCount + InConfig.VisualProjectileCount);
	for (int32 WaveIndex = 0; WaveIndex < InConfig.WaveCount; ++WaveIndex)
	{
		const float StartTimeSeconds = WaveIndex * InConfig.WaveIntervalSeconds;
		const float DamageAngleOffset = WaveIndex == 0 ? 0.0f : InConfig.SecondWaveOffsetDegrees;
		for (int32 Index = 0; Index < InConfig.DamageProjectilesPerWave; ++Index)
		{
			FStellarRemnantSample& Sample = Result.AddDefaulted_GetRef();
			Sample.WaveIndex = WaveIndex;
			Sample.AngleDegrees = Index * InConfig.DamageAngleStepDegrees + DamageAngleOffset;
			Sample.StartTimeSeconds = StartTimeSeconds;
			Sample.Damage = InPatternConfig.StellarDamage;
		}
		for (int32 Index = 0; Index < InConfig.VisualProjectilesPerWave; ++Index)
		{
			FStellarRemnantSample& Sample = Result.AddDefaulted_GetRef();
			Sample.WaveIndex = WaveIndex;
			Sample.AngleDegrees = Index * 45.0f + (WaveIndex == 0 ? 11.25f : 22.5f);
			Sample.StartTimeSeconds = StartTimeSeconds;
			Sample.Damage = InConfig.VisualDamage;
			Sample.bVisualOnly = true;
			Sample.VisualZOffsetCm = (Index + WaveIndex) % 2 == 0
				? InConfig.VisualZOffsetCm
				: -InConfig.VisualZOffsetCm;
			const float SizeAlpha = InConfig.VisualProjectilesPerWave > 1
				? static_cast<float>(Index) / static_cast<float>(InConfig.VisualProjectilesPerWave - 1)
				: 0.0f;
			Sample.VisualFullSizeCm = FMath::Lerp(
				InConfig.VisualFullSizeMinCm,
				InConfig.VisualFullSizeMaxCm,
				SizeAlpha);
		}
	}
	return Result;
}

bool AStellarRemnantPatternActor::IsSampleActive(
	const FStellarRemnantSample& Sample,
	float ElapsedSeconds,
	const FStellarRemnantConfig& InConfig)
{
	return ElapsedSeconds >= Sample.StartTimeSeconds
		&& ElapsedSeconds <= Sample.StartTimeSeconds + InConfig.TravelSeconds;
}

FVector AStellarRemnantPatternActor::EvaluateLocalPosition(
	const FStellarRemnantSample& Sample,
	float ElapsedSeconds,
	const FStellarRemnantConfig& InConfig)
{
	const float TravelElapsedSeconds = FMath::Clamp(
		ElapsedSeconds - Sample.StartTimeSeconds,
		0.0f,
		InConfig.TravelSeconds);
	const float RadiusCm = InConfig.StartRadiusCm
		+ TravelElapsedSeconds * InConfig.SpeedCmPerSecond;
	const float AngleRadians = FMath::DegreesToRadians(Sample.AngleDegrees);
	return FVector(FMath::Cos(AngleRadians), FMath::Sin(AngleRadians), 0.0f) * RadiusCm
		+ FVector::UpVector * Sample.VisualZOffsetCm;
}

bool AStellarRemnantPatternActor::IsPointInsideSweptSample(
	const FVector& PointWorld,
	const FTransform& BossTransform,
	const FStellarRemnantSample& Sample,
	float PreviousElapsedSeconds,
	float CurrentElapsedSeconds,
	const FStellarRemnantConfig& InConfig,
	float TargetRadiusCm)
{
	if (Sample.bVisualOnly || CurrentElapsedSeconds < PreviousElapsedSeconds)
	{
		return false;
	}

	const float SweepStartTime = FMath::Max(PreviousElapsedSeconds, Sample.StartTimeSeconds);
	const float SweepEndTime = FMath::Min(
		CurrentElapsedSeconds,
		Sample.StartTimeSeconds + InConfig.TravelSeconds);
	if (SweepStartTime > SweepEndTime)
	{
		return false;
	}

	const FVector SegmentStart = BossTransform.TransformPosition(EvaluateLocalPosition(Sample, SweepStartTime, InConfig));
	const FVector SegmentEnd = BossTransform.TransformPosition(EvaluateLocalPosition(Sample, SweepEndTime, InConfig));
	const FVector Segment = SegmentEnd - SegmentStart;
	const float SegmentSizeSquared = Segment.SizeSquared();
	const float Alpha = SegmentSizeSquared > SMALL_NUMBER
		? FMath::Clamp(FVector::DotProduct(PointWorld - SegmentStart, Segment) / SegmentSizeSquared, 0.0f, 1.0f)
		: 0.0f;
	const FVector ClosestPoint = SegmentStart + Segment * Alpha;
	const float CombinedRadiusCm = InConfig.CollisionRadiusCm + FMath::Max(0.0f, TargetRadiusCm);
	return FVector::DistSquared(PointWorld, ClosestPoint)
		<= FMath::Square(CombinedRadiusCm + KINDA_SMALL_NUMBER);
}

float AStellarRemnantPatternActor::GetServerWorldTimeSeconds() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0f);
}

void AStellarRemnantPatternActor::ApplyDamageForServer(
	float PreviousElapsedSeconds,
	float CurrentElapsedSeconds)
{
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

		FVector TargetOrigin = Drone->GetActorLocation();
		FVector TargetExtent = FVector::ZeroVector;
		Drone->GetActorBounds(false, TargetOrigin, TargetExtent);
		const float TargetRadiusCm = TargetExtent.Size();

		for (const FStellarRemnantSample& Sample : Samples)
		{
			if (IsPointInsideSweptSample(
				TargetOrigin,
				GetActorTransform(),
				Sample,
				PreviousElapsedSeconds,
				CurrentElapsedSeconds,
				Config,
				TargetRadiusCm))
			{
				PatternComponent->TryApplyPatternDamageForServer(Drone, PatternConfig.StellarDamage);
				break;
			}
		}
	}
}

void AStellarRemnantPatternActor::DrawDebugPattern(float ElapsedSeconds) const
{
	const bool bTelegraphing = GetPatternState().LifecycleState == EBossPatternLifecycleState::Telegraphing;
	if (bTelegraphing)
	{
		for (const FStellarRemnantSample& Sample : Samples)
		{
			if (Sample.bVisualOnly)
			{
				continue;
			}
			const FVector StartWorld = GetActorTransform().TransformPosition(EvaluateLocalPosition(Sample, Sample.StartTimeSeconds, Config));
			const FVector EndWorld = GetActorTransform().TransformPosition(
				EvaluateLocalPosition(Sample, Sample.StartTimeSeconds + Config.TravelSeconds, Config));
			DrawDashedDebugLine(StartWorld, EndWorld, FColor::Yellow, 8.0f);
		}
		return;
	}

	for (const FStellarRemnantSample& Sample : Samples)
	{
		if (!IsSampleActive(Sample, ElapsedSeconds, Config))
		{
			continue;
		}
		const FVector PositionWorld = GetActorTransform().TransformPosition(
			EvaluateLocalPosition(Sample, ElapsedSeconds, Config));
		DrawForegroundDebugSphere(
			PositionWorld,
			Sample.bVisualOnly ? Sample.VisualFullSizeCm * 0.5f : Config.CollisionRadiusCm,
			12,
			Sample.bVisualOnly ? FColor::Purple : FColor::Red,
			6.0f);
	}
}

#if WITH_DEV_AUTOMATION_TESTS
void AStellarRemnantPatternActor::ApplyDamageForServerForTest(
	float PreviousElapsedSeconds,
	float CurrentElapsedSeconds)
{
	ApplyDamageForServer(PreviousElapsedSeconds, CurrentElapsedSeconds);
}

int32 AStellarRemnantPatternActor::GetLogicalSampleCountForTest() const
{
	return Samples.Num();
}
#endif
