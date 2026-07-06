#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RaidBoss.generated.h"

class ARaidBossAttackTelegraph;
class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS()
class DRONEPROTO_API ARaidBoss : public AActor
{
	GENERATED_BODY()

public:
	ARaidBoss();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Raid|Boss")
	void ApplyDamageForServer(float DamageAmount, AController* InstigatorController, AActor* DamageCauser);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Raid|Boss|Debug")
	int32 PerformDebugAreaAttackForServer(FVector AttackCenter, float RadiusCm = 300.0f, int32 DamageAmount = 25);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Raid|Boss|Debug")
	bool StartDebugTelegraphedAreaAttackForServer(FVector AttackCenter, float RadiusCm = 300.0f, int32 DamageAmount = 25, float TelegraphSeconds = 1.0f);

	UFUNCTION(BlueprintPure, Category = "Raid|Boss")
	float GetCurrentHP() const;

	UFUNCTION(BlueprintPure, Category = "Raid|Boss")
	float GetMaxHP() const;

	UFUNCTION(BlueprintPure, Category = "Raid|Boss")
	bool IsDefeated() const;

	UFUNCTION(BlueprintPure, Category = "Raid|Boss|Visual")
	bool IsVisualReadyForCamera() const;

	UFUNCTION(BlueprintNativeEvent, Category = "Raid|Boss|Visual")
	void BP_OnBossDamagedVisual(float Damage, float OldHP, float NewHP, AActor* DamageCauser);

#if WITH_DEV_AUTOMATION_TESTS
	int32 GetCombatVisualBossDamagedCountForTest() const;
	float GetLastCombatVisualBossDamageForTest() const;
	float GetLastCombatVisualBossOldHPForTest() const;
	float GetLastCombatVisualBossNewHPForTest() const;
	FString GetPrototypeVisualLabelTextForTest() const;
#endif

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Raid|Boss|Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> PrototypeVisualMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Raid|Boss|Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTextRenderComponent> PrototypeVisualLabel = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Raid|Boss|Visual", meta = (AllowPrivateAccess = "true", ClampMin = "50.0", UIMin = "50.0"))
	float PrototypeVisualRadiusCm = 220.0f;

	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadOnly, Category = "Raid|Boss", meta = (AllowPrivateAccess = "true"))
	float MaxHP = 1000.0f;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentHP, VisibleInstanceOnly, BlueprintReadOnly, Category = "Raid|Boss", meta = (AllowPrivateAccess = "true"))
	float CurrentHP = 1000.0f;

	UFUNCTION()
	void OnRep_CurrentHP();

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayBossDamagedVisual(float Damage, float OldHP, float NewHP, AActor* DamageCauser);

	void ApplyPrototypeVisualSettings();
	void RefreshPrototypeVisualHPText();
	void PlayBossDamagedVisualLocally(float Damage, float OldHP, float NewHP, AActor* DamageCauser);
	void ExecuteDebugTelegraphedAreaAttackForServer(FVector AttackCenter, float RadiusCm, int32 DamageAmount, ARaidBossAttackTelegraph* TelegraphActor);

#if WITH_DEV_AUTOMATION_TESTS
	int32 CombatVisualBossDamagedCountForTest = 0;
	float LastCombatVisualBossDamageForTest = 0.0f;
	float LastCombatVisualBossOldHPForTest = 0.0f;
	float LastCombatVisualBossNewHPForTest = 0.0f;
#endif
};
