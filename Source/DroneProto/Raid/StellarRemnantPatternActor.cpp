#include "StellarRemnantPatternActor.h"

#include "BossPatternComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Drone.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AStellarRemnantPatternActor::AStellarRemnantPatternActor()
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
		TEXT("/Game/VFX/Boss/Telegraph01/NS_StellarRemnant.NS_StellarRemnant"));
	if (PatternVFXAsset.Succeeded())
	{
		PatternVFX->SetAsset(PatternVFXAsset.Object);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ShardMeshAsset(
		TEXT("/Game/VFX/Boss/Telegraph01/SM_StellarShard.SM_StellarShard"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CoreMeshAsset(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShardMaterialAsset(
		TEXT("/Game/VFX/Boss/Telegraph01/M_StellarShard.M_StellarShard"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CoreMaterialAsset(
		TEXT("/Game/VFX/Boss/Telegraph01/M_StellarCore.M_StellarCore"));

	DamageShardRenderer = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("DamageShardRenderer"));
	DamageShardRenderer->SetupAttachment(Root);
	DamageShardRenderer->SetStaticMesh(ShardMeshAsset.Object);
	DamageShardRenderer->SetMaterial(0, ShardMaterialAsset.Object);
	DamageShardRenderer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DamageShardRenderer->SetGenerateOverlapEvents(false);
	DamageShardRenderer->SetCanEverAffectNavigation(false);
	DamageShardRenderer->CastShadow = false;

	VisualShardRenderer = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("VisualShardRenderer"));
	VisualShardRenderer->SetupAttachment(Root);
	VisualShardRenderer->SetStaticMesh(ShardMeshAsset.Object);
	VisualShardRenderer->SetMaterial(0, ShardMaterialAsset.Object);
	VisualShardRenderer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualShardRenderer->SetGenerateOverlapEvents(false);
	VisualShardRenderer->SetCanEverAffectNavigation(false);
	VisualShardRenderer->CastShadow = false;

	CoreRenderer = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoreRenderer"));
	CoreRenderer->SetupAttachment(Root);
	CoreRenderer->SetStaticMesh(CoreMeshAsset.Object);
	CoreRenderer->SetMaterial(0, CoreMaterialAsset.Object);
	CoreRenderer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CoreRenderer->SetGenerateOverlapEvents(false);
	CoreRenderer->SetCanEverAffectNavigation(false);
	CoreRenderer->CastShadow = false;
	CoreRenderer->SetVisibility(false, true);
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
		RefreshPatternVFX(ElapsedSeconds);
		if (bEnableDebugVisualization)
		{
			DrawDebugPattern(ElapsedSeconds);
		}
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

TArray<FStellarRemnantVisualFrame> AStellarRemnantPatternActor::BuildVisualFrames(
	float ElapsedSeconds,
	const FStellarRemnantConfig& InConfig,
	const FBossPatternConfig& InPatternConfig)
{
	const TArray<FStellarRemnantSample> LogicalSamples = BuildLogicalSamples(InConfig, InPatternConfig);
	TArray<FStellarRemnantVisualFrame> Result;
	Result.Reserve(LogicalSamples.Num());
	for (const FStellarRemnantSample& Sample : LogicalSamples)
	{
		FStellarRemnantVisualFrame& Frame = Result.AddDefaulted_GetRef();
		Frame.Position = EvaluateLocalPosition(Sample, ElapsedSeconds, InConfig);
		Frame.AngleDegrees = Sample.AngleDegrees;
		Frame.SizeCm = Sample.bVisualOnly ? Sample.VisualFullSizeCm : InConfig.CollisionRadiusCm * 2.0f;
		Frame.WaveIndex = Sample.WaveIndex;
		Frame.bActive = IsSampleActive(Sample, ElapsedSeconds, InConfig);
		Frame.bVisualOnly = Sample.bVisualOnly;
	}
	return Result;
}

FStellarTelegraphVisualFrame AStellarRemnantPatternActor::BuildTelegraphVisualFrame(
	float ElapsedSeconds,
	float TelegraphDurationSeconds)
{
	FStellarTelegraphVisualFrame Result;
	const float SafeDuration = FMath::Max(TelegraphDurationSeconds, KINDA_SMALL_NUMBER);
	Result.GatherAlpha = FMath::Clamp(ElapsedSeconds / SafeDuration, 0.0f, 1.0f);
	Result.CoreIntensity = FMath::Square(Result.GatherAlpha);
	return Result;
}

void AStellarRemnantPatternActor::RefreshPatternVFX(float ElapsedSeconds)
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
		DamageShardRenderer->ClearInstances();
		VisualShardRenderer->ClearInstances();
		CoreRenderer->SetVisibility(false, true);
		return;
	}

	TArray<FVector> Positions;
	TArray<float> Angles;
	TArray<float> Sizes;
	TArray<bool> ActiveMask;
	TArray<bool> VisualOnlyMask;
	TArray<int32> WaveIndices;
	const TArray<FStellarRemnantVisualFrame> Frames = BuildVisualFrames(ElapsedSeconds, Config, PatternConfig);
	Positions.Reserve(Frames.Num());
	Angles.Reserve(Frames.Num());
	Sizes.Reserve(Frames.Num());
	ActiveMask.Reserve(Frames.Num());
	VisualOnlyMask.Reserve(Frames.Num());
	WaveIndices.Reserve(Frames.Num());
	DamageShardRenderer->ClearInstances();
	VisualShardRenderer->ClearInstances();
	DamageShardRenderer->SetScalarParameterValueOnMaterials(TEXT("VFXIntensity"), 24.0f);
	DamageShardRenderer->SetScalarParameterValueOnMaterials(TEXT("VFXOpacity"), 0.88f);
	VisualShardRenderer->SetScalarParameterValueOnMaterials(TEXT("VFXIntensity"), 9.0f);
	VisualShardRenderer->SetScalarParameterValueOnMaterials(TEXT("VFXOpacity"), 0.42f);
	for (const FStellarRemnantVisualFrame& Frame : Frames)
	{
		Positions.Add(Frame.Position);
		Angles.Add(Frame.AngleDegrees);
		Sizes.Add(Frame.SizeCm);
		ActiveMask.Add(Frame.bActive && bActive);
		VisualOnlyMask.Add(Frame.bVisualOnly);
		WaveIndices.Add(Frame.WaveIndex);
		if (Frame.bActive && bActive)
		{
			const float UniformScale = FMath::Max(0.15f, Frame.SizeCm / 220.0f);
			const FTransform InstanceTransform(
				FRotator(0.0f, Frame.AngleDegrees, 0.0f),
				Frame.Position,
				FVector(UniformScale));
			(Frame.bVisualOnly ? VisualShardRenderer : DamageShardRenderer)->AddInstance(InstanceTransform);
		}
	}
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(PatternVFX, TEXT("ProjectilePosition"), Positions);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayFloat(PatternVFX, TEXT("ProjectileAngle"), Angles);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayFloat(PatternVFX, TEXT("ProjectileSize"), Sizes);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayBool(PatternVFX, TEXT("ProjectileActive"), ActiveMask);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayBool(PatternVFX, TEXT("ProjectileVisualOnly"), VisualOnlyMask);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayInt32(PatternVFX, TEXT("ProjectileWave"), WaveIndices);
	const FStellarTelegraphVisualFrame TelegraphFrame = BuildTelegraphVisualFrame(
		ElapsedSeconds,
		PatternConfig.StellarTelegraphSeconds);
	PatternVFX->SetVariableFloat(TEXT("User.ElapsedSeconds"), ElapsedSeconds);
	PatternVFX->SetVariableFloat(TEXT("User.GatherAlpha"), bTelegraphing ? TelegraphFrame.GatherAlpha : 0.0f);
	PatternVFX->SetVariableFloat(TEXT("User.CoreIntensity"), bTelegraphing ? TelegraphFrame.CoreIntensity : 1.0f);
	PatternVFX->SetVariableBool(TEXT("User.IsActive"), bActive);
	if (bTelegraphing)
	{
		const float CoreScale = FMath::Lerp(0.5f, 2.2f, TelegraphFrame.GatherAlpha);
		CoreRenderer->SetRelativeScale3D(FVector(CoreScale));
		CoreRenderer->SetScalarParameterValueOnMaterials(
			TEXT("VFXIntensity"), FMath::Lerp(4.0f, 32.0f, TelegraphFrame.CoreIntensity));
		CoreRenderer->SetVisibility(true, true);
	}
	else
	{
		CoreRenderer->SetVisibility(false, true);
	}
	if (!PatternVFX->IsActive() && PatternVFX->GetAsset())
	{
		PatternVFX->Activate(true);
	}
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

void AStellarRemnantPatternActor::RefreshPatternVFXForTest(float ElapsedSeconds)
{
	RefreshPatternVFX(ElapsedSeconds);
}
#endif
