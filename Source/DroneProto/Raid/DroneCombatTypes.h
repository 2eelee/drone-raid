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
	FString Callsign = TEXT("AAA");

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
	TArray<FText> AchievedBonusDisplayNames;

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

struct FDroneReportBonusRule
{
	EDroneReportBonusType Type = EDroneReportBonusType::BossSlayer;
	FText DisplayName;
	int32 PrimaryScore = 0;
	int32 SecondaryScore = 0;
	float PrimaryMinCombatDuration = 0.0f;
	float SecondaryMinCombatDuration = 0.0f;
	float PrimaryMinBossDamageRatio = 0.0f;
	float SecondaryMinBossDamageRatio = 0.0f;
	float PrimaryMinDamagePerMinute = 0.0f;
	float SecondaryMinDamagePerMinute = 0.0f;
	float PrimaryMinMoveDistance = 0.0f;
	float SecondaryMinMovePerMinute = 0.0f;
	float PrimaryMinHealAmount = 0.0f;
	float SecondaryMinHealAmount = 0.0f;
	float LateJoinBossHPThresholdRatio = 0.0f;
	int32 MaxDamageTakenCount = -1;
	int32 MaxScore = 0;
	bool bRequiresBossDefeated = false;
	bool bRequiresAlive = false;
};

struct FDroneReportGradeRule
{
	EDroneReportGrade Grade = EDroneReportGrade::C;
	float MinScore = 0.0f;
	float MaxScore = 0.0f;
};

struct FDroneReportResolvedConfig
{
	TArray<FDroneReportBonusRule> BonusRules;
	TArray<FDroneReportGradeRule> GradeRules;
	int32 BonusScoreCap = 250;

	const FDroneReportBonusRule* FindBonusRule(EDroneReportBonusType BonusType) const
	{
		return BonusRules.FindByPredicate([BonusType](const FDroneReportBonusRule& Rule)
		{
			return Rule.Type == BonusType;
		});
	}
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
	static FDroneReportResolvedConfig MakeCanonicalConfig()
	{
		FDroneReportResolvedConfig Config;
		Config.BonusScoreCap = 250;

		auto AddBonus = [&Config](
			EDroneReportBonusType Type,
			const TCHAR* DisplayName,
			int32 PrimaryScore,
			int32 SecondaryScore,
			float PrimaryDuration,
			float SecondaryDuration,
			float PrimaryDamageRatio,
			float SecondaryDamageRatio,
			int32 MaxScore)
		{
			FDroneReportBonusRule Rule;
			Rule.Type = Type;
			Rule.DisplayName = FText::FromString(DisplayName);
			Rule.PrimaryScore = PrimaryScore;
			Rule.SecondaryScore = SecondaryScore;
			Rule.PrimaryMinCombatDuration = PrimaryDuration;
			Rule.SecondaryMinCombatDuration = SecondaryDuration;
			Rule.PrimaryMinBossDamageRatio = PrimaryDamageRatio;
			Rule.SecondaryMinBossDamageRatio = SecondaryDamageRatio;
			Rule.MaxScore = MaxScore;
			Config.BonusRules.Add(Rule);
			return Config.BonusRules.Num() - 1;
		};

		FDroneReportBonusRule& BossSlayer = Config.BonusRules[
			AddBonus(EDroneReportBonusType::BossSlayer, TEXT("Boss Slayer"), 80, 40, 60.0f, 30.0f, 0.03f, 0.01f, 80)];
		BossSlayer.LateJoinBossHPThresholdRatio = 0.25f;
		BossSlayer.bRequiresBossDefeated = true;
		BossSlayer.bRequiresAlive = true;

		FDroneReportBonusRule& HighDPS = Config.BonusRules[
			AddBonus(EDroneReportBonusType::HighDPS, TEXT("High DPS"), 70, 40, 30.0f, 30.0f, 0.015f, 0.015f, 70)];
		HighDPS.PrimaryMinDamagePerMinute = 0.03f;
		HighDPS.SecondaryMinDamagePerMinute = 0.02f;

		FDroneReportBonusRule& NoDamage = Config.BonusRules[
			AddBonus(EDroneReportBonusType::NoDamage, TEXT("NO DAMAGE"), 50, 0, 60.0f, 0.0f, 0.02f, 0.0f, 50)];
		NoDamage.MaxDamageTakenCount = 0;

		FDroneReportBonusRule& KeepMoving = Config.BonusRules[
			AddBonus(EDroneReportBonusType::KeepMoving, TEXT("Keep Moving"), 50, 40, 60.0f, 30.0f, 0.02f, 0.015f, 50)];
		KeepMoving.PrimaryMinMoveDistance = 500.0f;
		KeepMoving.SecondaryMinMovePerMinute = 150.0f;

		FDroneReportBonusRule& HighRecovery = Config.BonusRules[
			AddBonus(EDroneReportBonusType::HighRecovery, TEXT("High Recovery"), 50, 30, 60.0f, 60.0f, 0.015f, 0.015f, 50)];
		HighRecovery.PrimaryMinHealAmount = 40.0f;
		HighRecovery.SecondaryMinHealAmount = 25.0f;
		HighRecovery.bRequiresAlive = true;

		Config.GradeRules =
		{
			{ EDroneReportGrade::S, 850.0f, 1000.0f },
			{ EDroneReportGrade::A, 650.0f, 849.0f },
			{ EDroneReportGrade::B, 400.0f, 649.0f },
			{ EDroneReportGrade::C, 0.0f, 399.0f }
		};
		return Config;
	}

