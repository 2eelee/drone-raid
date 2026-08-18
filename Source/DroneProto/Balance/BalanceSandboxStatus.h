#pragma once

#include "CoreMinimal.h"
#include "BalanceSandboxStatus.generated.h"

/**
 * 밸런스 상태 패널이 한 번에 읽어 가는 조회 결과.
 *
 * 저장되는 상태가 아니다. 요청할 때마다 드론·보스·GameState·재고에서 **읽어서 조립**하며,
 * 어떤 값도 여기 담기려고 따로 보관되지 않는다. 계산식도 새로 만들지 않는다 —
 * 마지막 공격 분해값은 공격 경로가 이미 계산해 둔 것을 그대로 옮겨 온다.
 */
USTRUCT(BlueprintType)
struct FBalanceSandboxStatus
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Balance Sandbox|Status")
	bool bHasDrone = false;

	UPROPERTY(BlueprintReadOnly, Category = "Balance Sandbox|Status")
	bool bHasBoss = false;

	// [LOADOUT]
	UPROPERTY(BlueprintReadOnly, Category = "Balance Sandbox|Loadout")
	FName EquippedCorePartID = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Balance Sandbox|Loadout")
	FName EquippedLeftWeaponPartID = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Balance Sandbox|Loadout")
	FName EquippedRightWeaponPartID = NAME_None;

	// [PLAYER]
	UPROPERTY(BlueprintReadOnly, Category = "Balance Sandbox|Player")
	int32 CurrentHP = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Balance Sandbox|Player")
	int32 MaxHP = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Balance Sandbox|Player")
	float CurrentMoveSpeed = 0.0f;

	/** 현재 상태 기준 코어 기본 배율. 코어 규칙을 그대로 호출해 얻는다. */
	UPROPERTY(BlueprintReadOnly, Category = "Balance Sandbox|Player")
	float CoreAttackModifier = 1.0f;

	/** 현재 상태 기준 코어 특수 보너스 배율(Zenith HP 비율, Booster 이동 누적 등). */
	UPROPERTY(BlueprintReadOnly, Category = "Balance Sandbox|Player")
	float CoreBonusAttackModifier = 1.0f;

	// [LAST ATTACK]
	UPROPERTY(BlueprintReadOnly, Category = "Balance Sandbox|Last Attack")
	bool bHasAttacked = false;

	UPROPERTY(BlueprintReadOnly, Category = "Balance Sandbox|Last Attack")
	float LastLeftWeaponDamage = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Balance Sandbox|Last Attack")
	float LastRightWeaponDamage = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Balance Sandbox|Last Attack")
	float LastFinalDamage = 0.0f;

	/** 보스 HP의 실제 감소분. 과잉 피해는 여기 포함되지 않는다. */
	UPROPERTY(BlueprintReadOnly, Category = "Balance Sandbox|Last Attack")
	float LastDamageDealt = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Balance Sandbox|Last Attack")
	float LastDrainHealAmount = 0.0f;

	// [WEAPON / CORE STATE]
	UPROPERTY(BlueprintReadOnly, Category = "Balance Sandbox|Weapon State")
	int32 LeftPulseAttackCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Balance Sandbox|Weapon State")
	int32 RightPulseAttackCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Balance Sandbox|Weapon State")
	float VectorAccumulatedMoveDistanceMeters = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Balance Sandbox|Weapon State")
	float BoosterAccumulatedMoveDistanceMeters = 0.0f;

	/** 코어 규칙이 내놓은 이동 보너스 비율. 부스터가 아니면 0이다. */
	UPROPERTY(BlueprintReadOnly, Category = "Balance Sandbox|Weapon State")
	float CoreMoveSpeedBonus = 0.0f;

	// [RAID]
	UPROPERTY(BlueprintReadOnly, Category = "Balance Sandbox|Raid")
	float BossCurrentHP = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Balance Sandbox|Raid")
	float BossMaxHP = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Balance Sandbox|Raid")
	float RaidTimeRemainingSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Balance Sandbox|Raid")
	FName CurrentBossPattern = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Balance Sandbox|Raid")
	FName RaidState = NAME_None;

	// [RECORD]
	UPROPERTY(BlueprintReadOnly, Category = "Balance Sandbox|Record")
	float TotalDamageDealt = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Balance Sandbox|Record")
	float TotalHealAmount = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Balance Sandbox|Record")
	float MoveDistanceMeters = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Balance Sandbox|Record")
	float SurvivalTimeSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Balance Sandbox|Record")
	int32 HitCount = 0;

	// [STOCK]
	UPROPERTY(BlueprintReadOnly, Category = "Balance Sandbox|Stock")
	int32 ZenithStock = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Balance Sandbox|Stock")
	int32 BoosterStock = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Balance Sandbox|Stock")
	int32 DrainStock = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Balance Sandbox|Stock")
	int32 PulseStock = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Balance Sandbox|Stock")
	int32 FractureStock = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Balance Sandbox|Stock")
	int32 VectorStock = 0;

	// [DATA SOURCE] — DataTable 사용 중이면 true. false면 canonical fallback으로 도는 중이라
	// 화면 수치가 기획표가 아니라 코드 기본값이다.
	UPROPERTY(BlueprintReadOnly, Category = "Balance Sandbox|Data Source")
	bool bCoreDataTableInUse = false;

	UPROPERTY(BlueprintReadOnly, Category = "Balance Sandbox|Data Source")
	bool bWeaponDataTableInUse = false;

	UPROPERTY(BlueprintReadOnly, Category = "Balance Sandbox|Data Source")
	bool bBossPatternDataTableInUse = false;

	UPROPERTY(BlueprintReadOnly, Category = "Balance Sandbox|Data Source")
	bool bReportDataTableInUse = false;

	/** 코어·무기 표 해석이 실패했다면 그 사유. 정상이면 `None`이다. */
	UPROPERTY(BlueprintReadOnly, Category = "Balance Sandbox|Data Source")
	FName CombatDataFallbackReason = NAME_None;

	/**
	 * 표 상태를 실제로 판정할 수 있었는지. 드론이 없으면 코어·무기를, 보스가 없으면 패턴을
	 * 읽을 대상이 없다. 이 값이 false인데 위 플래그가 false인 것은 "fallback"이 아니라
	 * "아직 모름"이다 — 둘을 섞으면 부트스트랩 실패를 밸런스 데이터 문제로 오독한다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Balance Sandbox|Data Source")
	bool bCombatDataSourceKnown = false;

	UPROPERTY(BlueprintReadOnly, Category = "Balance Sandbox|Data Source")
	bool bPatternDataSourceKnown = false;

	/** 판정 가능한 것 중 하나라도 fallback이면 true. */
	UPROPERTY(BlueprintReadOnly, Category = "Balance Sandbox|Data Source")
	bool bAnyDataTableFallback = false;
};
