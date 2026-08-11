#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputActionValue.h"
#include "Raid/DroneCombatTypes.h"
#include "Raid/DroneCombatDataTableResolver.h"
#include "TimerManager.h"
#include "Drone.generated.h"

class UFloatingPawnMovement;
class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class UDronePart;
class ARaidBoss;
class UWorld;
class UPrimitiveComponent;
class UDataTable;

USTRUCT(BlueprintType)
struct FDroneCombatCameraView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Camera")
	FVector CameraLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Camera")
	FRotator CameraRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Camera")
	bool bBossValid = false;
};

USTRUCT(BlueprintType)
struct FDroneCombatCameraTarget
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Camera")
	TObjectPtr<ARaidBoss> Boss = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Camera")
	FName Source = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Camera")
	FName InvalidReason = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Camera")
	bool bValid = false;
};

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

class ATutorialPlayerController;

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

	UFUNCTION(BlueprintCallable, Category = "Drone|Combat")
	void RequestDodge(FVector2D RawDirection);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Drone|Combat")
	void ApplyDamageForServer(int32 DamageAmount, FName Reason);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Drone|Combat")
	bool HealForServer(int32 HealAmount);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Drone|Combat")
	bool RequestDodgeForServer(FVector2D RawDirection = FVector2D::ZeroVector);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Drone|Combat")
	void CancelDodgeForServer(FName Reason);

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
	bool IsInvincibleForDamage() const;

	UFUNCTION(BlueprintPure, Category = "Drone|Movement")
	float GetAccumulatedMoveDistance() const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Drone|Movement")
	void ResetAccumulatedMoveDistanceForServer();

	UFUNCTION(BlueprintPure, Category = "Drone|Movement")
	float GetCurrentMoveSpeed() const;

	UFUNCTION(BlueprintPure, Category = "Drone|Dodge")
	bool IsDodging() const;

	UFUNCTION(BlueprintPure, Category = "Drone|Dodge")
	float GetDodgeCooldownRemaining() const;

	static bool CalculateFixedBossFacingQuarterView(
		const FVector& PlayerPosition,
		const FVector* BossPosition,
		FVector FallbackForwardDirection,
		float CameraDistanceCm,
		float CameraHeightCm,
		float FallbackPitchDegrees,
		FDroneCombatCameraView& OutCameraView);

	static bool ShouldApplyFixedBossFacingCamera(
		bool bPawnLocallyControlled,
		bool bControllerIsPlayerController,
		bool bPlayerControllerIsLocal);

	static FVector2D ConvertScreenInputToWorldMoveDirection(
		FVector2D RawAxis,
		FRotator CameraRotation);

	static bool ResolveFixedBossFacingCameraTargetForWorld(UWorld* World, FDroneCombatCameraTarget& OutTarget);
	static bool IsValidFixedBossFacingCameraTarget(const ARaidBoss* Boss, FName& OutInvalidReason);
	static FVector BuildFixedBossFacingFallbackForward(float FallbackYawDegrees);
	static float ResolveFixedBossFacingFallbackYaw(
		bool& bHasCachedYaw,
		float& InOutCachedYaw,
		float CandidateYawDegrees,
		float DefaultYawDegrees);
	static bool ShouldEmitThrottledSummaryLog(
		float Now,
		float LastLogTime,
		float MinIntervalSeconds,
		bool bForce);
	static bool ShouldEmitMoveAcceptedSummaryLog(
		float Now,
		float LastLogTime,
		bool bInputActive,
		bool bHasLastAxis,
		FVector2D LastAxis,
		FVector2D Axis);
	static bool IsMoveAcceptedSummaryAxisChangeSignificant(
		FVector2D PreviousAxis,
		FVector2D CurrentAxis);

	bool IsMovementAllowedForServer(FName& OutIgnoreReason) const;
	FVector ClampPositionToMovementBoundaryForServer(FVector RequestedPosition);
	FVector ClampPositionOutsideBossCenterForServer(FVector RequestedPosition);

	UFUNCTION(BlueprintImplementableEvent, Category = "Drone|Dodge")
	void BP_OnDodgeVisualStateChanged(bool bDodging);

	UFUNCTION(BlueprintNativeEvent, Category = "Drone|Dodge")
	void BP_OnDodgeInvincibleVisualChanged(bool bIsInvincibleVisual);

	UFUNCTION(BlueprintNativeEvent, Category = "Drone|Combat|Visual")
	void BP_OnDroneAttackVisual(FName LeftWeaponPartID, FName RightWeaponPartID, float Damage, FVector From, FVector To);

	UFUNCTION(BlueprintNativeEvent, Category = "Drone|Combat|Visual")
	void BP_OnDroneDamagedVisual(float Damage, float OldHP, float NewHP);

	UFUNCTION(BlueprintNativeEvent, Category = "Drone|Combat|Visual")
	void BP_OnDroneDamageIgnoredVisual(FName Reason);

