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
	explicit FRaidReservationLedger(
		int32 InMaxPlayers = 16,
		double InPendingLifetimeSeconds = 10.0,
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
