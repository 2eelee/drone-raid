#pragma once

#include "CoreMinimal.h"
#include "BossPatternTypes.h"

class UDataTable;

enum class EBossPatternDataFallbackReason : uint8
{
	None,
	MissingBossPatternTable,
	MissingCorruptedTable,
	MissingCorruptedPresetTable,
	MissingStellarTable,
	MissingBossPatternRow,
	MissingCorruptedRow,
	MissingCorruptedPresetRow,
	MissingStellarRow,
	UnexpectedBossPatternRowCount,
	UnexpectedCorruptedRowCount,
	UnexpectedCorruptedPresetRowCount,
	UnexpectedStellarRowCount,
	DuplicatePresetLaserIndex,
	InvalidBossPatternContract,
	InvalidBossPatternRange,
	InvalidCorruptedContract,
	InvalidCorruptedRange,
	InvalidCorruptedPresetContract,
	InvalidStellarContract,
	InvalidStellarRange
};

struct FBossPatternDataTableSet
{
	const UDataTable* BossPattern = nullptr;
	const UDataTable* CorruptedActino = nullptr;
	const UDataTable* CorruptedActinoPreset = nullptr;
	const UDataTable* StellarRemnant = nullptr;
};

namespace BossPatternData
{
	DRONEPROTO_API bool TryResolve(
		const FBossPatternDataTableSet& Tables,
		FBossPatternResolvedConfig& OutConfig,
		EBossPatternDataFallbackReason& OutReason);
	DRONEPROTO_API const TCHAR* ToString(EBossPatternDataFallbackReason Reason);
}
