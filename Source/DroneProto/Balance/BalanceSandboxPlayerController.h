#pragma once

#include "CoreMinimal.h"
#include "Raid/RaidPlayerController.h"
#include "BalanceSandboxPlayerController.generated.h"

/**
 * 밸런스 샌드박스 전용 콘솔 진입점.
 *
 * UMG 없이도 반복 시험이 되도록 `Exec` 래퍼만 얹는다. 판정은 하나도 하지 않고 전부
 * `ABalanceSandboxGameMode`의 서버 함수로 넘긴다 — 프로덕션 `ARaidPlayerController`에는
 * 샌드박스 명령을 추가하지 않기 위해 클래스를 나눴다.
 *
 * 로컬 단일 프로세스(PIE/Standalone) 기준이라 이 컨트롤러가 곧 서버 권한을 가진다.
 * 신규 RPC를 만들지 않은 이유이며, 권한이 없으면 각 명령이 그대로 거부된다.
 */
UCLASS()
class DRONEPROTO_API ABalanceSandboxPlayerController : public ARaidPlayerController
{
	GENERATED_BODY()

public:
	/** BalanceLoadout drain pulse pulse */
	UFUNCTION(Exec)
	void BalanceLoadout(FString CoreAlias, FString LeftWeaponAlias, FString RightWeaponAlias);

	/** 15초를 기다리지 않고 즉시 전투 시작 */
	UFUNCTION(Exec)
	void BalanceStart();

	/** 전투·보스·리포트를 초기화하고 선택 단계로 복귀 */
	UFUNCTION(Exec)
	void BalanceReset();

	/** BalancePattern corrupted | BalancePattern stellar */
	UFUNCTION(Exec)
	void BalancePattern(FString PatternAlias);

	/** BalanceBossDamage 5000 */
	UFUNCTION(Exec)
	void BalanceBossDamage(float DamageAmount);

	/** 현재 전투 기록으로 DroneReport 생성 */
	UFUNCTION(Exec)
	void BalanceReport();

private:
	class ABalanceSandboxGameMode* GetSandboxGameMode() const;
};
