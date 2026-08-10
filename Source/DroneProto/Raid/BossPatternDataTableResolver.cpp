#include "BossPatternDataTableResolver.h"

#include "BossPatternDataTableRows.h"
#include "Engine/DataTable.h"

namespace
{
constexpr float MetersToCentimeters = 100.0f;
constexpr float CorruptedBoundaryRadiusMeters = 50.0f;

bool IsPositiveFinite(const float Value)
{
	return FMath::IsFinite(Value) && Value > 0.0f;
}

bool IsNonNegativeFinite(const float Value)
{
	return FMath::IsFinite(Value) && Value >= 0.0f;
}

bool NearlyEqual(const float A, const float B)
{
	return FMath::IsNearlyEqual(A, B, 0.001f);
}

template <typename RowType>
const RowType* FindRow(const UDataTable* Table, const TCHAR* RowName)
{
	return Table ? Table->FindRow<RowType>(FName(RowName), TEXT("BossPatternDataResolve"), false) : nullptr;
}

bool ValidatePresetContract(
	const FCorruptedActinoPresetRow* const Presets[4],
	FCorruptedActinoConfig& OutConfig)
{
	static const float ExpectedBaseAngles[4] = {0.0f, 90.0f, 180.0f, 270.0f};
	static const float ExpectedContractPhases[4] = {0.0f, 0.25f, 0.5f, 0.75f};
	static const TCHAR* ExpectedDirections[4] =
	{
		TEXT("Clockwise"), TEXT("CounterClockwise"), TEXT("Clockwise"), TEXT("CounterClockwise")
	};
	static const TCHAR* ExpectedZStates[4] =
	{
		TEXT("Upper"), TEXT("CenterUp"), TEXT("Lower"), TEXT("CenterDown")
	};
	static const float ExpectedXYRadians[4] = {-HALF_PI, HALF_PI, -HALF_PI, HALF_PI};
	static const float ExpectedZRadians[4] = {HALF_PI, 0.0f, -HALF_PI, PI};

	for (int32 Index = 0; Index < 4; ++Index)
	{
		const FCorruptedActinoPresetRow& Row = *Presets[Index];
		if (Row.LaserIndex != Index + 1
			|| !NearlyEqual(Row.BaseAngle, ExpectedBaseAngles[Index])
			|| !NearlyEqual(Row.XYPhase, ExpectedContractPhases[Index])
			|| !NearlyEqual(Row.ZPhase, ExpectedContractPhases[Index])
			|| Row.SweepDirection != ExpectedDirections[Index]
			|| Row.ZStartState != ExpectedZStates[Index])
		{
			return false;
		}

		OutConfig.Presets[Index] = FCorruptedActinoLaserPreset(
			Row.BaseAngle,
			ExpectedXYRadians[Index],
			ExpectedZRadians[Index]);
	}
	return true;
}
}

FBossPatternResolvedConfig MakeCanonicalBossPatternResolvedConfig()
{
	return FBossPatternResolvedConfig();
}

