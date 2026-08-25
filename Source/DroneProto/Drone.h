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
class UMeshComponent;
class USoundBase;
class USoundAttenuation;
class UAudioComponent;
class UNiagaraComponent;
class UMaterialInterface;
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

/**
 * 마지막 Z 공격 한 번의 피해 분해값. 밸런스 시험에서 "왜 이 수치가 나왔는지"를 보기 위한
 * 기록이며, 공격 경로가 이미 계산해 로그로 내보내던 값을 그대로 담는다. 새로 계산하지 않는다.
 */
USTRUCT(BlueprintType)
struct FDroneLastAttackBreakdown
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Combat")
	bool bHasAttacked = false;

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Combat")
	float LeftWeaponDamage = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Combat")
	float RightWeaponDamage = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Combat")
	float FinalDamage = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Combat")
	float DamageDealt = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Combat")
	float HealAmount = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Combat")
	float CoreAttackModifier = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Combat")
	float CoreBonusAttackModifier = 1.0f;
};

/**
 * 공격 연출이 Normal과 Strong을 가르는 데 필요한 슬롯별 판정값.
 *
 * 클라이언트는 이 값을 스스로 알 수 없다 — `LeftPulseAttackCount`/`RightPulseAttackCount`와
 * `VectorAccumulatedMoveDistanceMeters`는 `Transient`이고 복제되지 않으며, `LastAttackBreakdown`도
 * 복제 대상이 아니다. 게다가 Pulse 카운터는 3타에서 0으로 되감기므로 연출 시점에 다시 읽으면
 * 3타를 영영 구분할 수 없다. 그래서 서버가 계산 시점에 확정한 값을 payload로 실어 보낸다.
 *
 * 좌우 동일 무기의 중복 재생 방지(dedupe)는 Blueprint가 한다. 여기서는 슬롯별로만 준다.
 */
USTRUCT(BlueprintType)
struct FDroneAttackVisualPayload
{
	GENERATED_BODY()

	/** 왼쪽 슬롯이 강화 변형이었는가. Pulse 3타 또는 Vector 충전 발사. */
	UPROPERTY(BlueprintReadOnly, Category = "Drone|Combat|Visual")
	bool bLeftStrongVariant = false;

	/** 오른쪽 슬롯이 강화 변형이었는가. 판정 기준은 왼쪽과 같다. */
	UPROPERTY(BlueprintReadOnly, Category = "Drone|Combat|Visual")
	bool bRightStrongVariant = false;

	// 슬롯별 무기 타입. 연출·사운드가 Pulse/Fracture/Vector를 가르는 유일한 근거다.
	// PartID만으로는 클라이언트가 타입을 알 수 없다 — `ResolveWeaponTypeForServer`는 서버 전용
	// DataTable 조회라 연출을 실제로 재생하는 원격 클라이언트에서는 쓸 수 없다.
	UPROPERTY(BlueprintReadOnly, Category = "Drone|Combat|Visual")
	EDroneCombatWeaponType LeftWeaponType = EDroneCombatWeaponType::None;

	/** 오른쪽 슬롯 무기 타입. 판정 근거는 왼쪽과 같다. */
	UPROPERTY(BlueprintReadOnly, Category = "Drone|Combat|Visual")
	EDroneCombatWeaponType RightWeaponType = EDroneCombatWeaponType::None;
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

	// 드론을 전투 이전 상태로 되돌린다 — 장착 해제, 사망 해제, HP 복구, 전투 기록 초기화.
	// Ready가 사망한 Pawn을 거부하므로(DeadPawn) 재시험을 위해서는 사망 해제가 선행돼야 한다.
	// 반환은 이 함수가 하지 않는다. 공유 재고는 기존 반환 경로만 건드린다.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Drone|Loadout")
	void ResetForSelectionPhaseForServer(FName Reason);

	/** 마지막 공격의 피해 분해값. 밸런스 상태 패널 조회용이며 값은 공격 경로가 채운다. */
	UFUNCTION(BlueprintPure, Category = "Drone|Combat")
	const FDroneLastAttackBreakdown& GetLastAttackBreakdown() const { return LastAttackBreakdown; }

	/** 펄스 3타 카운터. 슬롯별로 센다(WEAPON-03). */
	UFUNCTION(BlueprintPure, Category = "Drone|Combat")
	int32 GetPulseAttackCount(bool bIsLeftWeapon) const;

	/** 벡터 캐논 보너스에 쓰는 누적 이동 거리(m). 공격마다 0으로 되감긴다(WEAPON-05). */
	UFUNCTION(BlueprintPure, Category = "Drone|Combat")
	float GetVectorAccumulatedMoveDistance() const;

