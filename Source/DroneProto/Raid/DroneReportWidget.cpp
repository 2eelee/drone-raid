#include "DroneReportWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

namespace
{
constexpr const TCHAR* ReturnToLobbyMapName = TEXT("LobbyMap");
}

void UDroneReportWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ReturnToLobbyButton)
	{
		ReturnToLobbyButton->OnClicked.AddUniqueDynamic(this, &UDroneReportWidget::HandleReturnToLobbyClicked);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ReportReturnToLobbyButtonMissing Widget=%s"),
			*GetName());
	}
}

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

void UDroneReportWidget::RequestReturnToLobby()
{
	if (bReturnToLobbyRequested)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ReportReturnToLobbyIgnored Reason=AlreadyRequested Widget=%s"),
			*GetName());
		return;
	}

	bReturnToLobbyRequested = true;

	if (ReturnToLobbyButton)
	{
		ReturnToLobbyButton->SetIsEnabled(false);
	}

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ReportReturnToLobbyClicked Widget=%s"),
		*GetName());

#if WITH_DEV_AUTOMATION_TESTS
	if (bSuppressReturnToLobbyTravelForTest)
	{
		++ReturnToLobbyTravelRequestCountForTest;
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ReportReturnToLobbyTravel Target=LobbyMap Widget=%s Mode=SuppressedForAutomation"),
			*GetName());
		return;
	}
#endif

	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ReportReturnToLobbyIgnored Reason=InvalidWorldOrDedicatedServer Widget=%s"),
			*GetName());
		return;
	}

	APlayerController* OwningPC = GetOwningPlayer();
	if (!OwningPC || !OwningPC->IsLocalController())
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ReportReturnToLobbyIgnored Reason=NotOwningLocalController Widget=%s PC=%s"),
			*GetName(),
			*GetNameSafe(OwningPC));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ReportReturnToLobbyTravel Target=LobbyMap Widget=%s Method=OpenLevel"),
		*GetName());

	UGameplayStatics::OpenLevel(World, FName(ReturnToLobbyMapName));
}

void UDroneReportWidget::HandleReturnToLobbyClicked()
{
	RequestReturnToLobby();
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
		return FText::GetEmpty();
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
