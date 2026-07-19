#include "BossPatternActorBase.h"

#include "Net/UnrealNetwork.h"

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