	/** 부스터 코어 보정에 쓰는 누적 이동 거리(m). 벡터와 분리돼 있다. */
	UFUNCTION(BlueprintPure, Category = "Drone|Combat")
	float GetBoosterAccumulatedMoveDistance() const;

	/** 코어·무기 DataTable 해석 결과. `None`이면 표 값을 쓰는 중이다. */
	// UENUM이 아니라 순수 C++ enum이라 UFUNCTION으로 노출할 수 없다. 밸런스 패널은 C++에서 읽는다.
	EDroneCombatDataFallbackReason GetCombatDataFallbackReason() const { return DroneCombatDataFallbackReason; }

	// 코어·무기 표를 한 번이라도 해석했는지. 해석 전에는 DroneCombatDataFallbackReason이
	// 초기값(MissingCoreTable)이라 "아직 안 읽음"과 "표가 없음"이 구분되지 않는다.
	// 이 값이 false인 동안의 사유는 판정 근거가 아니다.
	UFUNCTION(BlueprintPure, Category = "Drone|Combat")
	bool IsCombatDataResolved() const { return bDroneCombatConfigResolved; }

	// 현재 상태 기준 코어 계산 결과. 기존 규칙 함수(FDroneCombatRules::CalculateCoreBonus)를
	// 그대로 쓰고 상태 변경도 로그도 하지 않는다 — 밸런스 패널이 주기적으로 불러도 안전하다.
	FDroneCoreCalculationResult GetCoreCalculationSnapshot() const;

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
		float PitchDegrees,
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

	// 전투 중 드론 정면은 항상 보스를 향한다(기획자 2026-08-22 확정).
	// 이동 방향으로는 회전하지 않는다 — 후방 이동도 정면을 유지한 채 뒤로 간다.
	// 카메라 Yaw와 같은 벡터(드론->보스를 XY 평면에 투영)를 쓰므로 두 값이 어긋나지 않는다.
	static bool CalculateBossFacingDroneYaw(
		const FVector& DroneLocation,
		const FVector* BossLocation,
		float& OutYawDegrees);

	bool IsBossFacingRotationAllowedForServer(FName& OutBlockReason) const;
	void UpdateBossFacingRotationForServer();

	bool IsMovementAllowedForServer(FName& OutIgnoreReason) const;
	FVector ClampPositionToMovementBoundaryForServer(FVector RequestedPosition);
	FVector ClampPositionOutsideBossCenterForServer(FVector RequestedPosition);

	UFUNCTION(BlueprintImplementableEvent, Category = "Drone|Dodge")
	void BP_OnDodgeVisualStateChanged(bool bDodging);

	// 사망 연출 진입점. 종전에는 `IsDead` 폴링으로만 알 수 있어 연출 타이밍이 프레임 단위로 밀렸다.
	// 새 RPC를 만들지 않는다 — `bIsDead`가 이미 `ReplicatedUsing = OnRep_IsDead`로 복제되므로
	// 원격 클라이언트는 OnRep에서, 권한 측(리슨·PIE)은 `HandleDeath`에서 직접 받는다.
	// `BP_OnDodgeVisualStateChanged`와 같은 패턴이다.
	UFUNCTION(BlueprintImplementableEvent, Category = "Drone|Combat")
	void BP_OnDroneDeathVisual();

	UFUNCTION(BlueprintNativeEvent, Category = "Drone|Dodge")
	void BP_OnDodgeInvincibleVisualChanged(bool bIsInvincibleVisual);

	// `Payload`는 뒤에 붙였다. 기존 BP 노드는 핀이 하나 늘 뿐 재배선이 필요 없다.
	UFUNCTION(BlueprintNativeEvent, Category = "Drone|Combat|Visual")
	void BP_OnDroneAttackVisual(FName LeftWeaponPartID, FName RightWeaponPartID, float Damage, FVector From, FVector To, FDroneAttackVisualPayload Payload);

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
	FDroneAttackVisualPayload GetLastCombatVisualAttackPayloadForTest() const;
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
	void SetCombatRecordForTest(const FDroneCombatRecord& InCombatRecord);
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

	FDroneLastAttackBreakdown LastAttackBreakdown;

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

