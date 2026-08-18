#include "RaidReservationLedger.h"

#include "Misc/Guid.h"
#include "Misc/ScopeLock.h"

FRaidReservationLedger::FRaidReservationLedger(
	int32 InMaxPlayers,
	double InPendingLifetimeSeconds,
	double InClaimedLifetimeSeconds)
	: MaxPlayers(FMath::Max(1, InMaxPlayers))
	, PendingLifetimeSeconds(FMath::Max(0.01, InPendingLifetimeSeconds))
	, ClaimedLifetimeSeconds(FMath::Max(0.01, InClaimedLifetimeSeconds))
{
}

bool FRaidReservationLedger::TryReserve(double NowSeconds, FString& OutToken)
{
	FScopeLock Lock(&Mutex);
	ExpireLocked(NowSeconds);
	if (ActivePlayers + Records.Num() >= MaxPlayers)
	{
		OutToken.Reset();
		return false;
	}

	do
	{
		OutToken = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	}
	while (Records.Contains(OutToken));

	FRaidReservationRecord& Record = Records.Add(OutToken);
	Record.ExpiresAtSeconds = NowSeconds + PendingLifetimeSeconds;
	return true;
}

bool FRaidReservationLedger::TryClaim(const FString& Token, double NowSeconds)
{
	FScopeLock Lock(&Mutex);
	ExpireLocked(NowSeconds);
	FRaidReservationRecord* Record = Records.Find(Token);
	if (!Record || Record->State != ERaidReservationRecordState::Pending)
	{
		return false;
	}

	Record->State = ERaidReservationRecordState::Claimed;
	Record->ExpiresAtSeconds = NowSeconds + ClaimedLifetimeSeconds;
	return true;
}

bool FRaidReservationLedger::TryCommitClaimed(const FString& Token)
{
	FScopeLock Lock(&Mutex);
	const FRaidReservationRecord* Record = Records.Find(Token);
	if (!Record || Record->State != ERaidReservationRecordState::Claimed)
	{
		return false;
	}

	Records.Remove(Token);
	++ActivePlayers;
	return true;
}

bool FRaidReservationLedger::ReleaseReservation(const FString& Token)
{
	FScopeLock Lock(&Mutex);
	return Records.Remove(Token) > 0;
}

void FRaidReservationLedger::GetClaimedTokens(TArray<FString>& OutTokens) const
{
	FScopeLock Lock(&Mutex);
	for (const TPair<FString, FRaidReservationRecord>& Pair : Records)
	{
		if (Pair.Value.State == ERaidReservationRecordState::Claimed)
		{
			OutTokens.Add(Pair.Key);
		}
	}
}

bool FRaidReservationLedger::ReleaseActivePlayer()
{
	FScopeLock Lock(&Mutex);
	if (ActivePlayers <= 0)
	{
		return false;
	}

	--ActivePlayers;
	return true;
}

void FRaidReservationLedger::Expire(double NowSeconds)
{
	FScopeLock Lock(&Mutex);
	ExpireLocked(NowSeconds);
}

int32 FRaidReservationLedger::GetActivePlayers() const
{
	FScopeLock Lock(&Mutex);
	return ActivePlayers;
}

int32 FRaidReservationLedger::GetReservedPlayers(double NowSeconds)
{
	FScopeLock Lock(&Mutex);
	ExpireLocked(NowSeconds);
	return Records.Num();
}

void FRaidReservationLedger::ExpireLocked(double NowSeconds)
{
	for (auto It = Records.CreateIterator(); It; ++It)
	{
		if (It.Value().ExpiresAtSeconds <= NowSeconds)
		{
			It.RemoveCurrent();
		}
	}
}
