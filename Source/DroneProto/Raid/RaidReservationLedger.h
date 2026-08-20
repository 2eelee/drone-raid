#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"

enum class ERaidReservationRecordState : uint8
{
	Pending,
	Claimed,
};

struct FRaidReservationRecord
{
	ERaidReservationRecordState State = ERaidReservationRecordState::Pending;
	double ExpiresAtSeconds = 0.0;
};

class DRONEPROTO_API FRaidReservationLedger
{
public:
	// PendingLifetime은 예약을 발급받고도 서버에 접속하지 않는 클라이언트를 회수하는 시간이고,
	// ClaimedLifetime은 PreLogin을 통과한 뒤 맵을 로드하는 동안 예약을 붙잡아 두는 시간이다.
	// 두 구간의 길이가 전혀 다르므로 수명도 분리한다(ENTRY-15).
	// PendingLifetime 30초는 2026-08-20 Dedicated Server 실환경 검증 결과다 — 10초일 때
	// 패키징 클라이언트의 콜드 스타트가 메모리 압박에서 13.4초까지 늘어 정상 플레이어가
	// 만료로 거부됐다(정상 상태 5.2초). claimed를 10초에서 120초로 늘린 것과 같은 축이다.
	explicit FRaidReservationLedger(
		int32 InMaxPlayers = 16,
		double InPendingLifetimeSeconds = 30.0,
		double InClaimedLifetimeSeconds = 120.0);

	bool TryReserve(double NowSeconds, FString& OutToken);
	bool TryClaim(const FString& Token, double NowSeconds);
	bool TryCommitClaimed(const FString& Token);
	bool ReleaseReservation(const FString& Token);
	void GetClaimedTokens(TArray<FString>& OutTokens) const;
	bool ReleaseActivePlayer();
	void Expire(double NowSeconds);
	int32 GetActivePlayers() const;
	int32 GetReservedPlayers(double NowSeconds);

private:
	void ExpireLocked(double NowSeconds);

	const int32 MaxPlayers;
	const double PendingLifetimeSeconds;
	const double ClaimedLifetimeSeconds;
	mutable FCriticalSection Mutex;
	TMap<FString, FRaidReservationRecord> Records;
	int32 ActivePlayers = 0;
};
