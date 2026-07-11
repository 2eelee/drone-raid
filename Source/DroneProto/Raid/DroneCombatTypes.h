#pragma once

#include "CoreMinimal.h"
#include "DroneCombatTypes.generated.h"

UENUM(BlueprintType)
enum class EDroneCombatCoreType : uint8
{
	None,
	Zenith,
	Booster,
	Drain,
};

UENUM(BlueprintType)
enum class EDroneCombatWeaponType : uint8
{
	None,
	PulseLaser,
	FractureBurst,
	VectorCannon,
};

UENUM(BlueprintType)
enum class EDroneCombatWeaponSlot : uint8
{
	Left,
	Right,
};

UENUM(BlueprintType)
enum class EDroneReportBonusType : uint8
{
	BossSlayer,
	HighDPS,
	NoDamage,
	KeepMoving,
	HighRecovery,
};

UENUM(BlueprintType)
enum class EDroneReportGrade : uint8
{
	S,
	A,
	B,
	C,
};

UENUM(BlueprintType)
enum class EDroneReportTrigger : uint8
{
	Death,
	BossDefeated,
	RaidTimeLimit,
};

USTRUCT(BlueprintType)
struct DRONEPROTO_API FDroneWeaponCalculationInput
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Drone|Combat")
	EDroneCombatWeaponType WeaponType = EDroneCombatWeaponType::None;

	UPROPERTY(BlueprintReadWrite, Category = "Drone|Combat")
	EDroneCombatWeaponSlot Slot = EDroneCombatWeaponSlot::Left;

	UPROPERTY(BlueprintReadWrite, Category = "Drone|Combat")
	int32 PulseAttackCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Drone|Combat")
	float VectorAccumulatedMoveDistanceMeters = 0.0f;
};

USTRUCT(BlueprintType)
struct DRONEPROTO_API FDroneWeaponCalculationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Combat")
	float WeaponDamage = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Combat")
	float BaseDamage = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Combat")
	float BonusDamage = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Combat")
	int32 HitCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Combat")
	int32 AdditionalHitCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Combat")
	int32 PulseAttackCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Combat")
	float VectorDistance = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Combat")
	bool bResetVectorDistance = false;
};

USTRUCT(BlueprintType)
struct DRONEPROTO_API FDroneCoreCalculationInput
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Drone|Combat")
	EDroneCombatCoreType CoreType = EDroneCombatCoreType::None;

	UPROPERTY(BlueprintReadWrite, Category = "Drone|Combat")
	float CurrentHP = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Drone|Combat")
	float MaxHP = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Drone|Combat")
	float AccumulatedMoveDistanceMeters = 0.0f;
};

USTRUCT(BlueprintType)
struct DRONEPROTO_API FDroneCoreCalculationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Combat")
	float CoreAttackModifier = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Combat")
	float CoreBonusAttackModifier = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Combat")
	float CoreMoveSpeedModifier = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Combat")
	float HPRatio = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Combat")
	float MoveSpeedBonus = 0.0f;
};

USTRUCT(BlueprintType)
struct DRONEPROTO_API FDroneCombatRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Report")
	float SurvivalTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Report")
	float BossDamage = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Report")
	float BossMaxHP = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Report")
	float BossHPOnJoin = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Report")
	float MoveDistance = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Report")
	float HealAmount = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Report")
	int32 DamageTakenCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Report")
	float RaidJoinTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Report")
	float CombatStartTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Report")
	float CombatEndTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Report")
	bool bIsAliveAtReport = true;
};

USTRUCT(BlueprintType)
struct DRONEPROTO_API FDroneReportData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Report")
	float SurvivalTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Report")
	float BossDamage = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Report")
	float BossDamageRatio = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Report")
	float MoveDistance = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Report")
	float HealAmount = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Report")
	int32 BonusScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Report")
	TArray<EDroneReportBonusType> AchievedBonusList;

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Report")
	EDroneReportGrade Grade = EDroneReportGrade::C;

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Report")
	float ReportScore = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Report")
	float CombatDuration = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Report")
	float BossHPOnJoin = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Report")
	int32 DamageTakenCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Report")
	bool bIsAliveAtReport = false;

	UPROPERTY(BlueprintReadOnly, Category = "Drone|Report")
	bool bIsReportGenerated = false;
};

