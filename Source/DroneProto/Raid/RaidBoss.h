#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RaidBoss.generated.h"

class ARaidBossAttackTelegraph;
class UBossPatternComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class USoundBase;
class USoundAttenuation;
class UAudioComponent;

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
	void NotifyPatternPopulationChangedForServer(FName Reason);

	// 스턴 상태 변경은 서버 전용. 스턴 중 받는 데미지에 StunDamageMultiplier가 적용된다.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Raid|Boss|Stun")
	void SetStunnedForServer(bool bInStunned, FName Reason);

	// 최종 보스 에셋이 없는 동안 기획 크기(보스 원문 `:303-307` 높이 16m / 폭 18m)를 눈으로
	// 가늠하기 위한 임시 프록시 크기다. 시각 컴포넌트의 스케일만 바꾸며 접근 제한(8m),
	// 패턴 시작 반경, 피격·타겟 기준점은 하나도 건드리지 않는다.
	// 컴포넌트 스케일은 복제되지 않으므로 단일 프로세스(PIE/Standalone) 표현용이다.
	UFUNCTION(BlueprintCallable, Category = "Raid|Boss|Visual")
	void ApplyVisualProxySize(float VisualWidthMeters, float VisualHeightMeters, FName Reason);

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

	// ---- 보스 SFX ----

	/** 보스가 전투 맵에 활성일 때 1개만. Death/Clear/TimeOver/Unload 어느 쪽이든 멈춘다. 3D Boss. */
	UPROPERTY(EditDefaultsOnly, Category = "Raid|Boss|Audio")
	TObjectPtr<USoundBase> SFX_Boss_Idle_Loop = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Raid|Boss|Audio")
	TObjectPtr<USoundAttenuation> BossAudioAttenuation = nullptr;

	// 로컬 플레이어가 유효 피해를 준 경우에만 울린다. 16인이 동시에 때리는 보스라
	// 모든 피격에 울리면 소리가 뭉개진다(가이드 3절).
	UPROPERTY(EditDefaultsOnly, Category = "Raid|Boss|Audio")
	TObjectPtr<USoundBase> SFX_Boss_Hit = nullptr;

	/** HP <= 0. 사망/클리어 전환에 걸쳐 정확히 1회. 3D Boss. */
	UPROPERTY(EditDefaultsOnly, Category = "Raid|Boss|Audio")
	TObjectPtr<USoundBase> SFX_Boss_Death = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> BossIdleLoopAudioComponent = nullptr;

	// 사망음은 Dead와 Clear 두 번의 전환에 걸쳐 도착할 수 있다. 한 번만 울리도록 잠근다.
	bool bHasPlayedBossDeathSound = false;

	// 클라이언트 표현 전용. 상태 전환과 BeginPlay 양쪽에서 부른다 —
	// 늦게 접속한 클라이언트는 이미 Battle인 보스를 만나 전환 이벤트를 받지 못한다.
	void UpdateBossAudioLocally();
	void StopBossIdleLoop();
	void ExecuteDebugTelegraphedAreaAttackForServer(FVector AttackCenter, float RadiusCm, int32 DamageAmount, ARaidBossAttackTelegraph* TelegraphActor);

#if WITH_DEV_AUTOMATION_TESTS
	int32 CombatVisualBossDamagedCountForTest = 0;
	float LastCombatVisualBossDamageForTest = 0.0f;
	float LastCombatVisualBossOldHPForTest = 0.0f;
	float LastCombatVisualBossNewHPForTest = 0.0f;
#endif
};