	static float CalculateCombatDuration(const FDroneCombatRecord& Record)
	{
		if (Record.CombatEndTime > Record.CombatStartTime)
		{
			return FMath::Max(0.0f, Record.CombatEndTime - Record.CombatStartTime);
		}

		return FMath::Max(0.0f, Record.SurvivalTime);
	}

	static bool IsCompleteConfig(const FDroneReportResolvedConfig& Config)
	{
		return Config.BonusRules.Num() == 5
			&& Config.GradeRules.Num() == 4
			&& Config.FindBonusRule(EDroneReportBonusType::BossSlayer)
			&& Config.FindBonusRule(EDroneReportBonusType::HighDPS)
			&& Config.FindBonusRule(EDroneReportBonusType::NoDamage)
			&& Config.FindBonusRule(EDroneReportBonusType::KeepMoving)
			&& Config.FindBonusRule(EDroneReportBonusType::HighRecovery);
	}

	static EDroneReportGrade CalculateGrade(float ReportScore, const FDroneReportResolvedConfig& Config)
	{
		for (const FDroneReportGradeRule& Rule : Config.GradeRules)
		{
			if (ReportScore >= Rule.MinScore)
			{
				return Rule.Grade;
			}
		}
		return EDroneReportGrade::C;
	}

	static EDroneReportGrade CalculateGrade(float ReportScore)
	{
		return CalculateGrade(ReportScore, MakeCanonicalConfig());
	}

	static FDroneReportData BuildReportData(const FDroneCombatRecord& Record, bool bBossDefeated)
	{
		return BuildReportData(Record, bBossDefeated, MakeCanonicalConfig());
	}

