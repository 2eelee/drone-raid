#pragma once

#include "CoreMinimal.h"
#include "DroneCombatTypes.h"

class UDataTable;

enum class EDroneReportDataFallbackReason : uint8
{
	None,
	MissingBonusTable,
	MissingSettingsTable,
	MissingGradeTable,
	InvalidBonusRowStruct,
	InvalidSettingsRowStruct,
	InvalidGradeRowStruct,
	MissingBonusRow,
	MissingSettingsRow,
	MissingGradeRow,
	UnexpectedBonusRow,
	UnexpectedSettingsRow,
	UnexpectedGradeRow,
	InvalidBonusIdentity,
	DuplicateBonusType,
	InvalidBonusRange,
	InvalidSettingsRange,
	DuplicateGrade,
	InvalidGradeRange
};

struct FDroneReportDataTableSet
{
	const UDataTable* Bonus = nullptr;
	const UDataTable* Settings = nullptr;
	const UDataTable* Grade = nullptr;
};

namespace DroneReportData
{
	DRONEPROTO_API bool TryResolve(
		const FDroneReportDataTableSet& Tables,
		FDroneReportResolvedConfig& OutConfig,
		EDroneReportDataFallbackReason& OutReason);
	DRONEPROTO_API const TCHAR* ToString(EDroneReportDataFallbackReason Reason);
}
