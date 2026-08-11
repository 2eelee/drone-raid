#pragma once

#include "CoreMinimal.h"
#include "DroneCombatTypes.h"

class UDataTable;

enum class EDroneCombatDataFallbackReason : uint8
{
	None,
	MissingCoreTable,
	MissingWeaponTable,
	InvalidCoreRowStruct,
	InvalidWeaponRowStruct,
	MissingCoreRow,
	MissingWeaponRow,
	UnexpectedCoreRow,
	UnexpectedWeaponRow,
	InvalidCoreIdentity,
	InvalidWeaponIdentity,
	InvalidCoreEffectType,
	InvalidWeaponEffectType,
	DuplicateCoreType,
	DuplicateWeaponType,
	InvalidCoreRange,
	InvalidWeaponRange
};

struct FDroneCombatDataTableSet
{
	const UDataTable* Core = nullptr;
	const UDataTable* Weapon = nullptr;
};

namespace DroneCombatData
{
	DRONEPROTO_API bool TryResolve(
		const FDroneCombatDataTableSet& Tables,
		FDroneCombatResolvedConfig& OutConfig,
		EDroneCombatDataFallbackReason& OutReason);
	DRONEPROTO_API const TCHAR* ToString(EDroneCombatDataFallbackReason Reason);
}