	static FDroneReportData BuildReportData(
		const FDroneCombatRecord& Record,
		bool bBossDefeated,
		const FDroneReportResolvedConfig& Config)
	{
		FDroneReportResolvedConfig FallbackConfig;
		const FDroneReportResolvedConfig* ActiveConfig = &Config;
		if (!IsCompleteConfig(Config))
		{
			FallbackConfig = MakeCanonicalConfig();
			ActiveConfig = &FallbackConfig;
		}

		const FDroneReportBonusRule* BossSlayerRule = ActiveConfig->FindBonusRule(EDroneReportBonusType::BossSlayer);
		const FDroneReportBonusRule* HighDPSRule = ActiveConfig->FindBonusRule(EDroneReportBonusType::HighDPS);
		const FDroneReportBonusRule* NoDamageRule = ActiveConfig->FindBonusRule(EDroneReportBonusType::NoDamage);
		const FDroneReportBonusRule* KeepMovingRule = ActiveConfig->FindBonusRule(EDroneReportBonusType::KeepMoving);
		const FDroneReportBonusRule* HighRecoveryRule = ActiveConfig->FindBonusRule(EDroneReportBonusType::HighRecovery);

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
		const auto AddBonus = [&Report, &RawBonusScore](const FDroneReportBonusRule& Rule, int32 Score)
		{
			const int32 AppliedScore = FMath::Min(Score, Rule.MaxScore);
			if (AppliedScore <= 0)
			{
				return;
			}

			Report.AchievedBonusList.Add(Rule.Type);
			Report.AchievedBonusDisplayNames.Add(Rule.DisplayName);
			RawBonusScore += AppliedScore;
		};

		const float DurationMinutes = Report.CombatDuration > KINDA_SMALL_NUMBER ? Report.CombatDuration / 60.0f : 0.0f;
		const auto MeetsCommon = [&Report, bBossDefeated](
			const FDroneReportBonusRule& Rule,
			float MinDuration,
			float MinDamageRatio)
		{
			return (!Rule.bRequiresBossDefeated || bBossDefeated)
				&& (!Rule.bRequiresAlive || Report.bIsAliveAtReport)
				&& Report.CombatDuration >= MinDuration
				&& Report.BossDamageRatio >= MinDamageRatio;
		};

		if (BossSlayerRule)
		{
			const bool bLateJoin = BossSlayerRule->LateJoinBossHPThresholdRatio > 0.0f
				&& BossMaxHP > KINDA_SMALL_NUMBER
				&& Report.BossHPOnJoin <= BossMaxHP * BossSlayerRule->LateJoinBossHPThresholdRatio;
			if (bLateJoin)
			{
				if (MeetsCommon(*BossSlayerRule, BossSlayerRule->SecondaryMinCombatDuration, BossSlayerRule->SecondaryMinBossDamageRatio))
				{
					AddBonus(*BossSlayerRule, BossSlayerRule->SecondaryScore);
				}
			}
			else if (MeetsCommon(*BossSlayerRule, BossSlayerRule->PrimaryMinCombatDuration, BossSlayerRule->PrimaryMinBossDamageRatio))
			{
				AddBonus(*BossSlayerRule, BossSlayerRule->PrimaryScore);
			}
		}

		if (HighDPSRule && DurationMinutes > KINDA_SMALL_NUMBER)
		{
			const float DamagePerMinute = Report.BossDamageRatio / DurationMinutes;
			if (MeetsCommon(*HighDPSRule, HighDPSRule->PrimaryMinCombatDuration, HighDPSRule->PrimaryMinBossDamageRatio)
				&& DamagePerMinute >= HighDPSRule->PrimaryMinDamagePerMinute)
			{
				AddBonus(*HighDPSRule, HighDPSRule->PrimaryScore);
			}
			else if (MeetsCommon(*HighDPSRule, HighDPSRule->SecondaryMinCombatDuration, HighDPSRule->SecondaryMinBossDamageRatio)
				&& DamagePerMinute >= HighDPSRule->SecondaryMinDamagePerMinute)
			{
				AddBonus(*HighDPSRule, HighDPSRule->SecondaryScore);
			}
		}

		if (NoDamageRule
			&& NoDamageRule->MaxDamageTakenCount >= 0
			&& Report.DamageTakenCount <= NoDamageRule->MaxDamageTakenCount
			&& MeetsCommon(*NoDamageRule, NoDamageRule->PrimaryMinCombatDuration, NoDamageRule->PrimaryMinBossDamageRatio))
		{
			AddBonus(*NoDamageRule, NoDamageRule->PrimaryScore);
		}

		if (KeepMovingRule
			&& MeetsCommon(*KeepMovingRule, KeepMovingRule->PrimaryMinCombatDuration, KeepMovingRule->PrimaryMinBossDamageRatio)
			&& Report.MoveDistance >= KeepMovingRule->PrimaryMinMoveDistance)
		{
			AddBonus(*KeepMovingRule, KeepMovingRule->PrimaryScore);
		}
		else if (KeepMovingRule
			&& MeetsCommon(*KeepMovingRule, KeepMovingRule->SecondaryMinCombatDuration, KeepMovingRule->SecondaryMinBossDamageRatio)
			&& DurationMinutes > KINDA_SMALL_NUMBER)
		{
			const float MovePerMinute = Report.MoveDistance / DurationMinutes;
			if (MovePerMinute >= KeepMovingRule->SecondaryMinMovePerMinute)
			{
				AddBonus(*KeepMovingRule, KeepMovingRule->SecondaryScore);
			}
		}

		if (HighRecoveryRule)
		{
			if (MeetsCommon(*HighRecoveryRule, HighRecoveryRule->PrimaryMinCombatDuration, HighRecoveryRule->PrimaryMinBossDamageRatio)
				&& Report.HealAmount >= HighRecoveryRule->PrimaryMinHealAmount)
			{
				AddBonus(*HighRecoveryRule, HighRecoveryRule->PrimaryScore);
			}
			else if (MeetsCommon(*HighRecoveryRule, HighRecoveryRule->SecondaryMinCombatDuration, HighRecoveryRule->SecondaryMinBossDamageRatio)
				&& Report.HealAmount >= HighRecoveryRule->SecondaryMinHealAmount)
			{
				AddBonus(*HighRecoveryRule, HighRecoveryRule->SecondaryScore);
			}
		}

		Report.BonusScore = FMath::Min(RawBonusScore, ActiveConfig->BonusScoreCap);
		const float BasePerformanceScore = SurvivalScore + BossDamageScore + MoveScore + HealScore;
		Report.ReportScore = BasePerformanceScore + static_cast<float>(Report.BonusScore);
		Report.Grade = CalculateGrade(Report.ReportScore, *ActiveConfig);
		return Report;
	}
};
