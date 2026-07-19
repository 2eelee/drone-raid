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

	const FBossPatternRepState& GetPatternState() const;

	UFUNCTION(BlueprintNativeEvent, Category = "Raid|Boss|Pattern|Visual")
	void BP_OnPatternVisualChanged(const FBossPatternRepState& NewState);

	UFUNCTION(BlueprintNativeEvent, Category = "Raid|Boss|Pattern|Visual")
	void BP_OnPatternVisualEnded();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(ReplicatedUsing = OnRep_PatternState)
	FBossPatternRepState PatternState;

	UFUNCTION()
	void OnRep_PatternState();

	void NotifyVisualChanged();
};
