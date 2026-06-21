#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RaidBoss.generated.h"

UCLASS()
class DRONEPROTO_API ARaidBoss : public AActor
{
	GENERATED_BODY()

public:
	ARaidBoss();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Raid|Boss")
	void ApplyDamageForServer(float DamageAmount, AController* InstigatorController, AActor* DamageCauser);

	UFUNCTION(BlueprintPure, Category = "Raid|Boss")
	float GetCurrentHP() const;

	UFUNCTION(BlueprintPure, Category = "Raid|Boss")
	float GetMaxHP() const;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Raid|Boss", meta = (AllowPrivateAccess = "true"))
	float MaxHP = 1000.0f;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentHP, VisibleInstanceOnly, BlueprintReadOnly, Category = "Raid|Boss", meta = (AllowPrivateAccess = "true"))
	float CurrentHP = 1000.0f;

	UFUNCTION()
	void OnRep_CurrentHP();
};
