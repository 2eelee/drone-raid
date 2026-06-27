#include "DroneReportWidget.h"

#include "Components/TextBlock.h"

void UDroneReportWidget::RefreshReport(const FDroneReportData& InReportData)
{
	CachedSurvivalTimeText = FText::FromString(FString::Printf(TEXT("%.1f s"), InReportData.SurvivalTime));
	CachedBossDamageText = FText::FromString(FString::Printf(TEXT("%.1f"), InReportData.BossDamage));
	CachedBossDamageRatioText = FText::FromString(FString::Printf(TEXT("%.1f%%"), InReportData.BossDamageRatio * 100.0f));
	CachedMoveDistanceText = FText::FromString(FString::Printf(TEXT("%.1f m"), InReportData.MoveDistance));
	CachedHealAmountText = FText::FromString(FString::Printf(TEXT("%.1f"), InReportData.HealAmount));
	CachedBonusScoreText = FText::FromString(FString::Printf(TEXT("%d"), InReportData.BonusScore));
	CachedAchievedBonusText = BuildAchievedBonusText(InReportData.AchievedBonusList);
	CachedGradeText = GetGradeDisplayText(InReportData.Grade);

	SetOptionalText(SurvivalTimeText, CachedSurvivalTimeText);
	SetOptionalText(BossDamageText, CachedBossDamageText);
	SetOptionalText(BossDamageRatioText, CachedBossDamageRatioText);
	SetOptionalText(MoveDistanceText, CachedMoveDistanceText);
	SetOptionalText(HealAmountText, CachedHealAmountText);
	SetOptionalText(BonusScoreText, CachedBonusScoreText);
	SetOptionalText(AchievedBonusText, CachedAchievedBonusText);
	SetOptionalText(GradeText, CachedGradeText);
	SetOptionalText(ResultTitleText, FText::FromString(TEXT("Drone Report")));
	SetOptionalText(ReportTitleText, FText::FromString(TEXT("Drone Report")));
}

FText UDroneReportWidget::GetSurvivalTimeText() const
{
	return CachedSurvivalTimeText;
}

FText UDroneReportWidget::GetBossDamageText() const
{
	return CachedBossDamageText;
}

FText UDroneReportWidget::GetBossDamageRatioText() const
{
	return CachedBossDamageRatioText;
}

FText UDroneReportWidget::GetMoveDistanceText() const
{
	return CachedMoveDistanceText;
}

FText UDroneReportWidget::GetHealAmountText() const
{
	return CachedHealAmountText;
}

FText UDroneReportWidget::GetBonusScoreText() const
{
	return CachedBonusScoreText;
}

FText UDroneReportWidget::GetAchievedBonusText() const
{
	return CachedAchievedBonusText;
}

FText UDroneReportWidget::GetGradeText() const
{
	return CachedGradeText;
}

FText UDroneReportWidget::GetBonusTypeDisplayText(EDroneReportBonusType BonusType)
{
	switch (BonusType)
	{
	case EDroneReportBonusType::BossSlayer:
		return FText::FromString(TEXT("Boss Slayer"));
	case EDroneReportBonusType::HighDPS:
		return FText::FromString(TEXT("High DPS"));
	case EDroneReportBonusType::NoDamage:
		return FText::FromString(TEXT("NO DAMAGE"));
	case EDroneReportBonusType::KeepMoving:
		return FText::FromString(TEXT("Keep Moving"));
	case EDroneReportBonusType::HighRecovery:
		return FText::FromString(TEXT("High Recovery"));
	default:
		return FText::GetEmpty();
	}
}

FText UDroneReportWidget::GetGradeDisplayText(EDroneReportGrade Grade)
{
	switch (Grade)
	{
	case EDroneReportGrade::S:
		return FText::FromString(TEXT("S"));
	case EDroneReportGrade::A:
		return FText::FromString(TEXT("A"));
	case EDroneReportGrade::B:
		return FText::FromString(TEXT("B"));
	case EDroneReportGrade::C:
		return FText::FromString(TEXT("C"));
	default:
		return FText::GetEmpty();
	}
}

FText UDroneReportWidget::BuildAchievedBonusText(const TArray<EDroneReportBonusType>& AchievedBonusList)
{
	if (AchievedBonusList.IsEmpty())
	{
		return FText::FromString(TEXT("None"));
	}

	TArray<FString> BonusNames;
	BonusNames.Reserve(AchievedBonusList.Num());
	for (const EDroneReportBonusType BonusType : AchievedBonusList)
	{
		BonusNames.Add(GetBonusTypeDisplayText(BonusType).ToString());
	}

	return FText::FromString(FString::Join(BonusNames, TEXT(", ")));
}

void UDroneReportWidget::SetOptionalText(UTextBlock* TextBlock, const FText& Text)
{
	if (TextBlock)
	{
		TextBlock->SetText(Text);
	}
}
