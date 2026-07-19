#include "BossPatternActorBase.h"

#include "DrawDebugHelpers.h"
#include "Net/UnrealNetwork.h"

namespace
{
constexpr float DebugDashLengthCm = 150.0f;
constexpr float DebugDashGapCm = 100.0f;
}

ABossPatternActorBase::ABossPatternActorBase()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
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
		DrawDebugLine(World, DashStart, DashEnd, Color, false, 0.0f, 0, Thickness);
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