	// 원본 시퀀스처럼 드론과 함께 600uu를 이동한다. 시작·종료 위치에 따로 스폰하지 않는다.
	UPROPERTY(VisibleDefaultsOnly, Category = "Drone|Dodge|Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraComponent> DodgeTeleportVFX = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Drone|Combat|Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMaterialInterface> DroneHitFlashMaterial = nullptr;

	// ---- SFX 슬롯 ----
	// 에디터에서 `BP_Drone`의 Details 패널로 지정한다. 비워두면 그 소리만 조용히 건너뛴다.
	// 재생 시점·중복 제거·강화 변형 판정은 전부 C++이 한다(SFX 가이드 1·2절).
	// 가이드가 이 7종을 전부 "2D Local"로 지정했으므로 로컬 플레이어의 드론에서만 울린다.

	/** Pulse 1·2타. 3타는 `SFX_Weapon_Pulse_StrongFire`가 대신하며 둘은 겹치지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "Drone|Audio|Weapon", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USoundBase> SFX_Weapon_Pulse_Fire = nullptr;

	/** Pulse 3타 강공격. */
	UPROPERTY(EditDefaultsOnly, Category = "Drone|Audio|Weapon", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USoundBase> SFX_Weapon_Pulse_StrongFire = nullptr;

	// Fracture는 발사·명중·파편을 복합 1개로 처리한다. 다단히트마다 재생하지 않는다.
	// 강화 변형이 없으므로 슬롯도 하나뿐이다.
	UPROPERTY(EditDefaultsOnly, Category = "Drone|Audio|Weapon", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USoundBase> SFX_Weapon_Fracture_Fire = nullptr;

	/** Vector 무충전 발사. 누적 이동 보너스가 0일 때. */
	UPROPERTY(EditDefaultsOnly, Category = "Drone|Audio|Weapon", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USoundBase> SFX_Weapon_Vector_Fire = nullptr;

	/** Vector 충전 발사. 누적 이동 보너스가 1 이상이면 무충전 대신 이쪽만 울린다. */
	UPROPERTY(EditDefaultsOnly, Category = "Drone|Audio|Weapon", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USoundBase> SFX_Weapon_Vector_ChargedFire = nullptr;

	/** 로컬 플레이어가 유효 피해를 받은 순간. 무적으로 무시된 피해에는 울리지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "Drone|Audio|Drone", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USoundBase> SFX_Drone_Hit = nullptr;

	/** 회피가 승인된 순간 1회. 0.25초 회피 전체를 이 한 파일로 표현한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Drone|Audio|Drone", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USoundBase> SFX_Drone_DodgeTeleport = nullptr;

	// ---- 루프 SFX ----

	// 부유 + 이동 공통 루프. 원샷 7종과 달리 3D Attached라 살아 있는 모든 드론에서 울린다.
	// 남의 드론 호버음은 위치를 알려주는 정보이므로 로컬 한정으로 걸지 않는다.
	UPROPERTY(EditDefaultsOnly, Category = "Drone|Audio|Drone", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USoundBase> SFX_Drone_Hover_Loop = nullptr;

	// Hover 루프의 거리 감쇠. `ATT_3D_Default`를 지정한다. 비워두면 감쇠 없이 들린다.
	UPROPERTY(EditDefaultsOnly, Category = "Drone|Audio|Drone", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USoundAttenuation> HoverLoopAttenuation = nullptr;

	// Vector 이동 에너지 축적 루프. 2D Local이며 자기 드론에서만 울린다.
	UPROPERTY(EditDefaultsOnly, Category = "Drone|Audio|Weapon", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USoundBase> SFX_Weapon_Vector_Charge_Loop = nullptr;

	// 호버 루프의 이동 반응 폭. 가이드가 "소폭 증가"라 했으므로 기본값을 작게 잡는다.
	UPROPERTY(EditDefaultsOnly, Category = "Drone|Audio|Drone", meta = (ClampMin = "0.0", ClampMax = "1.0", AllowPrivateAccess = "true"))
	float HoverLoopMaxPitchGain = 0.08f;

	UPROPERTY(EditDefaultsOnly, Category = "Drone|Audio|Drone", meta = (ClampMin = "0.0", ClampMax = "1.0", AllowPrivateAccess = "true"))
	float HoverLoopMaxVolumeGain = 0.15f;

	// Vector 충전 단계당 피치 상승폭. 단계는 0~9이므로 최대 +0.16이 된다.
	UPROPERTY(EditDefaultsOnly, Category = "Drone|Audio|Weapon", meta = (ClampMin = "0.0", ClampMax = "0.2", AllowPrivateAccess = "true"))
	float VectorChargePitchGainPerStep = 0.02f;

	// 루프 재생 핸들. 복제하지 않는다 — 각 클라이언트가 자기 것만 들고 있으면 된다.
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> HoverLoopAudioComponent = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> VectorChargeAudioComponent = nullptr;

	// 호버 피치 계산용 이전 위치. 원격 드론은 속도를 직접 알 수 없어 위치 변화로 추정한다.
	FVector HoverLoopLastLocation = FVector::ZeroVector;
	bool bHasHoverLoopLocationSample = false;
	float HoverLoopSmoothedSpeedCmPerSecond = 0.0f;

	// 마지막으로 Owner에게 보낸 Vector 충전 단계. 값이 바뀔 때만 RPC를 보내기 위한 비교 기준이다.
	uint8 LastSentVectorChargeStepForServer = 0;

	// 클라이언트가 적용 중인 단계. 같은 값이 다시 오면 재생을 건드리지 않는다.
	uint8 AppliedVectorChargeStepLocally = 0;

	UPROPERTY(EditDefaultsOnly, Category = "Drone|Camera", meta = (ClampMin = "0.0", Units = "cm", AllowPrivateAccess = "true"))
	float CombatCameraDistanceCm = 1400.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Drone|Camera", meta = (ClampMin = "0.0", Units = "cm", AllowPrivateAccess = "true"))
	float CombatCameraHeightCm = 380.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Drone|Camera", meta = (Units = "deg", AllowPrivateAccess = "true"))
	float CombatCameraPitchDegrees = -10.0f;

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
	FTimerHandle DroneHitFlashTimerHandle;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMeshComponent>> DroneHitFlashMeshes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> DroneHitFlashPreviousOverlays;

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
	void EmitAttackAttemptedForServer();
	void EmitAttackResolvedForServer(FName Result, FName Reason, float RawDamage, float AppliedDamage, float HealAmount, float BossHPBefore, float BossHPAfter);
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
	void Multicast_SetDodgeInvincibleVisual(bool bIsInvincibleVisual, FVector_NetQuantizeNormal DodgeVisualDirection);
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayDroneAttackVisual(FName LeftWeaponPartID, FName RightWeaponPartID, float Damage, FVector From, FVector To, FDroneAttackVisualPayload Payload);
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayDroneDamagedVisual(float Damage, float OldHP, float NewHP);
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayDroneDamageIgnoredVisual(FName Reason);
	void PlayDroneAttackVisualLocally(FName LeftWeaponPartID, FName RightWeaponPartID, float Damage, FVector From, FVector To, const FDroneAttackVisualPayload& Payload);
	void PlayDroneDamagedVisualLocally(float Damage, float OldHP, float NewHP);
	void PlayDroneDamageIgnoredVisualLocally(FName Reason);
	void StartDroneHitFlash(float OldHP, float NewHP);
	void EndDroneHitFlash();

	// 이번 타격에서 슬롯 하나가 낼 소리를 고른다. 지정되지 않았거나 무기가 없으면 nullptr.
	USoundBase* ResolveWeaponFireSound(EDroneCombatWeaponType WeaponType, bool bStrongVariant) const;

	// 로컬 플레이어의 드론에서만 2D로 재생한다. 데디케이티드 서버와 남의 드론에서는 아무것도 하지 않는다.
	// `Sound`가 nullptr이면 조용히 건너뛴다 — 에셋 미지정이 로그를 더럽히지 않게 한다.
	void PlayLocal2DSound(USoundBase* Sound) const;

	// 좌우 슬롯의 발사음을 가이드 dedupe 규칙대로 재생한다.
	void PlayAttackSoundsLocally(const FDroneAttackVisualPayload& Payload) const;

	// ---- 루프 SFX ----

	// 클라이언트 표현 전용. 데디케이티드 서버에서는 아무것도 하지 않는다.
	void UpdateAudioLoopsLocally(float DeltaSeconds);
	void UpdateHoverLoopLocally(float DeltaSeconds);
	void StopAudioLoopsLocally();

	// Vector 충전 단계를 계산해 값이 바뀐 경우에만 Owner에게 보낸다.
	// 누적 거리 자체는 복제하지 않는다 — 매 프레임 변하는 값이라 16인 레이드에서 대역폭을 계속 먹는다.
	void UpdateVectorChargeVisualForServer();

	// 0 = 정지, 1 = 충전 시작(보너스 아직 0), 2~9 = 보너스 1~8단계.
	// 데이터테이블 조회가 const가 아니라 이 함수도 const로 둘 수 없다.
	uint8 CalculateVectorChargeStepForServer();

	UFUNCTION(Client, Unreliable)
	void Client_UpdateVectorChargeVisual(uint8 ChargeStep);

	void ApplyVectorChargeVisualLocally(uint8 ChargeStep);
	void ApplyDodgeVisualStateLocally(bool bDodging);
	void ApplyDodgeVisualDirectionLocally(const FVector& DodgeVisualDirection);
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
	FDroneAttackVisualPayload LastCombatVisualAttackPayloadForTest;
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