bool BossPatternData::TryResolve(
	const FBossPatternDataTableSet& Tables,
	FBossPatternResolvedConfig& OutConfig,
	EBossPatternDataFallbackReason& OutReason)
{
	auto Fail = [&OutReason](const EBossPatternDataFallbackReason Reason)
	{
		OutReason = Reason;
		return false;
	};

	if (!Tables.BossPattern) return Fail(EBossPatternDataFallbackReason::MissingBossPatternTable);
	if (!Tables.CorruptedActino) return Fail(EBossPatternDataFallbackReason::MissingCorruptedTable);
	if (!Tables.CorruptedActinoPreset) return Fail(EBossPatternDataFallbackReason::MissingCorruptedPresetTable);
	if (!Tables.StellarRemnant) return Fail(EBossPatternDataFallbackReason::MissingStellarTable);

	const FBossPatternSystemRow* BossRow = FindRow<FBossPatternSystemRow>(Tables.BossPattern, TEXT("BOSS_PATTERN_SYSTEM_001"));
	if (!BossRow) return Fail(EBossPatternDataFallbackReason::MissingBossPatternRow);
	const FCorruptedActinoRow* CorruptedRow = FindRow<FCorruptedActinoRow>(Tables.CorruptedActino, TEXT("PATTERN_001"));
	if (!CorruptedRow) return Fail(EBossPatternDataFallbackReason::MissingCorruptedRow);

	static const TCHAR* PresetRowNames[4] =
	{
		TEXT("ACTINO_LASER_01"), TEXT("ACTINO_LASER_02"), TEXT("ACTINO_LASER_03"), TEXT("ACTINO_LASER_04")
	};
	const FCorruptedActinoPresetRow* PresetRows[4] = {};
	for (int32 Index = 0; Index < 4; ++Index)
	{
		PresetRows[Index] = FindRow<FCorruptedActinoPresetRow>(Tables.CorruptedActinoPreset, PresetRowNames[Index]);
		if (!PresetRows[Index]) return Fail(EBossPatternDataFallbackReason::MissingCorruptedPresetRow);
	}
	const FStellarRemnantRow* StellarRow = FindRow<FStellarRemnantRow>(Tables.StellarRemnant, TEXT("PATTERN_002"));
	if (!StellarRow) return Fail(EBossPatternDataFallbackReason::MissingStellarRow);

	if (Tables.BossPattern->GetRowMap().Num() != 1) return Fail(EBossPatternDataFallbackReason::UnexpectedBossPatternRowCount);
	if (Tables.CorruptedActino->GetRowMap().Num() != 1) return Fail(EBossPatternDataFallbackReason::UnexpectedCorruptedRowCount);
	if (Tables.CorruptedActinoPreset->GetRowMap().Num() != 4) return Fail(EBossPatternDataFallbackReason::UnexpectedCorruptedPresetRowCount);
	if (Tables.StellarRemnant->GetRowMap().Num() != 1) return Fail(EBossPatternDataFallbackReason::UnexpectedStellarRowCount);

	TSet<int32> LaserIndices;
	for (const FCorruptedActinoPresetRow* Preset : PresetRows)
	{
		if (LaserIndices.Contains(Preset->LaserIndex))
		{
			return Fail(EBossPatternDataFallbackReason::DuplicatePresetLaserIndex);
		}
		LaserIndices.Add(Preset->LaserIndex);
	}

	if (BossRow->PatternSystemID != 10001
		|| BossRow->PatternOrder != TEXT("1\u21922 Repeat")
		|| BossRow->PlayerMaxHPReference != 100)
	{
		return Fail(EBossPatternDataFallbackReason::InvalidBossPatternContract);
	}
	if (!IsPositiveFinite(BossRow->PatternInterval)
		|| !IsNonNegativeFinite(BossRow->FirstPatternDelay)
		|| !IsPositiveFinite(BossRow->HitInvincibleDuration))
	{
		return Fail(EBossPatternDataFallbackReason::InvalidBossPatternRange);
	}

	if (CorruptedRow->PatternID != 11001
		|| CorruptedRow->PatternName != TEXT("Corrupted Actino")
		|| CorruptedRow->Difficulty != TEXT("Middle")
		|| CorruptedRow->LaserCount != 4
		|| CorruptedRow->FirstUseTelegraph)
	{
		return Fail(EBossPatternDataFallbackReason::InvalidCorruptedContract);
	}
	if (CorruptedRow->Damage <= 0
		|| !IsPositiveFinite(CorruptedRow->Duration)
		|| !IsNonNegativeFinite(CorruptedRow->Telegraph)
		|| !IsPositiveFinite(CorruptedRow->StartDistance)
		|| !IsPositiveFinite(CorruptedRow->EndDistance)
		|| !IsPositiveFinite(CorruptedRow->Length)
		|| !NearlyEqual(CorruptedRow->EndDistance - CorruptedRow->StartDistance, CorruptedRow->Length)
		|| !IsPositiveFinite(CorruptedRow->InnerHitWidth)
		|| !IsPositiveFinite(CorruptedRow->OuterHitWidth)
		|| CorruptedRow->OuterHitWidth < CorruptedRow->InnerHitWidth
		|| !IsPositiveFinite(CorruptedRow->InnerVisualWidth)
		|| !IsPositiveFinite(CorruptedRow->OuterVisualWidth)
		|| CorruptedRow->OuterVisualWidth < CorruptedRow->InnerVisualWidth
		|| !IsPositiveFinite(CorruptedRow->CollisionHeight)
		|| !IsPositiveFinite(CorruptedRow->ZAmplitude)
		|| !IsPositiveFinite(CorruptedRow->ZOscillationPeriod)
		|| !IsPositiveFinite(CorruptedRow->AngleSweepRange))
	{
		return Fail(EBossPatternDataFallbackReason::InvalidCorruptedRange);
	}

	FBossPatternResolvedConfig Candidate;
	if (!ValidatePresetContract(PresetRows, Candidate.Corrupted))
	{
		return Fail(EBossPatternDataFallbackReason::InvalidCorruptedPresetContract);
	}

	if (StellarRow->PatternID != 11002
		|| StellarRow->PatternName != TEXT("Stellar Remnant")
		|| StellarRow->Difficulty != TEXT("MiddleHigh")
		|| StellarRow->WaveCount != 2
		|| StellarRow->DamageCount != 32
		|| StellarRow->VisualCount != 16
		|| StellarRow->DamagePerWave != 16
		|| StellarRow->VisualPerWave != 8
		|| StellarRow->VisualDamage != 0)
	{
		return Fail(EBossPatternDataFallbackReason::InvalidStellarContract);
	}
	if (StellarRow->Damage <= 0
		|| !IsPositiveFinite(StellarRow->Duration)
		|| !IsNonNegativeFinite(StellarRow->Telegraph)
		|| !IsPositiveFinite(StellarRow->WaveInterval)
		|| !IsPositiveFinite(StellarRow->StartDistance)
		|| !IsPositiveFinite(StellarRow->EndDistance)
		|| !IsPositiveFinite(StellarRow->Length)
		|| !NearlyEqual(StellarRow->EndDistance - StellarRow->StartDistance, StellarRow->Length)
		|| !IsPositiveFinite(StellarRow->MoveDuration)
		|| !IsPositiveFinite(StellarRow->MoveSpeed)
		|| !NearlyEqual(StellarRow->MoveSpeed * StellarRow->MoveDuration, StellarRow->Length)
		|| !IsPositiveFinite(StellarRow->HitRadius)
		|| !IsPositiveFinite(StellarRow->VisualSizeMin)
		|| !IsPositiveFinite(StellarRow->VisualSizeMax)
		|| StellarRow->VisualSizeMax < StellarRow->VisualSizeMin
		|| !IsNonNegativeFinite(StellarRow->SecondWaveOffset)
		|| !IsNonNegativeFinite(StellarRow->VisualZOffset))
	{
		return Fail(EBossPatternDataFallbackReason::InvalidStellarRange);
	}

	Candidate.Common.FirstDelaySeconds = BossRow->FirstPatternDelay;
	Candidate.Common.IntermissionSeconds = BossRow->PatternInterval;
	Candidate.Common.GlobalHitLockSeconds = BossRow->HitInvincibleDuration;
	Candidate.Common.CorruptedDurationSeconds = CorruptedRow->Duration;
	Candidate.Common.CorruptedTelegraphSeconds = CorruptedRow->Telegraph;
	Candidate.Common.CorruptedDamage = CorruptedRow->Damage;
	Candidate.Common.StellarDurationSeconds = StellarRow->Duration;
	Candidate.Common.StellarTelegraphSeconds = StellarRow->Telegraph;
	Candidate.Common.StellarDamage = StellarRow->Damage;

	Candidate.Corrupted.LaserCount = CorruptedRow->LaserCount;
	Candidate.Corrupted.StartRadiusCm = CorruptedRow->StartDistance * MetersToCentimeters;
	const float ClampedEndDistanceMeters = FMath::Min(CorruptedRow->EndDistance, CorruptedBoundaryRadiusMeters);
	Candidate.Corrupted.EndRadiusCm = ClampedEndDistanceMeters * MetersToCentimeters;
	Candidate.Corrupted.LengthCm = (ClampedEndDistanceMeters - CorruptedRow->StartDistance) * MetersToCentimeters;
	Candidate.Corrupted.InnerCollisionFullWidthCm =
		FMath::Min(CorruptedRow->InnerHitWidth, CorruptedRow->InnerVisualWidth) * MetersToCentimeters;
	Candidate.Corrupted.OuterCollisionFullWidthCm =
		FMath::Min(CorruptedRow->OuterHitWidth, CorruptedRow->OuterVisualWidth) * MetersToCentimeters;
	Candidate.Corrupted.InnerVisualFullWidthCm = CorruptedRow->InnerVisualWidth * MetersToCentimeters;
	Candidate.Corrupted.OuterVisualFullWidthCm = CorruptedRow->OuterVisualWidth * MetersToCentimeters;
	Candidate.Corrupted.CollisionFullHeightCm = CorruptedRow->CollisionHeight * MetersToCentimeters;
	Candidate.Corrupted.ZAmplitudeCm = CorruptedRow->ZAmplitude * MetersToCentimeters;
	Candidate.Corrupted.ZPeriodSeconds = CorruptedRow->ZOscillationPeriod;
	Candidate.Corrupted.XYAmplitudeDegrees = CorruptedRow->AngleSweepRange;
	Candidate.Corrupted.XYPeriodSeconds = CorruptedRow->Duration;

	Candidate.Stellar.WaveCount = StellarRow->WaveCount;
	Candidate.Stellar.DamageProjectileCount = StellarRow->DamageCount;
	Candidate.Stellar.VisualProjectileCount = StellarRow->VisualCount;
	Candidate.Stellar.DamageProjectilesPerWave = StellarRow->DamagePerWave;
	Candidate.Stellar.VisualProjectilesPerWave = StellarRow->VisualPerWave;
	Candidate.Stellar.WaveIntervalSeconds = StellarRow->WaveInterval;
	Candidate.Stellar.StartRadiusCm = StellarRow->StartDistance * MetersToCentimeters;
	Candidate.Stellar.EndRadiusCm = StellarRow->EndDistance * MetersToCentimeters;
	Candidate.Stellar.LengthCm = StellarRow->Length * MetersToCentimeters;
	Candidate.Stellar.TravelSeconds = StellarRow->MoveDuration;
	Candidate.Stellar.SpeedCmPerSecond = StellarRow->MoveSpeed * MetersToCentimeters;
	Candidate.Stellar.CollisionRadiusCm = StellarRow->HitRadius * MetersToCentimeters;
	Candidate.Stellar.DamageAngleStepDegrees = 360.0f / static_cast<float>(StellarRow->DamagePerWave);
	Candidate.Stellar.SecondWaveOffsetDegrees = StellarRow->SecondWaveOffset;
	Candidate.Stellar.VisualZOffsetCm = StellarRow->VisualZOffset * MetersToCentimeters;
	Candidate.Stellar.VisualFullSizeMinCm = StellarRow->VisualSizeMin * MetersToCentimeters;
	Candidate.Stellar.VisualFullSizeMaxCm = StellarRow->VisualSizeMax * MetersToCentimeters;
	Candidate.Stellar.VisualDamage = StellarRow->VisualDamage;

	OutConfig = Candidate;
	OutReason = EBossPatternDataFallbackReason::None;
	return true;
}

