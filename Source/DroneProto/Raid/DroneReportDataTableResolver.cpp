#include "DroneReportDataTableResolver.h"

#include "DroneDataTableRows.h"
#include "Engine/DataTable.h"

namespace
{
struct FExpectedBonusRow
{
	const TCHAR* RowName;
	int32 BonusID;
	const TCHAR* BonusName;
	EDroneReportBonusType Type;
};

constexpr FExpectedBonusRow ExpectedBonusRows[] =
{
	{ TEXT("BONUS_001"), 5001, TEXT("BossSlayer"), EDroneReportBonusType::BossSlayer },
	{ TEXT("BONUS_002"), 5002, TEXT("HighDPS"), EDroneReportBonusType::HighDPS },
	{ TEXT("BONUS_003"), 5003, TEXT("NoDamage"), EDroneReportBonusType::NoDamage },
	{ TEXT("BONUS_004"), 5004, TEXT("KeepMoving"), EDroneReportBonusType::KeepMoving },
	{ TEXT("BONUS_005"), 5005, TEXT("HighRecovery"), EDroneReportBonusType::HighRecovery }
};

struct FExpectedGradeRow
{
	const TCHAR* RowName;
	EDroneReportGrade Grade;
};

constexpr FExpectedGradeRow ExpectedGradeRows[] =
{
	{ TEXT("GRADE_S"), EDroneReportGrade::S },
	{ TEXT("GRADE_A"), EDroneReportGrade::A },
	{ TEXT("GRADE_B"), EDroneReportGrade::B },
	{ TEXT("GRADE_C"), EDroneReportGrade::C }
};

template <typename RowType>
const RowType* FindRow(const UDataTable* Table, const TCHAR* RowName)
{
	return Table ? Table->FindRow<RowType>(FName(RowName), TEXT("DroneReportDataResolve"), false) : nullptr;
}

bool IsRatio(float Value)
{
	return FMath::IsFinite(Value) && Value >= 0.0f && Value <= 1.0f;
}

bool IsNonNegative(float Value)
{
	return FMath::IsFinite(Value) && Value >= 0.0f;
}

bool IsValidBonusRange(const FDroneBonusRow& Row)
{
	return Row.BonusScore >= 0
		&& Row.SecondaryBonusScore >= 0
		&& Row.MaxScore >= FMath::Max(Row.BonusScore, Row.SecondaryBonusScore)
		&& IsNonNegative(Row.MinCombatDuration)
		&& IsNonNegative(Row.SecondaryMinCombatDuration)
		&& IsRatio(Row.MinBossDamageRatio)
		&& IsRatio(Row.SecondaryMinBossDamageRatio)
		&& IsNonNegative(Row.PrimaryMinDamagePerMinute)
		&& IsNonNegative(Row.SecondaryMinDamagePerMinute)
		&& IsNonNegative(Row.PrimaryMinMoveDistance)
		&& IsNonNegative(Row.SecondaryMinMovePerMinute)
		&& IsNonNegative(Row.PrimaryMinHealAmount)
		&& IsNonNegative(Row.SecondaryMinHealAmount)
		&& IsRatio(Row.LateJoinBossHPThresholdRatio)
		&& Row.MaxDamageTakenCount >= -1
		&& !Row.BonusDisplayName.IsEmpty();
}

FDroneReportBonusRule MakeBonusRule(const FDroneBonusRow& Row, EDroneReportBonusType Type)
{
	FDroneReportBonusRule Rule;
	Rule.Type = Type;
	Rule.DisplayName = Row.BonusDisplayName;
	Rule.PrimaryScore = Row.BonusScore;
	Rule.SecondaryScore = Row.SecondaryBonusScore;
	Rule.PrimaryMinCombatDuration = Row.MinCombatDuration;
	Rule.SecondaryMinCombatDuration = Row.SecondaryMinCombatDuration;
	Rule.PrimaryMinBossDamageRatio = Row.MinBossDamageRatio;
	Rule.SecondaryMinBossDamageRatio = Row.SecondaryMinBossDamageRatio;
	Rule.PrimaryMinDamagePerMinute = Row.PrimaryMinDamagePerMinute;
	Rule.SecondaryMinDamagePerMinute = Row.SecondaryMinDamagePerMinute;
	Rule.PrimaryMinMoveDistance = Row.PrimaryMinMoveDistance;
	Rule.SecondaryMinMovePerMinute = Row.SecondaryMinMovePerMinute;
	Rule.PrimaryMinHealAmount = Row.PrimaryMinHealAmount;
	Rule.SecondaryMinHealAmount = Row.SecondaryMinHealAmount;
	Rule.LateJoinBossHPThresholdRatio = Row.LateJoinBossHPThresholdRatio;
	Rule.MaxDamageTakenCount = Row.MaxDamageTakenCount;
	Rule.MaxScore = Row.MaxScore;
	Rule.bRequiresBossDefeated = Row.bRequiresBossDefeated;
	Rule.bRequiresAlive = Row.bRequiresAlive;
	return Rule;
}
}

