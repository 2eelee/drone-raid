#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BossPatternTypes.h"
#include "BossPatternActorBase.generated.h"

class USoundBase;
class USoundAttenuation;
class UAudioComponent;

UCLASS()
class DRONEPROTO_API ABossPatternActorBase : public AActor
{
	GENERATED_BODY()

public:
	ABossPatternActorBase();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void InitializeForServer(EBossPatternKind PatternKind, EBossPatternLifecycleState LifecycleState, int32 InstanceID, float StartServerTime);
	void SetLifecycleForServer(EBossPatternLifecycleState LifecycleState, float StartServerTime);
	void SnapshotResolvedConfig(const FBossPatternResolvedConfig& Config);
	bool HasResolvedConfigSnapshot() const;
	bool CopyResolvedConfigSnapshot(FBossPatternResolvedConfig& OutConfig) const;

	const FBossPatternRepState& GetPatternState() const;

	UFUNCTION(BlueprintNativeEvent, Category = "Raid|Boss|Pattern|Visual")
	void BP_OnPatternVisualChanged(const FBossPatternRepState& NewState);

	UFUNCTION(BlueprintNativeEvent, Category = "Raid|Boss|Pattern|Visual")
	void BP_OnPatternVisualEnded();

protected:
	virtual void BeginPlay() override;
	bool TryAcquireResolvedConfigSnapshot();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ---- 패턴 SFX ----
	// 파생 패턴 액터의 Blueprint에서 지정한다. 비워두면 그 소리만 조용히 건너뛴다.
	//
	// 예고음이 "레이드 최초 Actino에는 울리면 안 된다"는 가이드 규칙은 여기서 따로 막지 않는다.
	// `UBossPatternComponent`가 `FirstDelay`에서 `BeginActiveForServer()`로 직행해
	// 첫 패턴은 Telegraphing 상태 자체를 거치지 않기 때문이다.

	/** Telegraphing 진입 시 1회. 2D Global — 예고는 위치와 무관하게 모두에게 같은 크기로 들려야 한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Raid|Boss|Pattern|Audio")
	TObjectPtr<USoundBase> SFX_PatternTelegraph = nullptr;

	/** Active 진입 시 1회. 2D Global. 레이저 4개처럼 개별 요소마다 울리지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "Raid|Boss|Pattern|Audio")
	TObjectPtr<USoundBase> SFX_PatternStart = nullptr;

	/** Active 동안 1개만 도는 루프. 패턴 종료·중단·보스 사망 어느 쪽이든 `EndPlay`에서 멈춘다. */
	UPROPERTY(EditDefaultsOnly, Category = "Raid|Boss|Pattern|Audio")
	TObjectPtr<USoundBase> SFX_PatternActiveLoop = nullptr;

	/** 루프의 거리 감쇠. 가이드가 Laser Loop을 3D Boss로 지정했다. */
	UPROPERTY(EditDefaultsOnly, Category = "Raid|Boss|Pattern|Audio")
	TObjectPtr<USoundAttenuation> PatternActiveLoopAttenuation = nullptr;

	// 클라이언트 표현 전용. 서버에서는 아무것도 하지 않는다.
	void PlayPattern2DSound(USoundBase* Sound, float PitchMultiplier = 1.0f) const;
	void StopPatternActiveLoop();
	const FBossPatternResolvedConfig& GetResolvedConfigSnapshot() const;
	virtual void OnResolvedConfigSnapshot();
	void DrawDashedDebugLine(
		const FVector& Start,
		const FVector& End,
		const FColor& Color,
		float Thickness) const;
	void DrawForegroundDebugSphere(
		const FVector& Center,
		float Radius,
		int32 Segments,
		const FColor& Color,
		float Thickness) const;

private:
	UPROPERTY(ReplicatedUsing = OnRep_PatternState)
	FBossPatternRepState PatternState;
	FBossPatternResolvedConfig ResolvedConfigSnapshot;
	bool bHasResolvedConfigSnapshot = false;

	UFUNCTION()
	void OnRep_PatternState();

	void NotifyVisualChanged();

	// 상태가 실제로 바뀐 경우에만 소리를 낸다. 복제는 같은 값으로 다시 도착할 수 있고,
	// 그때마다 예고음이 겹치면 가이드의 "state transition당 정확히 1회"가 깨진다.
	void UpdatePatternAudioLocally(const FBossPatternRepState& NewState);

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> PatternActiveLoopAudioComponent = nullptr;

	bool bHasAppliedPatternAudioState = false;
	EBossPatternLifecycleState AppliedPatternAudioLifecycle = EBossPatternLifecycleState::Telegraphing;
	int32 AppliedPatternAudioInstanceID = 0;
};
