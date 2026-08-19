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
	ABalanceSandboxPlayerController();

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

protected:
	/**
	 * 샌드박스는 선택 시간 15초 자동 확정을 걸지 않는다.
	 *
	 * 프로덕션은 원문 (7)/3.(2)대로 15초 뒤 현재 상태로 자동 확정하고 곧바로 전투에 들어가는데,
	 * 밸런스 샌드박스는 시험자가 로드아웃을 조합하고 전투 시작 시점을 직접 잡는 것이 목적이라
	 * 맵에 들어가 있기만 해도 15초 뒤 보스 패턴이 도는 것이 시험을 방해한다.
	 * 프로덕션 `ARaidPlayerController`의 계약은 건드리지 않고 이 훅만 덮는다.
	 */
	virtual bool ShouldAutoConfirmSelectionForServer() const override;

	/**
	 * 샌드박스는 DroneReport 확인에서 LobbyMap으로 나가지 않는다.
	 *
	 * 프로덕션은 확인 → LobbyMap이 맞지만, 샌드박스는 같은 맵에서 밸런스를 반복 시험하는 것이
	 * 목적이라 로비로 나가면 사이클이 끊긴다. 리포트만 닫고 BalanceMap에 남아 Reset으로 다음
	 * 회차를 돌 수 있게 한다. 서버의 리포트 생성·저장·중복 방지 상태는 건드리지 않는다 —
	 * 이미 만들어진 리포트는 그대로 남고, 닫았다고 다시 만들 수 있게 되지 않는다.
	 */
	virtual bool TryHandleDroneReportConfirmedForLocalPlayer(class UDroneReportWidget* ReportWidget) override;

private:
	class ABalanceSandboxGameMode* GetSandboxGameMode() const;
};
