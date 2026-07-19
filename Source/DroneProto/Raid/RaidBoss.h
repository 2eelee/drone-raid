#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RaidBoss.generated.h"

class ARaidBossAttackTelegraph;
class UBossPatternComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

UENUM(BlueprintType)
enum class EBossState : uint8
{
	Spawn  UMETA(DisplayName = "Spawn"),
	Battle UMETA(DisplayName = "Battle"),
	Dead   UMETA(DisplayName = "Dead"),
	Clear  UMETA(DisplayName = "Clear"),
};

UCLASS()
class DRONEPROTO_API ARaidBoss : public AActor
{
	GENERATED_BODY()

public:
	ARaidBoss();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Raid|Boss")
	void ApplyDamageForServer(float DamageAmount, AController* InstigatorController, AActor* DamageCauser);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Raid|Boss")
	void SetBossStateForServer(EBossState NewBossState, FName Reason = NAME_None);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Raid|Boss|Debug")
	int32 PerformDebugAreaAttackForServer(FVector AttackCenter, float RadiusCm = 300.0f, int32 DamageAmount = 25);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Raid|Boss|Debug")
	bool StartDebugTelegraphedAreaAttackForServer(FVector AttackCenter, float RadiusCm = 300.0f, int32 DamageAmount = 25, float TelegraphSeconds = 1.0f);

	// Battle 시작 시 서버 타이머로 기존 telegraph 공격을 반복 실행한다. 이미 실행 중이면 무시(false).
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Raid|Boss|Pattern")
	bool StartBossPatternForServer();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Raid|Boss|Pattern")
	void StopBossPatternForServer(FName Reason);

	// 스턴 상태 변경은 서버 전용. 스턴 중 받는 데미지에 StunDamageMultiplier가 적용된다.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Raid|Boss|Stun")
	void SetStunnedForServer(bool bInStunned, FName Reason);

	UFUNCTION(BlueprintPure, Category = "Raid|Boss|Stun")
	bool IsStunned() const;

	// 스턴 시작/해제 visual 전용 훅. 게임플레이 로직을 넣지 말 것.
	UFUNCTION(BlueprintNativeEvent, Category = "Raid|Boss|Visual")
	void BP_OnBossStunChangedVisual(bool bNewStunned);

	UFUNCTION(BlueprintPure, Category = "Raid|Boss")
	float GetCurrentHP() const;

	UFUNCTION(BlueprintPure, Category = "Raid|Boss")
	float GetMaxHP() const;

	UFUNCTION(BlueprintPure, Category = "Raid|Boss")
	bool IsDefeated() const;

	UFUNCTION(BlueprintPure, Category = "Raid|Boss")
	EBossState GetBossState() const;

	UFUNCTION(BlueprintPure, Category = "Raid|Boss|Targeting")
	FName GetBossID() const;

	UFUNCTION(BlueprintPure, Category = "Raid|Boss|Targeting")
	FText GetBossDisplayName() const;

	UFUNCTION(BlueprintPure, Category = "Raid|Boss|Targeting")
	bool IsAliveForTargeting() const;

	UFUNCTION(BlueprintPure, Category = "Raid|Boss|Targeting")
	bool IsTargetableForDrone() const;

	UFUNCTION(BlueprintPure, Category = "Raid|Boss|Targeting")
	FVector GetTargetMarkerWorldLocation() const;
	UFUNCTION(BlueprintPure, Category = "Raid|Boss|Visual")
	bool IsVisualReadyForCamera() const;

	UFUNCTION(BlueprintNativeEvent, Category = "Raid|Boss|Visual")
	void BP_OnBossDamagedVisual(float Damage, float OldHP, float NewHP, AActor* DamageCauser);

	UFUNCTION(BlueprintNativeEvent, Category = "Raid|Boss|Visual")
	void BP_OnBossStateChangedVisual(EBossState NewBossState);

#if WITH_DEV_AUTOMATION_TESTS
	int32 GetCombatVisualBossDamagedCountForTest() const;
	float GetLastCombatVisualBossDamageForTest() const;
	float GetLastCombatVisualBossOldHPForTest() const;
	float GetLastCombatVisualBossNewHPForTest() const;
	FString GetPrototypeVisualLabelTextForTest() const;
	bool IsBossPatternTimerActiveForTest() const;
	void FireBossPatternOnceForTest();
#endif

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Destroyed() override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Raid|Boss|Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> PrototypeVisualMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Raid|Boss|Pattern", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBossPatternComponent> BossPatternComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Raid|Boss|Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTextRenderComponent> PrototypeVisualLabel = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Raid|Boss|Visual", meta = (AllowPrivateAccess = "true"))
	bool bShowPrototypeVisualLabel = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Raid|Boss|Visual", meta = (AllowPrivateAccess = "true", ClampMin = "50.0", UIMin = "50.0"))
	float PrototypeVisualRadiusCm = 220.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Raid|Boss|Targeting", meta = (AllowPrivateAccess = "true"))
	FName BossID;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Raid|Boss|Targeting", meta = (AllowPrivateAccess = "true"))
	FText BossDisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Raid|Boss|Targeting", meta = (AllowPrivateAccess = "true"))
	bool bIsTargetable = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Raid|Boss|Targeting", meta = (AllowPrivateAccess = "true"))
	FName TargetMarkerSocketName;
	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadOnly, Category = "Raid|Boss", meta = (AllowPrivateAccess = "true"))
	float MaxHP = 60000.0f;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentHP, VisibleInstanceOnly, BlueprintReadOnly, Category = "Raid|Boss", meta = (AllowPrivateAccess = "true"))
	float CurrentHP = 60000.0f;

	UPROPERTY(ReplicatedUsing = OnRep_BossState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Raid|Boss", meta = (AllowPrivateAccess = "true"))
	EBossState BossState = EBossState::Spawn;

	UFUNCTION()
	void OnRep_CurrentHP();

	UFUNCTION()
	void OnRep_BossState();

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayBossDamagedVisual(float Damage, float OldHP, float NewHP, AActor* DamageCauser);

	// SpecDecisionNeeded: 스턴 배율(1.5 placeholder)/지속시간/자연 진입 조건 미확정.
	// 스턴 중 패턴 정지 여부도 미정 — 현재는 패턴이 계속 진행된다.
	// Q9_SPEC_BOUNDARY_BOSS_PATTERN_STUN:
	// Stun behavior is placeholder until boss pattern/stun source spec is available.
	// No natural stun trigger is specified; debug/manual stun is the only current entry path.
	// Do not merge bIsStunned into BossState.
	UPROPERTY(EditDefaultsOnly, Category = "Raid|Boss|Stun", meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
	float StunDamageMultiplier = 1.5f;

	UPROPERTY(ReplicatedUsing = OnRep_IsStunned, VisibleInstanceOnly, BlueprintReadOnly, Category = "Raid|Boss|Stun", meta = (AllowPrivateAccess = "true"))
	bool bIsStunned = false;

	UFUNCTION()
	void OnRep_IsStunned();

	void ClearThisBossFromAllTargetsForServer(FName Reason);
	void ApplyPrototypeVisualSettings();
	void ApplyPrototypeVisualLabelVisibility();
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
