#include "BossPatternActorBase.h"

#include "BossPatternComponent.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "Components/AudioComponent.h"
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
	// 패턴 종료·중단·보스 사망·레이드 종료가 전부 이 액터의 파괴로 수렴한다.
	// 종료 사유마다 따로 훅을 걸 필요 없이 여기 한 곳에서 루프를 끊는다.
	StopPatternActiveLoop();
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
		// BP 훅이 아니라 여기서 낸다. `BP_OnPatternVisualChanged`는 BlueprintNativeEvent라
		// 파생 BP가 연출을 붙이려고 오버라이드하는 순간 사운드가 조용히 사라진다.
		UpdatePatternAudioLocally(PatternState);
	}
}

void ABossPatternActorBase::UpdatePatternAudioLocally(const FBossPatternRepState& NewState)
{
	// 복제는 같은 값으로 다시 도착할 수 있다. 그때마다 예고음이 겹치면
	// 가이드의 "state transition당 정확히 1회"가 깨진다.
	if (bHasAppliedPatternAudioState
		&& AppliedPatternAudioInstanceID == NewState.InstanceID
		&& AppliedPatternAudioLifecycle == NewState.LifecycleState)
	{
		return;
	}
	bHasAppliedPatternAudioState = true;
	AppliedPatternAudioInstanceID = NewState.InstanceID;
	AppliedPatternAudioLifecycle = NewState.LifecycleState;

	if (NewState.LifecycleState == EBossPatternLifecycleState::Telegraphing)
	{
		PlayPattern2DSound(SFX_PatternTelegraph);
		return;
	}

	PlayPattern2DSound(SFX_PatternStart);

	if (!SFX_PatternActiveLoop || PatternActiveLoopAudioComponent)
	{
		return;
	}

	// 3D Boss. 패턴 액터는 보스 위치에 스폰되므로 액터에 붙이면 그대로 보스 소리가 된다.
	// bAutoDestroy = false — 핸들을 들고 있어야 `EndPlay`에서 끊을 수 있다.
	PatternActiveLoopAudioComponent = UGameplayStatics::SpawnSoundAttached(
		SFX_PatternActiveLoop,
		GetRootComponent(),
		NAME_None,
		FVector::ZeroVector,
		EAttachLocation::KeepRelativeOffset,
		false,
		1.0f,
		1.0f,
		0.0f,
		PatternActiveLoopAttenuation,
		nullptr,
		false);
}

void ABossPatternActorBase::PlayPattern2DSound(USoundBase* Sound, float PitchMultiplier) const
{
	// 에셋 미지정은 정상 상태다. 배선이 끝나지 않은 소리를 로그로 시끄럽게 만들지 않는다.
	if (!Sound || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// 가이드가 예고·시작·웨이브를 "2D Global"로 지정했다. 예고를 놓치면 회피할 수 없으므로
	// 보스와의 거리에 따라 작아지면 안 된다.
	UGameplayStatics::PlaySound2D(this, Sound, 1.0f, PitchMultiplier);
}

void ABossPatternActorBase::StopPatternActiveLoop()
{
	if (PatternActiveLoopAudioComponent)
	{
		PatternActiveLoopAudioComponent->Stop();
		PatternActiveLoopAudioComponent = nullptr;
	}
}

void ABossPatternActorBase::BP_OnPatternVisualChanged_Implementation(const FBossPatternRepState& NewState)
{
	(void)NewState;
}

void ABossPatternActorBase::BP_OnPatternVisualEnded_Implementation()
{
}
