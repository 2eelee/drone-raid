#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputActionValue.h"
#include "Drone.generated.h"

class UFloatingPawnMovement;
class UInputMappingContext;
class UInputAction;
class UDronePart;
class ARaidBoss;

UCLASS()
class DRONEPROTO_API ADrone : public APawn
{
	GENERATED_BODY()

public:
	ADrone();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Drone|Loadout")
	bool ApplyLoadout(FName CorePartID, FName LeftWeaponPartID, FName RightWeaponPartID);

	UFUNCTION(BlueprintCallable, Category = "Drone|Combat")
	void RequestAttackBoss();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Drone|Combat")
	void ApplyDamageForServer(int32 DamageAmount, FName Reason);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Drone|Combat")
	bool HealForServer(int32 HealAmount);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Drone|Combat")
	bool RequestDodgeForServer();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Drone|Combat")
	void ResetCombatRuntimeStateForServer();

	UFUNCTION(BlueprintPure, Category = "Drone|Stats")
	int32 GetHealth() const;

	UFUNCTION(BlueprintPure, Category = "Drone|Stats")
	int32 GetMaxHealth() const;

	UFUNCTION(BlueprintPure, Category = "Drone|Stats")
	int32 GetAttackPower() const;

	UFUNCTION(BlueprintPure, Category = "Drone|Stats")
	bool IsDead() const;

#if WITH_DEV_AUTOMATION_TESTS
	int32 GetPulseAttackCountForTest(bool bIsLeftWeapon) const;
	float GetHealthValueForTest() const;
	bool ApplyMoveInputForServerForTest(FVector2D RawAxis);
	void UpdateMoveDistanceForServerForTest(float DeltaSeconds);
	void ResetMoveDistanceForServerForTest(FName Reason);
	void ResetVectorMoveDistanceForServerForTest(FName Reason);
	float GetVectorAccumulatedMoveDistanceForTest() const;
	float GetBoosterAccumulatedMoveDistanceForTest() const;
	FVector2D GetLastServerMoveInputForTest() const;
#endif

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
	                         AController* EventInstigator, AActor* DamageCauser) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(VisibleAnywhere)
	UFloatingPawnMovement* FloatingMovement;

	// D6~8 발사 위치용 — 지금은 배치만
	UPROPERTY(VisibleAnywhere)
	USceneComponent* MuzzlePoint;

	// ---- Replicated Stats (부품 합산 결과만 복제) ----
	UPROPERTY(ReplicatedUsing = OnRep_Health)
	float Health = 100.0f;

	UPROPERTY(Replicated)
	int32 MaxHealth = 100;

	UPROPERTY(Replicated)
	int32 AttackPower = 0;

	UPROPERTY(ReplicatedUsing = OnRep_IsDead)
	bool bIsDead = false;

	UFUNCTION()
	void OnRep_Health();

	UFUNCTION()
	void OnRep_IsDead();

	// ---- Enhanced Input ----
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputMappingContext* DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* MoveAction;

	UFUNCTION(Server, Reliable)
	void Server_RequestAttackBoss();

	UFUNCTION(Server, Unreliable)
	void Server_SetMoveInput(FVector2D RawAxis);

	void Move(const FInputActionValue& Value);

	// ---- 장착 부품 (서버 전용, 복제 안 함) ----
	UPROPERTY(Transient)
	TArray<TObjectPtr<UDronePart>> EquippedParts;

	UPROPERTY(Transient)
	FName EquippedCorePartID = NAME_None;

	UPROPERTY(Transient)
	FName EquippedLeftWeaponPartID = NAME_None;

	UPROPERTY(Transient)
	FName EquippedRightWeaponPartID = NAME_None;

	UPROPERTY(Transient)
	int32 LeftPulseAttackCount = 0;

	UPROPERTY(Transient)
	int32 RightPulseAttackCount = 0;

	UPROPERTY(Transient)
	float VectorAccumulatedMoveDistanceMeters = 0.0f;

	UPROPERTY(Transient)
	float BoosterAccumulatedMoveDistanceMeters = 0.0f;

	UPROPERTY(Transient)
	FVector LastMoveDistanceLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	bool bHasMoveDistanceSample = false;

	UPROPERTY(Transient)
	FVector2D LastServerMoveInput = FVector2D::ZeroVector;

	UPROPERTY(Transient)
	float LastMoveInputSummaryLogTime = -1000.0f;

	UPROPERTY(Transient)
	FName LastMoveInputSummaryResult = NAME_None;

	UPROPERTY(Transient)
	FName LastMoveInputSummaryReason = NAME_None;

	UPROPERTY(Transient)
	float LastMoveDistanceSummaryLogTime = -1000.0f;

	UPROPERTY(Transient)
	float LastMoveDistanceIgnoredLogTime = -1000.0f;

	UPROPERTY(Transient)
	FName LastMoveDistanceIgnoredReason = NAME_None;

	void ServerEquipPart(TSubclassOf<UDronePart> PartClass);
	void RecalculateStats();
	void HandleDeath();
	void HandleAttackBossForServer();
	void ClearEquippedPartsForServer();
	void LogDeadInputIgnored(const TCHAR* ActionName) const;
	FVector2D ClampMoveInputAxisForServer(FVector2D RawAxis, bool& bOutWasClamped) const;
	bool ApplyMoveInputForServer(FVector2D RawAxis);
	void LogMoveInputSummary(const TCHAR* Result, const TCHAR* Reason, const FVector2D& Axis);
	void UpdateMoveDistanceForServer(float DeltaSeconds);
	void ResetMoveDistanceForServer(FName Reason);
	void ResetVectorMoveDistanceForServer(FName Reason);
	bool CanAccumulateMoveDistanceForServer(FName& OutIgnoreReason) const;
	void LogMoveDistanceIgnored(FName Reason);
	float CalculateWeaponDamageForServer(FName WeaponPartID, bool bIsLeftWeapon);
	float GetCoreAttackModifierForServer(FName CorePartID) const;
	float GetCoreBonusAttackModifierForServer(FName CorePartID) const;
	ARaidBoss* FindRaidBossForServer() const;
	void ApplyDrainHealForServer(float DamageDealt);
	void ResetCombatRuntimeStateForReason(FName Reason);
};