bool DroneReportData::TryResolve(
	const FDroneReportDataTableSet& Tables,
	FDroneReportResolvedConfig& OutConfig,
	EDroneReportDataFallbackReason& OutReason)
{
	auto Fail = [&OutReason](EDroneReportDataFallbackReason Reason)
	{
		OutReason = Reason;
		return false;
	};

	if (!Tables.Bonus) return Fail(EDroneReportDataFallbackReason::MissingBonusTable);
	if (!Tables.Settings) return Fail(EDroneReportDataFallbackReason::MissingSettingsTable);
	if (!Tables.Grade) return Fail(EDroneReportDataFallbackReason::MissingGradeTable);
	if (Tables.Bonus->GetRowStruct() != FDroneBonusRow::StaticStruct()) return Fail(EDroneReportDataFallbackReason::InvalidBonusRowStruct);
	if (Tables.Settings->GetRowStruct() != FDroneReportSettingsRow::StaticStruct()) return Fail(EDroneReportDataFallbackReason::InvalidSettingsRowStruct);
	if (Tables.Grade->GetRowStruct() != FDroneGradeRow::StaticStruct()) return Fail(EDroneReportDataFallbackReason::InvalidGradeRowStruct);

	FDroneReportResolvedConfig Candidate;
	TSet<EDroneReportBonusType> SeenBonusTypes;
	for (const FExpectedBonusRow& Expected : ExpectedBonusRows)
	{
		const FDroneBonusRow* Row = FindRow<FDroneBonusRow>(Tables.Bonus, Expected.RowName);
		if (!Row) return Fail(EDroneReportDataFallbackReason::MissingBonusRow);
		if (Row->BonusID != Expected.BonusID || Row->BonusName != FName(Expected.BonusName)) return Fail(EDroneReportDataFallbackReason::InvalidBonusIdentity);
		if (SeenBonusTypes.Contains(Expected.Type)) return Fail(EDroneReportDataFallbackReason::DuplicateBonusType);
		if (!IsValidBonusRange(*Row)) return Fail(EDroneReportDataFallbackReason::InvalidBonusRange);
		SeenBonusTypes.Add(Expected.Type);
		Candidate.BonusRules.Add(MakeBonusRule(*Row, Expected.Type));
	}
	if (Tables.Bonus->GetRowMap().Num() != UE_ARRAY_COUNT(ExpectedBonusRows)) return Fail(EDroneReportDataFallbackReason::UnexpectedBonusRow);

	const FDroneReportSettingsRow* Settings = FindRow<FDroneReportSettingsRow>(Tables.Settings, TEXT("REPORT_SETTINGS"));
	if (!Settings) return Fail(EDroneReportDataFallbackReason::MissingSettingsRow);
	if (Tables.Settings->GetRowMap().Num() != 1) return Fail(EDroneReportDataFallbackReason::UnexpectedSettingsRow);
	if (Settings->BonusScoreCap < 0) return Fail(EDroneReportDataFallbackReason::InvalidSettingsRange);
	Candidate.BonusScoreCap = Settings->BonusScoreCap;

	TSet<EDroneReportGrade> SeenGrades;
	for (const FExpectedGradeRow& Expected : ExpectedGradeRows)
	{
		const FDroneGradeRow* Row = FindRow<FDroneGradeRow>(Tables.Grade, Expected.RowName);
		if (!Row) return Fail(EDroneReportDataFallbackReason::MissingGradeRow);
		if (Row->Grade != Expected.Grade || SeenGrades.Contains(Row->Grade)) return Fail(EDroneReportDataFallbackReason::DuplicateGrade);
		if (!IsNonNegative(Row->MinScore) || !IsNonNegative(Row->MaxScore) || Row->MinScore > Row->MaxScore) return Fail(EDroneReportDataFallbackReason::InvalidGradeRange);
		SeenGrades.Add(Row->Grade);
		Candidate.GradeRules.Add({ Row->Grade, Row->MinScore, Row->MaxScore });
	}
	if (Tables.Grade->GetRowMap().Num() != UE_ARRAY_COUNT(ExpectedGradeRows)) return Fail(EDroneReportDataFallbackReason::UnexpectedGradeRow);
	for (int32 Index = 1; Index < Candidate.GradeRules.Num(); ++Index)
	{
		if (Candidate.GradeRules[Index - 1].MinScore <= Candidate.GradeRules[Index].MinScore)
		{
			return Fail(EDroneReportDataFallbackReason::InvalidGradeRange);
		}
	}

	OutConfig = MoveTemp(Candidate);
	OutReason = EDroneReportDataFallbackReason::None;
	return true;
}

