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
	explicit FRaidReservationLedger(int32 InMaxPlayers = 16, double InTokenLifetimeSeconds = 10.0);

	bool TryReserve(double NowSeconds, FString& OutToken);
	bool TryClaim(const FString& Token, double NowSeconds);
	bool TryCommitClaimed(const FString& Token);
	bool ReleaseActivePlayer();
	void Expire(double NowSeconds);
	int32 GetActivePlayers() const;
	int32 GetReservedPlayers(double NowSeconds);

private:
	void ExpireLocked(double NowSeconds);

	const int32 MaxPlayers;
	const double TokenLifetimeSeconds;
	mutable FCriticalSection Mutex;
	TMap<FString, FRaidReservationRecord> Records;
	int32 ActivePlayers = 0;
};
