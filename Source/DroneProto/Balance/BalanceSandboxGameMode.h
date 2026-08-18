#pragma once

#include "CoreMinimal.h"
#include "Raid/RaidGameMode.h"
#include "BalanceSandboxGameMode.generated.h"

/**
 * 기획자 전용 밸런스 반복 시험 GameMode.
 *
 * 로컬 PIE/Standalone에서 외부 서버 탐색·HTTP 예약·로비 매칭을 거치지 않고 바로 전투를
 * 시험하기 위한 진입점만 제공한다. 전투 로직은 하나도 복제하지 않는다 — 아래 함수는 전부
 * 기존 서버 경로(`Server_RequestSelectPart`, `Server_RequestReadyForRaid`, 반환 매니저,
 * `ApplyLoadout`, `ApplyDamageForServer`, 리포트 생성)를 그대로 호출한다.
 *
 * 우회하는 것은 입장 과정뿐이다. 예약·admission 코드는 건드리지 않으며, Dedicated Server의
 * fail-closed 정책(`ARaidGameMode::BeginPlay`의 `bAdmissionRequired`)도 그대로다 —
 * 이 GameMode는 로컬 넷모드에서만 쓰이고 그 분기 자체를 수정하지 않는다.
 */
UCLASS()
class DRONEPROTO_API ABalanceSandboxGameMode : public ARaidGameMode
{
	GENERATED_BODY()

public:
	ABalanceSandboxGameMode();

	// 보스를 확보한 뒤 임시 프록시 크기를 입힌다. 스폰 경로 자체는 그대로 두고 시각만 얹는다.
	virtual ARaidBoss* EnsureRaidBossForServer() override;

	/** 코어/좌/우를 한 번에 지정한다. 슬롯마다 기존 선택 RPC를 그대로 타므로 재고 차감·교체 계약이 유지된다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Balance Sandbox")
	bool ApplySandboxLoadoutForServer(const FString& CoreAlias, const FString& LeftWeaponAlias, const FString& RightWeaponAlias);

	/** 15초를 기다리지 않고 즉시 전투에 들어간다. 기존 Ready 경로를 그대로 쓴다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Balance Sandbox")
	bool StartSandboxBattleForServer();

	/** 전투 상태·보스·리포트를 초기화하고 선택 단계로 되돌린다. 부품은 기존 반환 경로로 되돌아간다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Balance Sandbox")
	bool ResetSandboxRaidForServer();

	/** 다음 패턴을 지정한다. `corrupted` / `stellar`. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Balance Sandbox")
	bool SetSandboxNextPatternForServer(const FString& PatternAlias);

	/** 지정한 패턴을 바로 돌린다 — 다음 패턴을 정하고 루프가 멈춰 있으면 시작한다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Balance Sandbox")
	bool RunSandboxPatternForServer(const FString& PatternAlias);

	/** 보스에게 실제 피해 경로로 피해를 넣는다. 기여도 집계와 사망 처리가 그대로 동작한다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Balance Sandbox")
	bool DamageSandboxBossForServer(float DamageAmount);

	/** 현재 전투 기록으로 리포트를 만든다. 점수·등급·보너스는 기존 계산을 그대로 쓴다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Balance Sandbox")
	bool CreateSandboxReportForServer();

	/** 별칭(zenith/booster/drain/pulse/fracture/vector) 또는 실제 PartID를 해석한다. `none`은 빈 슬롯이다. */
	static bool TryResolvePartAlias(const FString& Alias, FName& OutPartID);

	/** 기획 원문 기준 임시 프록시 크기(폭 18m / 높이 16m). 접근 제한 8m와는 무관한 시각값이다. */
	static constexpr float BossProxyVisualWidthMeters = 18.0f;
	static constexpr float BossProxyVisualHeightMeters = 16.0f;

	/** 첫 번째 로컬 `ARaidPlayerController`를 돌려준다. 샌드박스는 단일 플레이어 기준이다. */
	class ARaidPlayerController* GetSandboxPlayerControllerForServer() const;
};
