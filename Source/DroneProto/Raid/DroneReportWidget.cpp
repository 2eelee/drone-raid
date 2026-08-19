#include "DroneReportWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "RaidPlayerController.h"

namespace
{
constexpr const TCHAR* ReturnToLobbyMapName = TEXT("LobbyMap");
}

void UDroneReportWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	BindReturnToLobbyButton();
}

void UDroneReportWidget::BindReturnToLobbyButton()
{
	if (!ReturnToLobbyButton)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ReportReturnToLobbyButtonMissing Widget=%s"),
			*GetName());
		return;
	}

	// 확인 버튼에서 나가는 길을 하나로 좁힌다.
	//
	// WBP_DroneReport에는 같은 버튼에 묶인 Blueprint 이벤트
	// (BndEvt__WBP_DroneReport_ConfirmButton_...OnButtonClickedEvent)가 남아 있고, 그 이벤트가
	// GameplayStatics::OpenLevel(LobbyMap)을 직접 부른다. 그 경로는 RequestReturnToLobby를 거치지
	// 않으므로 소유 컨트롤러에게 아무것도 묻지 않는다. Blueprint 바인딩은 UUserWidget::Initialize
	// 단계에서 등록되어 NativeConstruct보다 먼저 자리를 잡으므로, 샌드박스 override가 true를 돌려
	// C++ travel을 막아도 Blueprint가 예약한 이동이 그대로 이어져 LobbyMap으로 나갔다.
	// 자동화가 GREEN인데 PIE만 다르게 동작한 원인이 이 두 번째 경로다 —
	// 테스트는 C++ 홉만 직접 부르기 때문에 Blueprint 경로가 애초에 없다.
	//
	// 이 위젯 자신에게 걸린 클릭 경로를 전부 걷어내고 C++ 핸들러만 다시 건다. 확인은
	// RequestReturnToLobby 한 곳으로 좁혀지고, 이동 여부는 컨트롤러 계약이 단독으로 정한다.
	// 프로덕션 동작은 그대로다 — HandleReturnToLobbyClicked도 결국 같은 LobbyMap 이동을 한다.
	const bool bNativeHandlerAlreadyBound =
		ReturnToLobbyButton->OnClicked.IsAlreadyBound(this, &UDroneReportWidget::HandleReturnToLobbyClicked);
	const int32 BindingCountBefore = ReturnToLobbyButton->OnClicked.GetAllObjects().Num();
	ReclaimedConfirmBindingCount = FMath::Max(0, BindingCountBefore - (bNativeHandlerAlreadyBound ? 1 : 0));

	ReturnToLobbyButton->OnClicked.RemoveAll(this);
	ReturnToLobbyButton->OnClicked.AddUniqueDynamic(this, &UDroneReportWidget::HandleReturnToLobbyClicked);

	if (ReclaimedConfirmBindingCount > 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[DR_SUMMARY] ReportConfirmBindingReclaimed Widget=%s Removed=%d — 확인 버튼에 걸려 있던 Blueprint 클릭 경로를 걷어냈다. 확인은 RequestReturnToLobby 하나만 탄다"),
			*GetName(),
			ReclaimedConfirmBindingCount);
	}
}

FReply UDroneReportWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Z)
	{
		RequestReturnToLobby();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UDroneReportWidget::RefreshReport(const FDroneReportData& InReportData)
{
	CachedCallsignText = FText::FromString(InReportData.Callsign.IsEmpty() ? TEXT("AAA") : InReportData.Callsign);
	CachedSurvivalTimeText = FText::FromString(FString::Printf(TEXT("%.1f s"), InReportData.SurvivalTime));
	CachedBossDamageText = FText::FromString(FString::Printf(TEXT("%.1f"), InReportData.BossDamage));
	CachedBossDamageRatioText = FText::FromString(FString::Printf(TEXT("%.1f%%"), InReportData.BossDamageRatio * 100.0f));
	CachedMoveDistanceText = FText::FromString(FString::Printf(TEXT("%.1f m"), InReportData.MoveDistance));
	CachedHealAmountText = FText::FromString(FString::Printf(TEXT("%.1f"), InReportData.HealAmount));
	CachedBonusScoreText = FText::FromString(FString::Printf(TEXT("%d"), InReportData.BonusScore));
	CachedAchievedBonusText = BuildAchievedBonusText(
		InReportData.AchievedBonusList,
		InReportData.AchievedBonusDisplayNames);
	CachedGradeText = GetGradeDisplayText(InReportData.Grade);

	SetOptionalText(CallsignText, CachedCallsignText);
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

	// 확인 처리를 소유 컨트롤러에 먼저 맡긴다. 기본 구현은 false를 돌리므로 아래 LobbyMap 이동이
	// 그대로 돈다 — 프로덕션 계약은 바뀌지 않는다. 밸런스 샌드박스만 true를 돌려 이동을 억제한다.
	if (ARaidPlayerController* RaidPC = Cast<ARaidPlayerController>(GetOwningPlayer()))
	{
		if (RaidPC->TryHandleDroneReportConfirmedForLocalPlayer(this))
		{
			return;
		}
	}

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

void UDroneReportWidget::DismissReport()
{
	// 같은 인스턴스가 다음 리포트에 재사용되므로 확인 상태를 되돌려 둔다.
	bReturnToLobbyRequested = false;
	if (ReturnToLobbyButton)
	{
		ReturnToLobbyButton->SetIsEnabled(true);
	}

	if (IsInViewport())
	{
		RemoveFromParent();
	}
}

void UDroneReportWidget::HandleReturnToLobbyClicked()
{
	RequestReturnToLobby();
}

#if WITH_DEV_AUTOMATION_TESTS
void UDroneReportWidget::BindReturnToLobbyButtonForTest(UButton* InButton)
{
	ReturnToLobbyButton = InButton;
	BindReturnToLobbyButton();
}

int32 UDroneReportWidget::GetConfirmButtonBindingCountForTest() const
{
	return ReturnToLobbyButton ? ReturnToLobbyButton->OnClicked.GetAllObjects().Num() : 0;
}

bool UDroneReportWidget::IsConfirmButtonOwnedByNativeHandlerForTest()
{
	return ReturnToLobbyButton
		&& ReturnToLobbyButton->OnClicked.IsAlreadyBound(this, &UDroneReportWidget::HandleReturnToLobbyClicked);
}
#endif

FText UDroneReportWidget::GetCallsignText() const
{
	return CachedCallsignText;
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

FText UDroneReportWidget::BuildAchievedBonusText(
	const TArray<EDroneReportBonusType>& AchievedBonusList,
	const TArray<FText>& AchievedBonusDisplayNames)
{
	if (AchievedBonusList.IsEmpty())
	{
		return FText::GetEmpty();
	}

	if (AchievedBonusDisplayNames.Num() != AchievedBonusList.Num())
	{
		return BuildAchievedBonusText(AchievedBonusList);
	}

	TArray<FString> BonusNames;
	BonusNames.Reserve(AchievedBonusDisplayNames.Num());
	for (const FText& DisplayName : AchievedBonusDisplayNames)
	{
		BonusNames.Add(DisplayName.ToString());
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