const TCHAR* DroneReportData::ToString(EDroneReportDataFallbackReason Reason)
{
	switch (Reason)
	{
	case EDroneReportDataFallbackReason::None: return TEXT("None");
	case EDroneReportDataFallbackReason::MissingBonusTable: return TEXT("MissingBonusTable");
	case EDroneReportDataFallbackReason::MissingSettingsTable: return TEXT("MissingSettingsTable");
	case EDroneReportDataFallbackReason::MissingGradeTable: return TEXT("MissingGradeTable");
	case EDroneReportDataFallbackReason::InvalidBonusRowStruct: return TEXT("InvalidBonusRowStruct");
	case EDroneReportDataFallbackReason::InvalidSettingsRowStruct: return TEXT("InvalidSettingsRowStruct");
	case EDroneReportDataFallbackReason::InvalidGradeRowStruct: return TEXT("InvalidGradeRowStruct");
	case EDroneReportDataFallbackReason::MissingBonusRow: return TEXT("MissingBonusRow");
	case EDroneReportDataFallbackReason::MissingSettingsRow: return TEXT("MissingSettingsRow");
	case EDroneReportDataFallbackReason::MissingGradeRow: return TEXT("MissingGradeRow");
	case EDroneReportDataFallbackReason::UnexpectedBonusRow: return TEXT("UnexpectedBonusRow");
	case EDroneReportDataFallbackReason::UnexpectedSettingsRow: return TEXT("UnexpectedSettingsRow");
	case EDroneReportDataFallbackReason::UnexpectedGradeRow: return TEXT("UnexpectedGradeRow");
	case EDroneReportDataFallbackReason::InvalidBonusIdentity: return TEXT("InvalidBonusIdentity");
	case EDroneReportDataFallbackReason::DuplicateBonusType: return TEXT("DuplicateBonusType");
	case EDroneReportDataFallbackReason::InvalidBonusRange: return TEXT("InvalidBonusRange");
	case EDroneReportDataFallbackReason::InvalidSettingsRange: return TEXT("InvalidSettingsRange");
	case EDroneReportDataFallbackReason::DuplicateGrade: return TEXT("DuplicateGrade");
	case EDroneReportDataFallbackReason::InvalidGradeRange: return TEXT("InvalidGradeRange");
	default: return TEXT("Unknown");
	}
}