struct DRONEPROTO_API FDroneCombatRules
{
	static FDroneWeaponCalculationResult CalculateWeaponDamage(const FDroneWeaponCalculationInput& Input)
	{
		FDroneWeaponCalculationResult Result;
		Result.PulseAttackCount = FMath::Max(0, Input.PulseAttackCount);
		Result.VectorDistance = FMath::Max(0.0f, Input.VectorAccumulatedMoveDistanceMeters);

		switch (Input.WeaponType)
		{
		case EDroneCombatWeaponType::PulseLaser:
			Result.BaseDamage = 8.0f;
			Result.HitCount = 1;
			Result.PulseAttackCount++;
			if (Result.PulseAttackCount >= 3)
			{
				Result.WeaponDamage = 18.0f;
				Result.BonusDamage = 10.0f;
				Result.PulseAttackCount = 0;
			}
			else
			{
				Result.WeaponDamage = Result.BaseDamage;
			}
			break;

		case EDroneCombatWeaponType::FractureBurst:
			Result.BaseDamage = 5.0f;
			Result.AdditionalHitCount = 3;
			Result.BonusDamage = static_cast<float>(Result.AdditionalHitCount) * 2.0f;
			Result.HitCount = 4;
			Result.WeaponDamage = Result.BaseDamage + Result.BonusDamage;
			break;

		case EDroneCombatWeaponType::VectorCannon:
			Result.BaseDamage = 7.0f;
			Result.BonusDamage = FMath::Min(FMath::FloorToFloat(Result.VectorDistance / 5.0f), 8.0f);
			Result.HitCount = 1;
			Result.WeaponDamage = Result.BaseDamage + Result.BonusDamage;
			Result.bResetVectorDistance = true;
			break;

		default:
			break;
		}

		return Result;
	}

	static FDroneCoreCalculationResult CalculateCoreBonus(const FDroneCoreCalculationInput& Input)
	{
		FDroneCoreCalculationResult Result;

		switch (Input.CoreType)
		{
		case EDroneCombatCoreType::Zenith:
			Result.CoreAttackModifier = 1.0f;
			Result.CoreMoveSpeedModifier = 1.0f;
			Result.HPRatio = Input.MaxHP > KINDA_SMALL_NUMBER
				? FMath::Clamp(Input.CurrentHP / Input.MaxHP, 0.0f, 1.0f)
				: 0.0f;
			Result.CoreBonusAttackModifier = 1.0f + FMath::Min(FMath::FloorToFloat(Result.HPRatio / 0.1f) * 0.02f, 0.20f);
			break;

		case EDroneCombatCoreType::Booster:
		{
			Result.CoreAttackModifier = 0.95f;
			Result.CoreMoveSpeedModifier = 1.0f;
			const int32 MoveStackCount = FMath::Max(0, FMath::FloorToInt(FMath::Max(0.0f, Input.AccumulatedMoveDistanceMeters) / 20.0f));
			Result.MoveSpeedBonus = FMath::Min(static_cast<float>(MoveStackCount) * 0.03f, 0.30f);
			Result.CoreBonusAttackModifier = 1.0f + (Result.MoveSpeedBonus * 0.5f);
			break;
		}

		case EDroneCombatCoreType::Drain:
			Result.CoreAttackModifier = 0.85f;
			Result.CoreMoveSpeedModifier = 0.9f;
			break;

		default:
			break;
		}

		return Result;
	}

	static float CalculateDrainHeal(float DamageDealt)
	{
		return FMath::Min(FMath::Max(0.0f, DamageDealt) * 0.12f, 3.0f);
	}
};

struct DRONEPROTO_API FDroneReportRules
{
	static float CalculateCombatDuration(const FDroneCombatRecord& Record)
	{
		if (Record.CombatEndTime > Record.CombatStartTime)
		{
			return FMath::Max(0.0f, Record.CombatEndTime - Record.CombatStartTime);
		}

		return FMath::Max(0.0f, Record.SurvivalTime);
	}

	static EDroneReportGrade CalculateGrade(float ReportScore)
	{
		if (ReportScore >= 850.0f)
		{
			return EDroneReportGrade::S;
		}
		if (ReportScore >= 650.0f)
		{
			return EDroneReportGrade::A;
		}
		if (ReportScore >= 400.0f)
		{
			return EDroneReportGrade::B;
		}

		return EDroneReportGrade::C;
	}

