#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TutorialDebris.generated.h"

class APawn;
class USphereComponent;

UCLASS()
class DRONEPROTO_API ATutorialDebris : public AActor
{
	GENERATED_BODY()

public:
	ATutorialDebris();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Tutorial|Debris")
	bool ApplyTutorialHitForServer(AController* InstigatorController);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Tutorial|Debris")
	void SetTargetPawnForServer(APawn* InTargetPawn);

	UFUNCTION(BlueprintPure, Category = "Tutorial|Debris")
	int32 GetTutorialHitCount() const { return TutorialHitCount; }

	UFUNCTION(BlueprintPure, Category = "Tutorial|Debris")
	bool IsTutorialDebrisDestroyed() const { return bTutorialDebrisDestroyed; }

	UFUNCTION(BlueprintPure, Category = "Tutorial|Debris")
	APawn* GetTargetPawn() const { return TargetPawn; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tutorial|Debris")
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tutorial|Debris", meta = (ClampMin = "1"))
	int32 RequiredHitCount = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tutorial|Debris", meta = (ClampMin = "0.0", Units = "cm/s"))
	float ApproachSpeed = 150.0f;

	UFUNCTION(BlueprintImplementableEvent, Category = "Tutorial|Debris")
	void BP_OnTutorialDebrisHit(int32 HitCount, int32 RequiredHits);

	UFUNCTION(BlueprintImplementableEvent, Category = "Tutorial|Debris")
	void BP_OnTutorialDebrisDestroyed();

private:
	UPROPERTY(Replicated)
	int32 TutorialHitCount = 0;

	UPROPERTY(Replicated)
	bool bTutorialDebrisDestroyed = false;

	UPROPERTY()
	TObjectPtr<APawn> TargetPawn;
};