#if WITH_DEV_AUTOMATION_TESTS
	int32 GetPulseAttackCountForTest(bool bIsLeftWeapon) const;
	float GetHealthValueForTest() const;
	int32 GetCombatVisualAttackCountForTest() const;
	float GetLastCombatVisualAttackDamageForTest() const;
	FVector GetLastCombatVisualAttackFromForTest() const;
	FVector GetLastCombatVisualAttackToForTest() const;
	int32 GetCombatVisualDroneDamagedCountForTest() const;
	float GetLastCombatVisualDroneDamageForTest() const;
	float GetLastCombatVisualDroneDamageOldHPForTest() const;
	float GetLastCombatVisualDroneDamageNewHPForTest() const;
	int32 GetCombatVisualDroneDamageIgnoredCountForTest() const;
	FName GetLastCombatVisualDroneDamageIgnoredReasonForTest() const;
	FName GetLastAttackNoDamageReasonForTest() const;
	FName GetLastAttackIgnoredReasonForTest() const;
	FName GetLastDodgeIgnoredReasonForTest() const;
	static float GetMoveAcceptedSummaryLogIntervalSecondsForTest();
	static float GetMoveDistanceSummaryLogIntervalSecondsForTest();
	static bool ShouldEmitMoveAcceptedSummaryLogForTest(
		float Now,
		float LastLogTime,
		bool bInputActive,
		bool bHasLastAxis,
		FVector2D LastAxis,
		FVector2D Axis);
	bool ApplyMoveInputForServerForTest(FVector2D RawAxis);
	bool ApplyPendingServerMoveInputForTest(float DeltaSeconds);
	void FinishTutorialAttackInputForTest();
	void UpdateMoveDistanceForServerForTest(float DeltaSeconds);
	void ResetMoveDistanceForServerForTest(FName Reason);
	void ResetVectorMoveDistanceForServerForTest(FName Reason);
	float GetVectorAccumulatedMoveDistanceForTest() const;
	float GetBoosterAccumulatedMoveDistanceForTest() const;
	FDroneCombatRecord GetCombatRecordForTest() const;
	FVector2D GetLastServerMoveInputForTest() const;
	bool CacheMoveInputForDodgeForTest(FVector2D RawAxis);
	FVector2D GetCachedMoveInputForDodgeForTest() const;
	void ClearMoveInputForDodgeForTest();
	bool RequestDodgeFromCurrentMoveInputForTest();
	bool IsDodgingForTest() const;
	bool IsInvincibleForTest() const;
	void TickForTest(float DeltaSeconds);
	void SetIsAttackingForTest(bool bInIsAttacking);
	void SetLastDodgeEndTimeForTest(float InLastDodgeEndTime);
	FName GetEquippedCorePartIDForTest() const;
	FName GetEquippedLeftWeaponPartIDForTest() const;
	FName GetEquippedRightWeaponPartIDForTest() const;
	bool HasEquippedLoadoutForTest() const;
	void SetCombatDataTablesForTest(UDataTable* CoreTable, UDataTable* WeaponTable);
	int32 GetCombatDataResolveCountForTest() const;
	EDroneCombatDataFallbackReason GetCombatDataFallbackReasonForTest() const;
	FDroneWeaponCalculationResult CalculateWeaponDamageForServerForTest(FName WeaponPartID, bool bIsLeftWeapon);
