#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputActionValue.h"
#include "Raid/DroneCombatTypes.h"
#include "Drone.generated.h"

class UFloatingPawnMovement;
class UInputMappingContext;
class UInputAction;
class UDronePart;
class ARaidBoss;

USTRUCT()
struct FDroneOwnerMoveSync
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	UPROPERTY()
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY()
	uint32 Sequence = 0;
};

UCLASS()
class DRONEPROTO_API ADrone : public APawn
{
	GENERATED_BODY()

public:
	ADrone();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Drone|Loadout")
	bool ApplyLoadout(FName CorePartID, FName LeftWeaponPartID, FName RightWeaponPartID);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Drone|Loadout")
	void ClearEquippedLoadoutForServer(FName Reason);

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

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Drone|Report")
	FDroneCombatRecord GetCombatRecordForServer() const;

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
	bool ApplyPendingServerMoveInputForTest(float DeltaSeconds);
	void UpdateMoveDistanceForServerForTest(float DeltaSeconds);
	void ResetMoveDistanceForServerForTest(FName Reason);
	void ResetVectorMoveDistanceForServerForTest(FName Reason);
	float GetVectorAccumulatedMoveDistanceForTest() const;
	float GetBoosterAccumulatedMoveDistanceForTest() const;
	FDroneCombatRecord GetCombatRecordForTest() const;
	FVector2D GetLastServerMoveInputForTest() const;
	FName GetEquippedCorePartIDForTest() const;
	FName GetEquippedLeftWeaponPartIDForTest() const;
	FName GetEquippedRightWeaponPartIDForTest() const;
	bool HasEquippedLoadoutForTest() const;
#endif

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
	                         AController* EventInstigator, AActor* DamageCauser) override;
	virtual void OnRep_ReplicatedMovement() override;
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

	UPROPERTY(ReplicatedUsing = OnRep_OwnerMoveSync)
	FDroneOwnerMoveSync OwnerMoveSync;

	UFUNCTION()
	void OnRep_Health();

	UFUNCTION()
	void OnRep_IsDead();

	UFUNCTION()
	void OnRep_OwnerMoveSync();

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
	float LastServerMoveAppliedSummaryLogTime = -1000.0f;

	UPROPERTY(Transient)
	float LastReplicatedLocationSummaryLogTime = -1000.0f;

	UPROPERTY(Transient)
	float LastOwnerMoveSyncSentLogTime = -1000.0f;

	UPROPERTY(Transient)
	float LastOwnerMoveCorrectedLogTime = -1000.0f;

	UPROPERTY(Transient)
	float LastMoveDistanceSummaryLogTime = -1000.0f;

	UPROPERTY(Transient)
	float LastMoveDistanceIgnoredLogTime = -1000.0f;

	UPROPERTY(Transient)
	FName LastMoveDistanceIgnoredReason = NAME_None;

	UPROPERTY(Transient)
	FDroneCombatRecord CombatRecord;

	UPROPERTY(Transient)
	bool bCombatRecordActive = false;

	UPROPERTY(Transient)
	float BaseMoveSpeedCmPerSecond = 0.0f;

	void ServerEquipPart(TSubclassOf<UDronePart> PartClass);
	void RecalculateStats();
	void HandleDeath();
	void HandleAttackBossForServer();
	void LogDeadInputIgnored(const TCHAR* ActionName) const;
	FVector2D ClampMoveInputAxisForServer(FVector2D RawAxis, bool& bOutWasClamped) const;
	bool ApplyMoveInputForServer(FVector2D RawAxis);
	bool ApplyPendingServerMoveInputForServer(float DeltaSeconds);
	void UpdateOwnerMoveSyncForServer(const FVector& ServerLocation, const FRotator& ServerRotation);
	void LogMoveInputSummary(const TCHAR* Result, const TCHAR* Reason, const FVector2D& Axis);
	void UpdateMoveDistanceForServer(float DeltaSeconds);
	void ResetMoveDistanceForServer(FName Reason);
	void ResetVectorMoveDistanceForServer(FName Reason);
	bool CanAccumulateMoveDistanceForServer(FName& OutIgnoreReason) const;
	void LogMoveDistanceIgnored(FName Reason);
	FDroneWeaponCalculationResult CalculateWeaponDamageForServer(FName WeaponPartID, bool bIsLeftWeapon);
	FDroneCoreCalculationResult CalculateCoreForServer(FName CorePartID) const;
	EDroneCombatWeaponType ResolveWeaponTypeForServer(FName WeaponPartID) const;
	EDroneCombatCoreType ResolveCoreTypeForServer(FName CorePartID) const;
	ARaidBoss* FindRaidBossForServer() const;
	float ApplyDrainHealForServer(float DamageDealt);
	void StartCombatRecordForServer();
	FDroneCombatRecord BuildCombatRecordSnapshotForServer() const;
	void AddBossDamageToCombatRecordForServer(float DamageDealt, const ARaidBoss* Boss);
	void AddHealToCombatRecordForServer(float HealAmount);
	void AddMoveDistanceToCombatRecordForServer(float DeltaMeters);
	void MarkCombatRecordEndedForServer();
	void LogCombatRecordForServer() const;
	void RefreshMoveSpeedForServer();
	void ResetCombatRuntimeStateForReason(FName Reason);
};
