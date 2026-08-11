#include "DroneCombatDataTableResolver.h"

#include "DroneDataTableRows.h"
#include "Engine/DataTable.h"

namespace
{
struct FExpectedCoreRow
{
	const TCHAR* RowName;
	const TCHAR* CoreID;
	const TCHAR* EffectType;
	EDroneCombatCoreType Type;
};

constexpr FExpectedCoreRow ExpectedCoreRows[] =
{
	{ TEXT("CORE_001"), TEXT("1001"), TEXT("HP_TO_ATTACK"), EDroneCombatCoreType::Zenith },
	{ TEXT("CORE_002"), TEXT("1002"), TEXT("MOVE_TO_SPEED_ATTACK"), EDroneCombatCoreType::Booster },
	{ TEXT("CORE_003"), TEXT("1003"), TEXT("DAMAGE_TO_HEAL"), EDroneCombatCoreType::Drain }
};

struct FExpectedWeaponRow
{
	const TCHAR* RowName;
	const TCHAR* WeaponID;
	const TCHAR* EffectType;
	EDroneCombatWeaponType Type;
};

constexpr FExpectedWeaponRow ExpectedWeaponRows[] =
{
	{ TEXT("WEAPON_001"), TEXT("2001"), TEXT("THIRD_HIT_STRONG"), EDroneCombatWeaponType::PulseLaser },
	{ TEXT("WEAPON_002"), TEXT("2002"), TEXT("FRACTURE_MULTI_HIT"), EDroneCombatWeaponType::FractureBurst },
	{ TEXT("WEAPON_003"), TEXT("2003"), TEXT("MOVE_DISTANCE_DAMAGE"), EDroneCombatWeaponType::VectorCannon }
};

template <typename RowType>
const RowType* FindRow(const UDataTable* Table, const TCHAR* RowName)
{
	return Table ? Table->FindRow<RowType>(FName(RowName), TEXT("DroneCombatDataResolve"), false) : nullptr;
}

bool IsNonNegative(float Value)
{
	return FMath::IsFinite(Value) && Value >= 0.0f;
}

bool IsPositive(float Value)
{
	return FMath::IsFinite(Value) && Value > 0.0f;
}

bool IsNonNegativeInteger(float Value)
{
	return IsNonNegative(Value) && FMath::IsNearlyEqual(Value, FMath::RoundToFloat(Value));
}

bool IsValidCoreRange(const FDroneCoreRow& Row, EDroneCombatCoreType Type)
{
	if (!IsPositive(Row.AttackModifier)
		|| !IsPositive(Row.MoveSpeedModifier)
		|| !IsNonNegative(Row.EffectValue01)
		|| !IsNonNegative(Row.EffectValue02)
		|| !IsNonNegative(Row.EffectMaxValue))
	{
		return false;
	}

	switch (Type)
	{
	case EDroneCombatCoreType::Zenith:
	case EDroneCombatCoreType::Booster:
		return Row.EffectValue02 > 0.0f;
	case EDroneCombatCoreType::Drain:
		return true;
	default:
		return false;
	}
}

bool IsValidWeaponRange(const FDroneWeaponRow& Row, EDroneCombatWeaponType Type)
{
	if (!IsNonNegative(Row.BaseDamage)
		|| !IsNonNegative(Row.SpecialValue01)
		|| !IsNonNegative(Row.SpecialValue02)
		|| !IsNonNegative(Row.SpecialMaxValue)
		|| Row.HitCount <= 0)
	{
		return false;
	}

	switch (Type)
	{
	case EDroneCombatWeaponType::PulseLaser:
		return Row.SpecialValue01 >= 1.0f && IsNonNegativeInteger(Row.SpecialValue01);
	case EDroneCombatWeaponType::FractureBurst:
		return IsNonNegativeInteger(Row.SpecialValue01)
			&& Row.HitCount == FMath::RoundToInt(Row.SpecialValue01) + 1;
	case EDroneCombatWeaponType::VectorCannon:
		return Row.SpecialValue01 > 0.0f;
	default:
		return false;
	}
}

FDroneCoreRule MakeCoreRule(const FDroneCoreRow& Row, EDroneCombatCoreType Type)
{
	return { Type, Row.AttackModifier, Row.MoveSpeedModifier, Row.EffectValue01, Row.EffectValue02, Row.EffectMaxValue };
}

FDroneWeaponRule MakeWeaponRule(const FDroneWeaponRow& Row, EDroneCombatWeaponType Type)
{
	return { Type, Row.BaseDamage, Row.SpecialValue01, Row.SpecialValue02, Row.SpecialMaxValue, Row.HitCount };
}
}

