#include "BossPatternActorBase.h"

#include "BossPatternComponent.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

namespace
{
constexpr float DebugDashLengthCm = 100.0f;
constexpr float DebugDashGapCm = 300.0f;
constexpr float DebugPrimitiveLifetimeSeconds = 0.12f;
constexpr uint8 DebugForegroundDepthPriority = 1;
}

ABossPatternActorBase::ABossPatternActorBase()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
}

void ABossPatternActorBase::BeginPlay()
{
	Super::BeginPlay();
	TryAcquireResolvedConfigSnapshot();
}

bool ABossPatternActorBase::TryAcquireResolvedConfigSnapshot()
{
	if (bHasResolvedConfigSnapshot)
	{
		return true;
	}

	// 클라이언트에서는 Owner(보스)가 아직 복제되지 않았을 수 있다. BeginPlay 1회만 시도하면
	// 그 인스턴스는 config를 영영 못 받고 Tick이 계속 조기 반환해 아무것도 렌더되지 않는다.
	// 예고는 1초뿐이라 이 창을 놓치면 예고가 통째로 화면에서 사라진다.
	const UBossPatternComponent* Component = GetOwner()
		? GetOwner()->FindComponentByClass<UBossPatternComponent>()
		: nullptr;
	FBossPatternResolvedConfig LocalConfig;
	if (Component && Component->CopyResolvedConfig(LocalConfig))
	{
		SnapshotResolvedConfig(LocalConfig);
		return true;
	}
	return false;
}

void ABossPatternActorBase::SnapshotResolvedConfig(const FBossPatternResolvedConfig& Config)
{
	if (bHasResolvedConfigSnapshot)
	{
		return;
	}
	ResolvedConfigSnapshot = Config;
	bHasResolvedConfigSnapshot = true;
	OnResolvedConfigSnapshot();
}

bool ABossPatternActorBase::HasResolvedConfigSnapshot() const
{
	return bHasResolvedConfigSnapshot;
}

bool ABossPatternActorBase::CopyResolvedConfigSnapshot(FBossPatternResolvedConfig& OutConfig) const
{
	if (!bHasResolvedConfigSnapshot)
	{
		return false;
	}
	OutConfig = ResolvedConfigSnapshot;
	return true;
}

const FBossPatternResolvedConfig& ABossPatternActorBase::GetResolvedConfigSnapshot() const
{
	return ResolvedConfigSnapshot;
}

void ABossPatternActorBase::OnResolvedConfigSnapshot()
{
}

void ABossPatternActorBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABossPatternActorBase, PatternState);
}

void ABossPatternActorBase::InitializeForServer(
	EBossPatternKind PatternKind,
	EBossPatternLifecycleState LifecycleState,
	int32 InstanceID,
	float StartServerTime)
{
	if (!HasAuthority())
	{
		return;
	}
	if (UWorld* World = GetWorld(); InstanceID > 0 && World)
	{
		for (TActorIterator<ABossPatternActorBase> It(World); It; ++It)
		{
			ABossPatternActorBase* ExistingActor = *It;
			if (ExistingActor != this
				&& !ExistingActor->IsActorBeingDestroyed()
				&& ExistingActor->PatternState.InstanceID == InstanceID)
			{
				Destroy();
				return;
			}
		}
	}

	PatternState.InstanceID = InstanceID;
	PatternState.PatternKind = PatternKind;
	PatternState.LifecycleState = LifecycleState;
	PatternState.StartServerTime = StartServerTime;
	ForceNetUpdate();
	NotifyVisualChanged();
}

void ABossPatternActorBase::SetLifecycleForServer(EBossPatternLifecycleState LifecycleState, float StartServerTime)
{
	if (!HasAuthority())
	{
		return;
	}

	PatternState.LifecycleState = LifecycleState;
	PatternState.StartServerTime = StartServerTime;
	ForceNetUpdate();
	NotifyVisualChanged();
}

const FBossPatternRepState& ABossPatternActorBase::GetPatternState() const
{
	return PatternState;
}

void ABossPatternActorBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetNetMode() != NM_DedicatedServer)
	{
		BP_OnPatternVisualEnded();
	}
	Super::EndPlay(EndPlayReason);
}

void ABossPatternActorBase::DrawDashedDebugLine(
	const FVector& Start,
	const FVector& End,
	const FColor& Color,
	float Thickness) const
{
	UWorld* World = GetWorld();
	const FVector Delta = End - Start;
	const float Length = Delta.Size();
	if (!World || Length <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector Direction = Delta / Length;
	for (float Offset = 0.0f; Offset < Length; Offset += DebugDashLengthCm + DebugDashGapCm)
	{
		const FVector DashStart = Start + Direction * Offset;
		const FVector DashEnd = Start + Direction * FMath::Min(Offset + DebugDashLengthCm, Length);
		DrawDebugLine(
			World,
			DashStart,
			DashEnd,
			Color,
			false,
			DebugPrimitiveLifetimeSeconds,
			DebugForegroundDepthPriority,
			Thickness);
	}
}

void ABossPatternActorBase::DrawForegroundDebugSphere(
	const FVector& Center,
	float Radius,
	int32 Segments,
	const FColor& Color,
	float Thickness) const
{
	if (UWorld* World = GetWorld())
	{
		DrawDebugSphere(
			World,
			Center,
			Radius,
			Segments,
			Color,
			false,
			DebugPrimitiveLifetimeSeconds,
			DebugForegroundDepthPriority,
			Thickness);
	}
}

void ABossPatternActorBase::OnRep_PatternState()
{
	NotifyVisualChanged();
}

void ABossPatternActorBase::NotifyVisualChanged()
{
	if (GetNetMode() != NM_DedicatedServer)
	{
		BP_OnPatternVisualChanged(PatternState);
	}
}

void ABossPatternActorBase::BP_OnPatternVisualChanged_Implementation(const FBossPatternRepState& NewState)
{
	(void)NewState;
}

void ABossPatternActorBase::BP_OnPatternVisualEnded_Implementation()
{
}