#endif

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
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

	UPROPERTY(VisibleAnywhere, Category = "Drone|Camera")
	USpringArmComponent* CombatCameraSpringArm;

	UPROPERTY(VisibleAnywhere, Category = "Drone|Camera")
	UCameraComponent* CombatCameraComponent;

	// ---- Replicated Stats (부품 합산 결과만 복제) ----
	UPROPERTY(ReplicatedUsing = OnRep_Health)
	float Health = 100.0f;

	UPROPERTY(Replicated)
	int32 MaxHealth = 100;

	UPROPERTY(Replicated)
	int32 AttackPower = 0;

	UPROPERTY(ReplicatedUsing = OnRep_IsDead)
	bool bIsDead = false;

	UPROPERTY(ReplicatedUsing = OnRep_IsDodging)
	bool bIsDodging = false;

	UPROPERTY(ReplicatedUsing = OnRep_OwnerMoveSync)
	FDroneOwnerMoveSync OwnerMoveSync;

	UFUNCTION()
	void OnRep_Health();

	UFUNCTION()
	void OnRep_IsDead();

	UFUNCTION()
	void OnRep_IsDodging();

	UFUNCTION()
	void OnRep_OwnerMoveSync();

	// ---- Enhanced Input ----
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputMappingContext* DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* MoveAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* DodgeAction;

	UPROPERTY(EditDefaultsOnly, Category = "Drone|Dodge", meta = (ClampMin = "0.0", Units = "cm"))
	float DodgeDistanceCm = 600.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Drone|Dodge", meta = (ClampMin = "0.0", Units = "s"))
	float DodgeDurationSeconds = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category = "Drone|Dodge", meta = (ClampMin = "0.0", Units = "s"))
	float DodgeInvincibleDurationSeconds = 0.15f;

	UPROPERTY(EditDefaultsOnly, Category = "Drone|Dodge", meta = (ClampMin = "0.0", Units = "s"))
	float DodgeCooldownSeconds = 1.20f;

	UPROPERTY(EditDefaultsOnly, Category = "Drone|Camera", meta = (ClampMin = "0.0", Units = "cm", AllowPrivateAccess = "true"))
	float CombatCameraDistanceCm = 1400.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Drone|Camera", meta = (ClampMin = "0.0", Units = "cm", AllowPrivateAccess = "true"))
	float CombatCameraHeightCm = 380.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Drone|Camera", meta = (Units = "deg", AllowPrivateAccess = "true"))
	float CombatCameraFallbackPitchDegrees = -10.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Drone|Camera", meta = (ClampMin = "1.0", ClampMax = "170.0", Units = "deg", AllowPrivateAccess = "true"))
	float CombatCameraFOV = 75.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Drone|Camera", meta = (Units = "deg", AllowPrivateAccess = "true"))
	float CombatCameraDefaultFallbackYawDegrees = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Drone|Camera", meta = (AllowPrivateAccess = "true"))
	FVector CombatCameraSocketOffsetCm = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, Category = "Drone|Camera", meta = (ClampMin = "0.0", ClampMax = "1.0", AllowPrivateAccess = "true"))
	FVector2D CombatCameraTargetScreenPosition = FVector2D(0.50f, 0.55f);

	UPROPERTY(EditDefaultsOnly, Category = "Drone|Camera", meta = (ClampMin = "0.0", ClampMax = "1.0", AllowPrivateAccess = "true"))
	FVector2D CombatCameraBossScreenYRange = FVector2D(0.30f, 0.40f);

	UFUNCTION(Server, Reliable)
	void Server_RequestAttackBoss();

	UFUNCTION(Server, Reliable)
	void Server_FinishTutorialAttackInput();

	UFUNCTION(Server, Reliable)
	void Server_FinishTutorialMoveInput();

	UFUNCTION(Server, Reliable)
	void Server_RequestDodge(FVector2D RawDirection);

	UFUNCTION(Server, Unreliable)
	void Server_SetMoveInput(FVector2D RawAxis);

	void Move(const FInputActionValue& Value);
	void Dodge(const FInputActionValue& Value);
	void FinishTutorialAttackInput();
	void ClearMoveInputForDodge(const FInputActionValue& Value);

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
	float AccumulatedMoveDistanceMeters = 0.0f;

	UPROPERTY(Transient)
	FVector LastMoveDistanceLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	bool bHasMoveDistanceSample = false;

	UPROPERTY(Transient)
	FVector2D LastServerMoveInput = FVector2D::ZeroVector;

	UPROPERTY(Transient)
	FVector2D CachedMoveInputForDodge = FVector2D::ZeroVector;

	UPROPERTY(Transient)
	FVector2D CachedRawMoveInputForDodge = FVector2D::ZeroVector;

	UPROPERTY(Transient)
	float CachedMoveInputCameraYawForDodge = 0.0f;

	UPROPERTY(Transient)
	float LastMoveInputSummaryLogTime = -1000.0f;

	UPROPERTY(Transient)
	float NextDodgeAllowedServerTime = 0.0f;

	UPROPERTY(Transient)
	bool bIsInvincible = false;

	UPROPERTY(Transient)
	bool bIsAttacking = false;

	UPROPERTY(Replicated)
	float LastDodgeEndTime = -1000.0f;

	UPROPERTY(Transient)
	FTimerHandle DodgeInvincibleTimerHandle;

	UPROPERTY(Transient)
	FTimerHandle DodgeEndTimerHandle;

	UPROPERTY(Transient)
	FVector DodgeStartLocationForServer = FVector::ZeroVector;

	UPROPERTY(Transient)
	FVector DodgeTargetLocationForServer = FVector::ZeroVector;

	UPROPERTY(Transient)
	FVector DodgeLastAppliedLocationForServer = FVector::ZeroVector;

	UPROPERTY(Transient)
	FVector DodgeDirectionForServer = FVector::ZeroVector;

	UPROPERTY(Transient)
	float DodgeElapsedSecondsForServer = 0.0f;

	UPROPERTY(Transient)
	float DodgeActiveDurationSecondsForServer = 0.0f;

	UPROPERTY(Transient)
	float DodgeAccumulatedActualDistanceMetersForServer = 0.0f;

	UPROPERTY(Transient)
	bool bDodgeInterpolationActiveForServer = false;

	TArray<TWeakObjectPtr<UPrimitiveComponent>> DodgeVisualHiddenComponents;
	bool bDodgeInvincibleVisualHidden = false;

	UPROPERTY(Transient)
	FName LastMoveInputSummaryResult = NAME_None;

	UPROPERTY(Transient)
	FName LastMoveInputSummaryReason = NAME_None;

	UPROPERTY(Transient)
	float LastServerMoveAppliedSummaryLogTime = -1000.0f;

	UPROPERTY(Transient)
	float LastMoveAcceptedSummaryLogTime = -1000.0f;

	UPROPERTY(Transient)
	FVector2D LastMoveAcceptedSummaryAxis = FVector2D::ZeroVector;

	UPROPERTY(Transient)
	bool bHasLastMoveAcceptedSummaryAxis = false;

	UPROPERTY(Transient)
	bool bMoveAcceptedSummaryInputActive = false;

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
	float LastMoveInputConvertedLogTime = -1000.0f;

	UPROPERTY(Transient)
	FVector2D LastMoveInputConvertedRawAxis = FVector2D::ZeroVector;

	UPROPERTY(Transient)
	FVector2D LastMoveInputConvertedWorldAxis = FVector2D::ZeroVector;

	UPROPERTY(Transient)
	float LastMoveInputConvertedCameraYaw = 0.0f;

	UPROPERTY(Transient)
	float LastDodgeInputConvertedLogTime = -1000.0f;

	UPROPERTY(Transient)
	FVector2D LastDodgeInputConvertedRawAxis = FVector2D::ZeroVector;

	UPROPERTY(Transient)
	FVector2D LastDodgeInputConvertedWorldAxis = FVector2D::ZeroVector;

	UPROPERTY(Transient)
	float LastDodgeInputConvertedCameraYaw = 0.0f;

	UPROPERTY(Transient)
	float LastMoveBossMinClampSummaryLogTime = -1000.0f;

	UPROPERTY(Transient)
	float LastCombatCameraAppliedSummaryLogTime = -1000.0f;

	UPROPERTY(Transient)
	float LastCombatCameraBossFacingSummaryLogTime = -1000.0f;

	UPROPERTY(Transient)
	bool bCombatCameraRotationInputDisabled = false;

	UPROPERTY(Transient)
	bool bHasLastCombatCameraBossValid = false;

	UPROPERTY(Transient)
	bool bLastCombatCameraBossValid = false;

	UPROPERTY(Transient)
	bool bHasCombatCameraFallbackYaw = false;

	UPROPERTY(Transient)
	float CombatCameraFallbackYawDegrees = 0.0f;

	UPROPERTY(Transient)
	bool bHasLastCombatCameraAppliedConfig = false;

	UPROPERTY(Transient)
	float LastCombatCameraAppliedDistanceCm = 0.0f;

	UPROPERTY(Transient)
	float LastCombatCameraAppliedHeightCm = 0.0f;

	UPROPERTY(Transient)
	float LastCombatCameraAppliedFOV = 0.0f;

	UPROPERTY(Transient)
	FVector LastCombatCameraAppliedSocketOffsetCm = FVector::ZeroVector;

	UPROPERTY(Transient)
	FVector2D LastCombatCameraAppliedTargetScreenPosition = FVector2D::ZeroVector;

	UPROPERTY(Transient)
	FVector2D LastCombatCameraAppliedBossScreenYRange = FVector2D::ZeroVector;

	UPROPERTY(Transient)
	bool bHasLastCombatCameraTargetSummary = false;

	UPROPERTY(Transient)
	TWeakObjectPtr<ARaidBoss> LastCombatCameraTargetSummaryBoss;

	UPROPERTY(Transient)
	FName LastCombatCameraTargetSummarySource = NAME_None;

	UPROPERTY(Transient)
	FName LastCombatCameraTargetSummaryReason = NAME_None;

	UPROPERTY(Transient)
	FDroneCombatRecord CombatRecord;

	UPROPERTY(Transient)
	bool bCombatRecordActive = false;

	UPROPERTY(EditDefaultsOnly, Category = "Drone|Combat|Data", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDataTable> DroneCoreDataTable = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Drone|Combat|Data", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDataTable> DroneWeaponDataTable = nullptr;

	FDroneCombatResolvedConfig CachedDroneCombatConfig;
	bool bDroneCombatConfigResolved = false;
	EDroneCombatDataFallbackReason DroneCombatDataFallbackReason = EDroneCombatDataFallbackReason::MissingCoreTable;

	UPROPERTY(EditDefaultsOnly, Category = "Drone|Movement", meta = (ClampMin = "0.0", Units = "cm/s", AllowPrivateAccess = "true"))
	float BaseMoveSpeedCmPerSecond = 450.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Drone|Movement", meta = (Units = "cm", AllowPrivateAccess = "true"))
	float FixedZPosition = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Drone|Movement", meta = (ClampMin = "0.0", Units = "cm", AllowPrivateAccess = "true"))
	float MovementBoundaryRadiusCm = 5000.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Drone|Movement", meta = (ClampMin = "0.0", Units = "cm", AllowPrivateAccess = "true"))
	float BossMinApproachDistanceCm = 800.0f;

	UPROPERTY(Transient)
	FVector MovementBoundaryCenter = FVector::ZeroVector;

	UPROPERTY(Transient)
	bool bHasMovementBoundaryCenter = false;

	void ServerEquipPart(TSubclassOf<UDronePart> PartClass);
	void RecalculateStats();
	void HandleDeath();
	void NotifyPatternPopulationDeathForServer();
	void HandleAttackBossForServer();
	bool HandleTutorialAttackForServer(ATutorialPlayerController* TutorialPlayerController);
	void RecordAttackIgnoredForServer(FName Reason);
	void LogDeadInputIgnored(const TCHAR* ActionName) const;
	void UpdateLocalCombatCamera(float DeltaSeconds);
	void DisableLocalCombatCameraRotationInput(APlayerController* PC);
	ARaidBoss* FindRaidBossForLocalCamera() const;
	FRotator ResolveLocalCameraRelativeInputRotation() const;
	FVector2D ConvertLocalScreenInputToWorldMoveDirection(FVector2D RawAxis, float& OutCameraYaw) const;
	void LogInputConversionSummary(
		const TCHAR* LogName,
		const FVector2D& RawAxis,
		const FVector2D& WorldAxis,
		float CameraYaw,
		float& InOutLastLogTime,
		FVector2D& InOutLastRawAxis,
		FVector2D& InOutLastWorldAxis,
		float& InOutLastCameraYaw);
	FVector2D ClampMoveInputAxisForServer(FVector2D RawAxis, bool& bOutWasClamped) const;
	bool IsDodgeAllowedForServer(const FVector2D& Direction, FName& OutIgnoreReason) const;
	void AddDodgeMoveDistanceForServer(float DeltaMeters);
	void ApplyDodgeInterpolatedLocationForServer(float Alpha);
	void UpdateDodgeForServer(float DeltaSeconds);
	void ClearDodgeInterpolationForServer();
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SetDodgeInvincibleVisual(bool bIsInvincibleVisual);
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayDroneAttackVisual(FName LeftWeaponPartID, FName RightWeaponPartID, float Damage, FVector From, FVector To);
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayDroneDamagedVisual(float Damage, float OldHP, float NewHP);
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayDroneDamageIgnoredVisual(FName Reason);
	void PlayDroneAttackVisualLocally(FName LeftWeaponPartID, FName RightWeaponPartID, float Damage, FVector From, FVector To);
	void PlayDroneDamagedVisualLocally(float Damage, float OldHP, float NewHP);
	void PlayDroneDamageIgnoredVisualLocally(FName Reason);
	void ApplyDodgeInvincibleVisualLocally(bool bIsInvincibleVisual);
	void SetDodgeInvincibleVisualHidden(bool bShouldHideVisual);
	void EndDodgeInvincibilityForServer();
	void EndDodgeForServer();
	bool CacheMoveInputForDodge(FVector2D RawAxis);
	void ClearCachedMoveInputForDodge();
	bool RequestDodgeFromCurrentMoveInput();
	bool ApplyMoveInputForServer(FVector2D RawAxis);
	bool ApplyPendingServerMoveInputForServer(float DeltaSeconds);
	void UpdateOwnerMoveSyncForServer(const FVector& ServerLocation, const FRotator& ServerRotation);
	void LogMoveInputSummary(const TCHAR* Result, const TCHAR* Reason, const FVector2D& Axis);
	void UpdateMoveDistanceForServer(float DeltaSeconds);
	void ResetMoveDistanceForServer(FName Reason);
	void ResetVectorMoveDistanceForServer(FName Reason);
	void RefreshMovementBoundaryCenterForServer();
	FVector ResolveMovementBoundaryCenterForServer() const;
	void LockZPositionForServer(FVector& Position) const;
	FVector ClampFinalMovementPositionForServer(FVector RequestedPosition);
	bool CanAccumulateMoveDistanceForServer(FName& OutIgnoreReason) const;
	void LogMoveDistanceIgnored(FName Reason);
	const FDroneCombatResolvedConfig& ResolveDroneCombatConfigForServer();
	FDroneWeaponCalculationResult CalculateWeaponDamageForServer(FName WeaponPartID, bool bIsLeftWeapon);
	FDroneCoreCalculationResult CalculateCoreForServer(FName CorePartID);
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

#if WITH_DEV_AUTOMATION_TESTS
	int32 CombatVisualAttackCountForTest = 0;
	float LastCombatVisualAttackDamageForTest = 0.0f;
	FVector LastCombatVisualAttackFromForTest = FVector::ZeroVector;
	FVector LastCombatVisualAttackToForTest = FVector::ZeroVector;
	int32 CombatVisualDroneDamagedCountForTest = 0;
	float LastCombatVisualDroneDamageForTest = 0.0f;
	float LastCombatVisualDroneDamageOldHPForTest = 0.0f;
	float LastCombatVisualDroneDamageNewHPForTest = 0.0f;
	int32 CombatVisualDroneDamageIgnoredCountForTest = 0;
	FName LastCombatVisualDroneDamageIgnoredReasonForTest = NAME_None;
	FName LastAttackNoDamageReasonForTest = NAME_None;
	FName LastAttackIgnoredReasonForTest = NAME_None;
	FName LastDodgeIgnoredReasonForTest = NAME_None;
	int32 CombatDataResolveCountForTest = 0;
#endif
};