const TCHAR* BossPatternData::ToString(const EBossPatternDataFallbackReason Reason)
{
#define BOSS_PATTERN_REASON_CASE(Name) case EBossPatternDataFallbackReason::Name: return TEXT(#Name)
	switch (Reason)
	{
	BOSS_PATTERN_REASON_CASE(None);
	BOSS_PATTERN_REASON_CASE(MissingBossPatternTable);
	BOSS_PATTERN_REASON_CASE(MissingCorruptedTable);
	BOSS_PATTERN_REASON_CASE(MissingCorruptedPresetTable);
	BOSS_PATTERN_REASON_CASE(MissingStellarTable);
	BOSS_PATTERN_REASON_CASE(MissingBossPatternRow);
	BOSS_PATTERN_REASON_CASE(MissingCorruptedRow);
	BOSS_PATTERN_REASON_CASE(MissingCorruptedPresetRow);
	BOSS_PATTERN_REASON_CASE(MissingStellarRow);
	BOSS_PATTERN_REASON_CASE(UnexpectedBossPatternRowCount);
	BOSS_PATTERN_REASON_CASE(UnexpectedCorruptedRowCount);
	BOSS_PATTERN_REASON_CASE(UnexpectedCorruptedPresetRowCount);
	BOSS_PATTERN_REASON_CASE(UnexpectedStellarRowCount);
	BOSS_PATTERN_REASON_CASE(DuplicatePresetLaserIndex);
	BOSS_PATTERN_REASON_CASE(InvalidBossPatternContract);
	BOSS_PATTERN_REASON_CASE(InvalidBossPatternRange);
	BOSS_PATTERN_REASON_CASE(InvalidCorruptedContract);
	BOSS_PATTERN_REASON_CASE(InvalidCorruptedRange);
	BOSS_PATTERN_REASON_CASE(InvalidCorruptedPresetContract);
	BOSS_PATTERN_REASON_CASE(InvalidStellarContract);
	BOSS_PATTERN_REASON_CASE(InvalidStellarRange);
	default: return TEXT("Unknown");
	}
#undef BOSS_PATTERN_REASON_CASE
}
