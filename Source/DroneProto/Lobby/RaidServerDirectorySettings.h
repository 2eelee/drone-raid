#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RaidServerDirectorySettings.generated.h"

USTRUCT()
struct DRONEPROTO_API FRaidServerDefinition
{
	GENERATED_BODY()

	FRaidServerDefinition() = default;
	FRaidServerDefinition(FString InSlotId, int32 InPriority, FString InReservationUrl)
		: SlotId(MoveTemp(InSlotId))
		, Priority(InPriority)
		, ReservationUrl(MoveTemp(InReservationUrl))
	{
	}

	UPROPERTY(Config)
	FString SlotId;

	UPROPERTY(Config)
	int32 Priority = 0;

	UPROPERTY(Config)
	FString ReservationUrl;
};

UCLASS(Config=Engine, DefaultConfig)
class DRONEPROTO_API URaidServerDirectorySettings : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(Config)
	TArray<FRaidServerDefinition> Servers;

	/**
	 * 개발 전용 스위치다. 켜면 `URaidSessionSubsystem`이 예약 HTTP를 거치지 않고 `ULocalAssignment`를 쓴다.
	 *
	 * 예약 서비스는 `NM_DedicatedServer`에서만 뜨므로(`RaidGameMode.cpp`의 admission 분기) 단일 PIE에서는
	 * `127.0.0.1:7787~7789`에 아무도 없어 항상 `NoServerAvailable`이 되고 로비가 매칭 실패 팝업을 띄운다.
	 * 이 스위치는 그 구간만 우회해 로비 → 부품 선택 → 레이드 → 리포트 흐름을 PIE에서 볼 수 있게 한다.
	 *
	 * **Shipping 빌드에서는 이 값과 무관하게 항상 무시된다**(`URaidSessionSubsystem::ShouldUseLocalAssignment`).
	 * 기본값은 꺼짐이며 `-LocalRaidAssignment` 커맨드라인으로도 켤 수 있다.
	 */
	UPROPERTY(Config)
	bool bUseLocalAssignment = false;
};
