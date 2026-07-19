#include "StellarRemnantPatternActor.h"

#include "BossPatternComponent.h"
#include "Components/SceneComponent.h"
#include "Drone.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"

AStellarRemnantPatternActor::AStellarRemnantPatternActor()
	: Samples(BuildLogicalSamples())
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	PrimaryActorTick.bCanEverTick = true;
}

void AStellarRemnantPatternActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

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

TArray<FStellarRemnantSample> AStellarRemnantPatternActor::BuildLogicalSamples()
{
	const FStellarRemnantConfig CanonicalConfig;
	const FBossPatternConfig CommonConfig;
	TArray<FStellarRemnantSample> Result;
	Result.Reserve(CanonicalConfig.DamageProjectileCount + CanonicalConfig.VisualProjectileCount);
	for (int32 WaveIndex = 0; WaveIndex < CanonicalConfig.WaveCount; ++WaveIndex)
	{
		const float StartTimeSeconds = WaveIndex * CanonicalConfig.WaveIntervalSeconds;
		const float DamageAngleOffset = WaveIndex == 0 ? 0.0f : CanonicalConfig.SecondWaveOffsetDegrees;
		for (int32 Index = 0; Index < CanonicalConfig.DamageProjectilesPerWave; ++Index)
		{
			FStellarRemnantSample& Sample = Result.AddDefaulted_GetRef();
			Sample.WaveIndex = WaveIndex;
			Sample.AngleDegrees = Index * CanonicalConfig.DamageAngleStepDegrees + DamageAngleOffset;
			Sample.StartTimeSeconds = StartTimeSeconds;
			Sample.Damage = CommonConfig.StellarDamage;
		}
		for (int32 Index = 0; Index < CanonicalConfig.VisualProjectilesPerWave; ++Index)
		{
			FStellarRemnantSample& Sample = Result.AddDefaulted_GetRef();
			Sample.WaveIndex = WaveIndex;
			Sample.AngleDegrees = Index * 45.0f + (WaveIndex == 0 ? 11.25f : 22.5f);
			Sample.StartTimeSeconds = StartTimeSeconds;
			Sample.Damage = CanonicalConfig.VisualDamage;
			Sample.bVisualOnly = true;
		}
	}
	return Result;
}

bool AStellarRemnantPatternActor::IsSampleActive(
	const FStellarRemnantSample& Sample,
	float ElapsedSeconds)
{
	const FStellarRemnantConfig CanonicalConfig;
	return ElapsedSeconds >= Sample.StartTimeSeconds
		&& ElapsedSeconds <= Sample.StartTimeSeconds + CanonicalConfig.TravelSeconds;
}

FVector AStellarRemnantPatternActor::EvaluateLocalPosition(
	const FStellarRemnantSample& Sample,
	float ElapsedSeconds)
{
	const FStellarRemnantConfig CanonicalConfig;
	const float TravelElapsedSeconds = FMath::Clamp(
		ElapsedSeconds - Sample.StartTimeSeconds,
		0.0f,
		CanonicalConfig.TravelSeconds);
	const float RadiusCm = CanonicalConfig.StartRadiusCm
		+ TravelElapsedSeconds * CanonicalConfig.SpeedCmPerSecond;
	const float AngleRadians = FMath::DegreesToRadians(Sample.AngleDegrees);
	return FVector(FMath::Cos(AngleRadians), FMath::Sin(AngleRadians), 0.0f) * RadiusCm;
}

bool AStellarRemnantPatternActor::IsPointInsideSweptSample(
	const FVector& PointWorld,
	const FTransform& BossTransform,
	const FStellarRemnantSample& Sample,
	float PreviousElapsedSeconds,
	float CurrentElapsedSeconds)
{
	if (Sample.bVisualOnly || CurrentElapsedSeconds < PreviousElapsedSeconds)
	{
		return false;
	}

	const FStellarRemnantConfig CanonicalConfig;
	const float SweepStartTime = FMath::Max(PreviousElapsedSeconds, Sample.StartTimeSeconds);
	const float SweepEndTime = FMath::Min(
		CurrentElapsedSeconds,
		Sample.StartTimeSeconds + CanonicalConfig.TravelSeconds);
	if (SweepStartTime > SweepEndTime)
	{
		return false;
	}

	const FVector SegmentStart = BossTransform.TransformPosition(EvaluateLocalPosition(Sample, SweepStartTime));
	const FVector SegmentEnd = BossTransform.TransformPosition(EvaluateLocalPosition(Sample, SweepEndTime));
	const FVector Segment = SegmentEnd - SegmentStart;
	const float SegmentSizeSquared = Segment.SizeSquared();
	const float Alpha = SegmentSizeSquared > SMALL_NUMBER
		? FMath::Clamp(FVector::DotProduct(PointWorld - SegmentStart, Segment) / SegmentSizeSquared, 0.0f, 1.0f)
		: 0.0f;
	const FVector ClosestPoint = SegmentStart + Segment * Alpha;
	return FVector::DistSquared(PointWorld, ClosestPoint)
		<= FMath::Square(CanonicalConfig.CollisionRadiusCm + KINDA_SMALL_NUMBER);
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

		for (const FStellarRemnantSample& Sample : Samples)
		{
			if (IsPointInsideSweptSample(
				Drone->GetActorLocation(),
				GetActorTransform(),
				Sample,
				PreviousElapsedSeconds,
				CurrentElapsedSeconds))
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
			const FVector StartWorld = GetActorTransform().TransformPosition(EvaluateLocalPosition(Sample, Sample.StartTimeSeconds));
			const FVector EndWorld = GetActorTransform().TransformPosition(
				EvaluateLocalPosition(Sample, Sample.StartTimeSeconds + Config.TravelSeconds));
			DrawDebugLine(GetWorld(), StartWorld, EndWorld, FColor::Yellow, false, 0.0f, 0, 1.5f);
		}
		return;
	}

	for (const FStellarRemnantSample& Sample : Samples)
	{
		if (!IsSampleActive(Sample, ElapsedSeconds))
		{
			continue;
		}
		const FVector PositionWorld = GetActorTransform().TransformPosition(
			EvaluateLocalPosition(Sample, ElapsedSeconds));
		DrawDebugSphere(
			GetWorld(),
			PositionWorld,
			Sample.bVisualOnly ? 50.0f : Config.CollisionRadiusCm,
			8,
			Sample.bVisualOnly ? FColor::Purple : FColor::Red,
			false,
			0.0f,
			0,
			1.5f);
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