bool DroneCombatData::TryResolve(
	const FDroneCombatDataTableSet& Tables,
	FDroneCombatResolvedConfig& OutConfig,
	EDroneCombatDataFallbackReason& OutReason)
{
	auto Fail = [&OutReason](EDroneCombatDataFallbackReason Reason)
	{
		OutReason = Reason;
		return false;
	};

	if (!Tables.Core) return Fail(EDroneCombatDataFallbackReason::MissingCoreTable);
	if (!Tables.Weapon) return Fail(EDroneCombatDataFallbackReason::MissingWeaponTable);
	if (Tables.Core->GetRowStruct() != FDroneCoreRow::StaticStruct()) return Fail(EDroneCombatDataFallbackReason::InvalidCoreRowStruct);
	if (Tables.Weapon->GetRowStruct() != FDroneWeaponRow::StaticStruct()) return Fail(EDroneCombatDataFallbackReason::InvalidWeaponRowStruct);

	FDroneCombatResolvedConfig Candidate;
	TSet<EDroneCombatCoreType> SeenCoreTypes;
	for (const FExpectedCoreRow& Expected : ExpectedCoreRows)
	{
		const FDroneCoreRow* Row = FindRow<FDroneCoreRow>(Tables.Core, Expected.RowName);
		if (!Row) return Fail(EDroneCombatDataFallbackReason::MissingCoreRow);
		if (Row->CoreID != FName(Expected.CoreID)) return Fail(EDroneCombatDataFallbackReason::InvalidCoreIdentity);
		if (Row->EffectType != FName(Expected.EffectType)) return Fail(EDroneCombatDataFallbackReason::InvalidCoreEffectType);
		if (SeenCoreTypes.Contains(Expected.Type)) return Fail(EDroneCombatDataFallbackReason::DuplicateCoreType);
		if (!IsValidCoreRange(*Row, Expected.Type)) return Fail(EDroneCombatDataFallbackReason::InvalidCoreRange);
		SeenCoreTypes.Add(Expected.Type);
		Candidate.CoreRules.Add(MakeCoreRule(*Row, Expected.Type));
	}
	if (Tables.Core->GetRowMap().Num() != UE_ARRAY_COUNT(ExpectedCoreRows)) return Fail(EDroneCombatDataFallbackReason::UnexpectedCoreRow);

	TSet<EDroneCombatWeaponType> SeenWeaponTypes;
	for (const FExpectedWeaponRow& Expected : ExpectedWeaponRows)
	{
		const FDroneWeaponRow* Row = FindRow<FDroneWeaponRow>(Tables.Weapon, Expected.RowName);
		if (!Row) return Fail(EDroneCombatDataFallbackReason::MissingWeaponRow);
		if (Row->WeaponID != FName(Expected.WeaponID)) return Fail(EDroneCombatDataFallbackReason::InvalidWeaponIdentity);
		if (Row->SpecialEffectType != FName(Expected.EffectType)) return Fail(EDroneCombatDataFallbackReason::InvalidWeaponEffectType);
		if (SeenWeaponTypes.Contains(Expected.Type)) return Fail(EDroneCombatDataFallbackReason::DuplicateWeaponType);
		if (!IsValidWeaponRange(*Row, Expected.Type)) return Fail(EDroneCombatDataFallbackReason::InvalidWeaponRange);
		SeenWeaponTypes.Add(Expected.Type);
		Candidate.WeaponRules.Add(MakeWeaponRule(*Row, Expected.Type));
	}
	if (Tables.Weapon->GetRowMap().Num() != UE_ARRAY_COUNT(ExpectedWeaponRows)) return Fail(EDroneCombatDataFallbackReason::UnexpectedWeaponRow);

	OutConfig = MoveTemp(Candidate);
	OutReason = EDroneCombatDataFallbackReason::None;
	return true;
}

const TCHAR* DroneCombatData::ToString(EDroneCombatDataFallbackReason Reason)
{
	switch (Reason)
	{
	case EDroneCombatDataFallbackReason::None: return TEXT("None");
	case EDroneCombatDataFallbackReason::MissingCoreTable: return TEXT("MissingCoreTable");
	case EDroneCombatDataFallbackReason::MissingWeaponTable: return TEXT("MissingWeaponTable");
	case EDroneCombatDataFallbackReason::InvalidCoreRowStruct: return TEXT("InvalidCoreRowStruct");
	case EDroneCombatDataFallbackReason::InvalidWeaponRowStruct: return TEXT("InvalidWeaponRowStruct");
	case EDroneCombatDataFallbackReason::MissingCoreRow: return TEXT("MissingCoreRow");
	case EDroneCombatDataFallbackReason::MissingWeaponRow: return TEXT("MissingWeaponRow");
	case EDroneCombatDataFallbackReason::UnexpectedCoreRow: return TEXT("UnexpectedCoreRow");
	case EDroneCombatDataFallbackReason::UnexpectedWeaponRow: return TEXT("UnexpectedWeaponRow");
	case EDroneCombatDataFallbackReason::InvalidCoreIdentity: return TEXT("InvalidCoreIdentity");
	case EDroneCombatDataFallbackReason::InvalidWeaponIdentity: return TEXT("InvalidWeaponIdentity");
	case EDroneCombatDataFallbackReason::InvalidCoreEffectType: return TEXT("InvalidCoreEffectType");
	case EDroneCombatDataFallbackReason::InvalidWeaponEffectType: return TEXT("InvalidWeaponEffectType");
	case EDroneCombatDataFallbackReason::DuplicateCoreType: return TEXT("DuplicateCoreType");
	case EDroneCombatDataFallbackReason::DuplicateWeaponType: return TEXT("DuplicateWeaponType");
	case EDroneCombatDataFallbackReason::InvalidCoreRange: return TEXT("InvalidCoreRange");
	case EDroneCombatDataFallbackReason::InvalidWeaponRange: return TEXT("InvalidWeaponRange");
	default: return TEXT("Unknown");
	}
}