	static FDroneReportData BuildReportData(const FDroneCombatRecord& Record, bool bBossDefeated)
	{
		FDroneReportData Report;
		Report.SurvivalTime = FMath::Max(0.0f, Record.SurvivalTime);
		Report.BossDamage = FMath::Max(0.0f, Record.BossDamage);
		Report.MoveDistance = FMath::Max(0.0f, Record.MoveDistance);
		Report.HealAmount = FMath::Max(0.0f, Record.HealAmount);
		Report.CombatDuration = CalculateCombatDuration(Record);
		Report.BossHPOnJoin = FMath::Max(0.0f, Record.BossHPOnJoin);
		Report.DamageTakenCount = FMath::Max(0, Record.DamageTakenCount);
		Report.bIsAliveAtReport = Record.bIsAliveAtReport;
		Report.bIsReportGenerated = true;

		const float BossMaxHP = FMath::Max(0.0f, Record.BossMaxHP);
		Report.BossDamageRatio = BossMaxHP > KINDA_SMALL_NUMBER
			? FMath::Clamp(Report.BossDamage / BossMaxHP, 0.0f, 1.0f)
			: 0.0f;

		const float SurvivalScore = FMath::Min((Report.SurvivalTime / 180.0f) * 200.0f, 200.0f);
		const float BossDamageScore = Report.BossDamage > KINDA_SMALL_NUMBER
			? FMath::Min((Report.BossDamageRatio / 0.08f) * 350.0f, 350.0f)
			: 0.0f;
		const bool bHasMinimumBossContribution = Report.BossDamageRatio >= 0.01f;
		const float MoveScore = bHasMinimumBossContribution
			? FMath::Min((Report.MoveDistance / 600.0f) * 100.0f, 100.0f)
			: 0.0f;
		const float HealScore = bHasMinimumBossContribution
			? FMath::Min((Report.HealAmount / 60.0f) * 100.0f, 100.0f)
			: 0.0f;

		int32 RawBonusScore = 0;
		const auto AddBonus = [&Report, &RawBonusScore](EDroneReportBonusType BonusType, int32 Score)
		{
			if (Score <= 0)
			{
				return;
			}

			Report.AchievedBonusList.Add(BonusType);
			RawBonusScore += Score;
		};

		const float DurationMinutes = Report.CombatDuration > KINDA_SMALL_NUMBER ? Report.CombatDuration / 60.0f : 0.0f;

		if (bBossDefeated && Report.bIsAliveAtReport)
		{
			const bool bLateJoin = BossMaxHP > KINDA_SMALL_NUMBER && Report.BossHPOnJoin <= BossMaxHP * 0.25f;
			if (bLateJoin)
			{
				if (Report.CombatDuration >= 30.0f && Report.BossDamageRatio >= 0.01f)
				{
					AddBonus(EDroneReportBonusType::BossSlayer, 40);
				}
			}
			else if (Report.CombatDuration >= 60.0f && Report.BossDamageRatio >= 0.03f)
			{
				AddBonus(EDroneReportBonusType::BossSlayer, 80);
			}
		}

		if (Report.CombatDuration >= 30.0f && Report.BossDamageRatio >= 0.015f && DurationMinutes > KINDA_SMALL_NUMBER)
		{
			const float DamagePerMinute = Report.BossDamageRatio / DurationMinutes;
			if (DamagePerMinute >= 0.03f)
			{
				AddBonus(EDroneReportBonusType::HighDPS, 70);
			}
			else if (DamagePerMinute >= 0.02f)
			{
				AddBonus(EDroneReportBonusType::HighDPS, 40);
			}
		}

		if (Report.DamageTakenCount == 0 && Report.CombatDuration >= 60.0f && Report.BossDamageRatio >= 0.02f)
		{
			AddBonus(EDroneReportBonusType::NoDamage, 50);
		}

		if (Report.CombatDuration >= 60.0f && Report.MoveDistance >= 500.0f && Report.BossDamageRatio >= 0.02f)
		{
			AddBonus(EDroneReportBonusType::KeepMoving, 50);
		}
		else if (Report.CombatDuration >= 30.0f && Report.BossDamageRatio >= 0.015f && DurationMinutes > KINDA_SMALL_NUMBER)
		{
			const float MovePerMinute = Report.MoveDistance / DurationMinutes;
			if (MovePerMinute >= 150.0f)
			{
				AddBonus(EDroneReportBonusType::KeepMoving, 40);
			}
		}

		if (Report.bIsAliveAtReport && Report.CombatDuration >= 60.0f && Report.BossDamageRatio >= 0.015f)
		{
			if (Report.HealAmount >= 40.0f)
			{
				AddBonus(EDroneReportBonusType::HighRecovery, 50);
			}
			else if (Report.HealAmount >= 25.0f)
			{
				AddBonus(EDroneReportBonusType::HighRecovery, 30);
			}
		}

		Report.BonusScore = FMath::Min(RawBonusScore, 250);
		const float BasePerformanceScore = SurvivalScore + BossDamageScore + MoveScore + HealScore;
		Report.ReportScore = BasePerformanceScore + static_cast<float>(Report.BonusScore);
		Report.Grade = CalculateGrade(Report.ReportScore);
		return Report;
	}
};
