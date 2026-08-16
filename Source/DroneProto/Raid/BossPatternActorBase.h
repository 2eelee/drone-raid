#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BossPatternTypes.h"
#include "BossPatternActorBase.generated.h"

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
};
