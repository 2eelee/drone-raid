#include "DronePartSelectWidget.h"

#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "InputCoreTypes.h"
#include "RaidGameState.h"
#include "RaidPlayerController.h"
#include "Engine/World.h"
#include "UObject/FieldIterator.h"
#include "UObject/UnrealType.h"

FText UDronePartSelectWidget::GetControlGuideText()
{
	// 기획 원문 `현현_드론부품선택시스템_기획서.md:359-371` `(5) 조작 방식 팝업`의 표시 정보를 그대로 옮긴다.
	// 종전 한 줄 문구(`↑/↓ 항목 이동 …`)는 원문의 `슬롯`·`드론부품` 용어와 콜론 표기를 잃어버려 교체했다.
	return FText::FromString(
		TEXT("조작 방식\n")
		TEXT("↑↓: 슬롯 이동\n")
		TEXT("←→: 드론부품 변경\n")
		TEXT("Z: 선택\n")
		TEXT("C: 취소"));
}

FText UDronePartSelectWidget::FormatPartCountText(int32 CurrentCount)
{
	// 목업 `08`의 확정 계약은 `슬롯 하단 중앙에 회색 원형(남은 수량 표시 위치)`이고 그 안에는 숫자만 들어간다.
	// 목업에서 `남은 수량` 텍스트가 좌측 무기 슬롯 아래 한 곳에만 있는 것은 원형이 무엇인지 설명한 주석이며,
	// 세 슬롯 전부에 붙는 라벨이 아니다. 원문 `:334,342,350`의 `남은 수량 UI`도 요소 이름이지 출력 문자열이 아니고,
	// `:133-149`의 `레이저포 남은 수량 2 → 남은 수량 1`은 재고 변화를 설명하는 시퀀스 서술이다.
	// 따라서 출력은 수량 숫자 하나다. 종전 `남은 수량: %d / %d`는 레이블과 총량 모두 근거가 없었다.
	// `FText::AsNumber`는 로케일에 따라 천단위 구분자를 넣는다. 좁은 원형 안 표시라 서식 없이 숫자만 찍는다.
	return FText::FromString(FString::Printf(TEXT("%d"), CurrentCount));
}

FText UDronePartSelectWidget::FormatSelectionTimerText(float RemainingSeconds)
{
	const int32 RemainingCentiseconds = FMath::CeilToInt(FMath::Max(0.0f, RemainingSeconds) * 100.0f);
	return FText::FromString(FString::Printf(
		TEXT("%02d:%02d"),
		RemainingCentiseconds / 100,
		RemainingCentiseconds % 100));
}

void UDronePartSelectWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);
	ApplyPlanningLayout();
	LogOptionalWidgetBindings();
	CachedRaidPlayerController = GetOwningRaidPlayerController();
	if (CachedRaidPlayerController)
	{
		InitializeCandidatesFromController();
		SyncPreviewIndicesToSelection();

		CachedRaidPlayerController->OnPartSelectUIRefreshRequested.RemoveDynamic(
			this,
			&UDronePartSelectWidget::HandlePartSelectUIRefreshRequested);
		CachedRaidPlayerController->OnPartSelectUIRefreshRequested.AddDynamic(
			this,
			&UDronePartSelectWidget::HandlePartSelectUIRefreshRequested);
		CachedRaidPlayerController->OnPartSelectionResult.RemoveDynamic(
			this,
			&UDronePartSelectWidget::HandlePartSelectionResult);
		CachedRaidPlayerController->OnPartSelectionResult.AddDynamic(
			this,
			&UDronePartSelectWidget::HandlePartSelectionResult);
		CachedRaidPlayerController->OnPartSelectionServerError.RemoveDynamic(
			this,
			&UDronePartSelectWidget::ShowPartSelectionServerError);
		CachedRaidPlayerController->OnPartSelectionServerError.AddDynamic(
			this,
			&UDronePartSelectWidget::ShowPartSelectionServerError);
		CachedRaidPlayerController->RefreshDronePartInventoryBinding();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Client] DronePartSelectWidget has no ARaidPlayerController owning player"));
	}

	BindButtonEvents();
	RefreshFromController();
	StartTimerTextRefresh();
	SetKeyboardFocus();
}

void UDronePartSelectWidget::NativeDestruct()
{
	HidePartSelectionServerError();
	StopTimerTextRefresh();
	UnbindButtonEvents();
	ClearRefreshRetry();

	if (CachedRaidPlayerController)
	{
		CachedRaidPlayerController->OnPartSelectUIRefreshRequested.RemoveDynamic(
			this,
			&UDronePartSelectWidget::HandlePartSelectUIRefreshRequested);
		CachedRaidPlayerController->OnPartSelectionResult.RemoveDynamic(
			this,
			&UDronePartSelectWidget::HandlePartSelectionResult);
		CachedRaidPlayerController->OnPartSelectionServerError.RemoveDynamic(
			this,
			&UDronePartSelectWidget::ShowPartSelectionServerError);
	}

	CachedRaidPlayerController = nullptr;
	Super::NativeDestruct();
}

FReply UDronePartSelectWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();

	if (Key == EKeys::Up)
	{
		FocusPrevSlot();
		return FReply::Handled();
	}

	if (Key == EKeys::Down)
	{
		FocusNextSlot();
		return FReply::Handled();
	}

	if (Key == EKeys::Left)
	{
		if (!bCombatStartFocused)
		{
			MovePreview(FocusedSlot, -1);
		}
		return FReply::Handled();
	}

	if (Key == EKeys::Right)
	{
		if (!bCombatStartFocused)
		{
			MovePreview(FocusedSlot, 1);
		}
		return FReply::Handled();
	}

	if (Key == EKeys::Z)
	{
		ActivateFocusedControl();
		return FReply::Handled();
	}

	if (Key == EKeys::C)
	{
		if (!bCombatStartFocused)
		{
			CancelFocusedPart();
		}
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

ARaidPlayerController* UDronePartSelectWidget::GetOwningRaidPlayerController() const
{
	return Cast<ARaidPlayerController>(GetOwningPlayer());
}

void UDronePartSelectWidget::RefreshFromController()
{
	if (!CachedRaidPlayerController)
	{
		CachedRaidPlayerController = GetOwningRaidPlayerController();
	}

	if (!CachedRaidPlayerController)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Client] RefreshFromController skipped: Owning ARaidPlayerController is missing"));
		ScheduleRefreshRetry(TEXT("Owning ARaidPlayerController is missing"));
		return;
	}

	if (CorePartIDs.Num() == 0 && WeaponPartIDs.Num() == 0)
	{
		InitializeCandidatesFromController();
	}

	FString NotReadyReason;
	if (!IsInventoryDataReadyForRefresh(NotReadyReason))
	{
		SetLoadingText(NotReadyReason);
		ScheduleRefreshRetry(NotReadyReason);
		return;
	}

	ClearRefreshRetry();

	if (Text_ControlGuide)
	{
		Text_ControlGuide->SetText(GetControlGuideText());
	}

	UE_LOG(LogTemp, VeryVerbose, TEXT("[Client] DronePartSelectWidget RefreshFromController: Player=%s FocusedSlot=%s Core=%s Right=%s Left=%s"),
		*CachedRaidPlayerController->GetName(),
		bCombatStartFocused ? TEXT("CombatStart") : *GetSlotDisplayText(FocusedSlot).ToString(),
		*GetPreviewPartIDForSlot(EDronePartSlot::Core).ToString(),
		*GetPreviewPartIDForSlot(EDronePartSlot::RightWeapon).ToString(),
		*GetPreviewPartIDForSlot(EDronePartSlot::LeftWeapon).ToString());

	if (Text_FocusedSlot)
	{
		Text_FocusedSlot->SetText(FText::Format(
			FText::FromString(TEXT("현재 선택: {0}")),
			bCombatStartFocused ? FText::FromString(TEXT("전투 참가")) : GetSlotDisplayText(FocusedSlot)));
	}

	if (Button_CombatStart)
	{
		Button_CombatStart->SetBackgroundColor(
			bCombatStartFocused
				? FLinearColor(0.18f, 0.72f, 0.64f, 1.0f)
				: FLinearColor(0.78f, 0.28f, 0.58f, 1.0f));
	}

	if (TimerText)
	{
		RefreshTimerText();
	}

	if (CoreSelectedText)
	{
		CoreSelectedText->SetText(FText::FromString(FString::Printf(TEXT("Core: %s"), *GetSelectedCorePartID().ToString())));
	}

	if (LeftWeaponSelectedText)
	{
		LeftWeaponSelectedText->SetText(FText::FromString(FString::Printf(TEXT("Left: %s"), *GetSelectedLeftWeaponPartID().ToString())));
	}

	if (RightWeaponSelectedText)
	{
		RightWeaponSelectedText->SetText(FText::FromString(FString::Printf(TEXT("Right: %s"), *GetSelectedRightWeaponPartID().ToString())));
	}

	if (ResultText)
	{
		ResultText->SetText(FText::FromString(FString::Printf(TEXT("State: %s"), IsSelectionLocked() ? TEXT("Locked") : TEXT("Selecting"))));
	}

	RefreshSlot(
		EDronePartSlot::Core,
		GetPreviewPartIDForSlot(EDronePartSlot::Core),
		Text_CoreDescription,
		Text_CoreCount,
		Image_Core);

	RefreshSlot(
		EDronePartSlot::RightWeapon,
		GetPreviewPartIDForSlot(EDronePartSlot::RightWeapon),
		Text_RightWeaponDescription,
		Text_RightWeaponCount,
		Image_RightWeapon);

	RefreshSlot(
		EDronePartSlot::LeftWeapon,
		GetPreviewPartIDForSlot(EDronePartSlot::LeftWeapon),
		Text_LeftWeaponDescription,
		Text_LeftWeaponCount,
		Image_LeftWeapon);

	OnPartStockChangedForUI();
}

void UDronePartSelectWidget::MovePreview(EDronePartSlot PartSlot, int32 Delta)
{
	const TArray<FName>& PartIDs = GetPartIDsForSlot(PartSlot);
	if (PartIDs.Num() <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Client] MovePreview skipped: no part candidates for slot %d"), static_cast<int32>(PartSlot));
		return;
	}

	SetFocusedSlot(PartSlot);

	int32& PreviewIndex = GetMutablePreviewIndexForSlot(PartSlot);
	PreviewIndex = (PreviewIndex + Delta) % PartIDs.Num();
	if (PreviewIndex < 0)
	{
		PreviewIndex += PartIDs.Num();
	}

	RefreshFromController();
}

void UDronePartSelectWidget::FocusNextSlot()
{
	if (bCombatStartFocused)
	{
		SetFocusedSlot(EDronePartSlot::Core);
		return;
	}

	switch (FocusedSlot)
	{
	case EDronePartSlot::Core:
		SetFocusedSlot(EDronePartSlot::RightWeapon);
		break;
	case EDronePartSlot::RightWeapon:
		SetFocusedSlot(EDronePartSlot::LeftWeapon);
		break;
	case EDronePartSlot::LeftWeapon:
	default:
		bCombatStartFocused = true;
		RefreshFromController();
		SetKeyboardFocus();
		break;
	}
}

void UDronePartSelectWidget::FocusPrevSlot()
{
	if (bCombatStartFocused)
	{
		SetFocusedSlot(EDronePartSlot::LeftWeapon);
		return;
	}

	switch (FocusedSlot)
	{
	case EDronePartSlot::Core:
		bCombatStartFocused = true;
		RefreshFromController();
		SetKeyboardFocus();
		break;
	case EDronePartSlot::RightWeapon:
		SetFocusedSlot(EDronePartSlot::Core);
		break;
	case EDronePartSlot::LeftWeapon:
	default:
		SetFocusedSlot(EDronePartSlot::RightWeapon);
		break;
	}
}

bool UDronePartSelectWidget::IsCombatStartFocused() const
{
	return bCombatStartFocused;
}

void UDronePartSelectWidget::ActivateFocusedControl()
{
	if (bCombatStartFocused)
	{
		HandleCombatStartClicked();
		return;
	}

	SelectFocusedPart();
}

FName UDronePartSelectWidget::GetPreviewPartIDForSlot(EDronePartSlot PartSlot) const
{
	const TArray<FName>& PartIDs = GetPartIDsForSlot(PartSlot);
	if (PartIDs.Num() <= 0)
	{
		return NAME_None;
	}

	return PartIDs[FMath::Clamp(GetPreviewIndexForSlot(PartSlot), 0, PartIDs.Num() - 1)];
}

void UDronePartSelectWidget::SelectFocusedPart()
{
	HidePartSelectionServerError();

	if (!CachedRaidPlayerController)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Client] SelectFocusedPart skipped: Owning ARaidPlayerController is missing"));
		return;
	}

	const FName PartID = GetPreviewPartIDForSlot(FocusedSlot);
	if (PartID.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Client] SelectFocusedPart skipped: preview part is None"));
		return;
	}

	CachedRaidPlayerController->RequestSelectPartFromUI(FocusedSlot, PartID);
}

void UDronePartSelectWidget::CancelFocusedPart()
{
	HidePartSelectionServerError();

	if (!CachedRaidPlayerController)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Client] CancelFocusedPart skipped: Owning ARaidPlayerController is missing"));
		return;
	}

	// C 키 취소는 서버 왕복 결과를 기다리지 않는다. 입력을 받은 순간이 곧 취소 피드백이다.
	CachedRaidPlayerController->PlayUICancelSound();
	CachedRaidPlayerController->RequestCancelPartFromUI(FocusedSlot);
}

void UDronePartSelectWidget::ShowPartSelectionServerError()
{
	static const FText ServerErrorMessage = FText::FromString(
		TEXT("일시적인 오류가 발생했습니다. 다시 시도해주세요."));

	if (!ServerErrorPopupPanel)
	{
		ApplyPlanningLayout();
	}

	if (ServerErrorPopupText)
	{
		ServerErrorPopupText->SetText(ServerErrorMessage);
	}
	if (ServerErrorPopupPanel)
	{
		ServerErrorPopupPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UDronePartSelectWidget::HidePartSelectionServerError()
{
	if (ServerErrorPopupPanel)
	{
		ServerErrorPopupPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
}

float UDronePartSelectWidget::GetSelectionRemainingTime() const
{
	return CachedRaidPlayerController ? CachedRaidPlayerController->GetSelectionRemainingTime() : 0.0f;
}

EPlayerSelectionState UDronePartSelectWidget::GetCurrentSelectionState() const
{
	return CachedRaidPlayerController ? CachedRaidPlayerController->GetCurrentSelectionState() : EPlayerSelectionState::Selecting;
}

FName UDronePartSelectWidget::GetSelectedCorePartID() const
{
	return CachedRaidPlayerController ? CachedRaidPlayerController->GetSelectedCorePartID() : NAME_None;
}

FName UDronePartSelectWidget::GetSelectedLeftWeaponPartID() const
{
	return CachedRaidPlayerController ? CachedRaidPlayerController->GetSelectedLeftWeaponPartID() : NAME_None;
}

FName UDronePartSelectWidget::GetSelectedRightWeaponPartID() const
{
	return CachedRaidPlayerController ? CachedRaidPlayerController->GetSelectedRightWeaponPartID() : NAME_None;
}

bool UDronePartSelectWidget::IsSelectionLocked() const
{
	return CachedRaidPlayerController ? CachedRaidPlayerController->IsSelectionLocked() : false;
}

void UDronePartSelectWidget::HandlePartSelectUIRefreshRequested()
{
	RefreshFromController();
	StartTimerTextRefresh();
}

void UDronePartSelectWidget::HandlePartSelectionResult(EPartSlot PartSlot, FName PartID, bool bSuccess, FString Reason)
{
	if (bSuccess)
	{
		HidePartSelectionServerError();
	}

	if (ResultText)
	{
		ResultText->SetText(FText::FromString(FString::Printf(
			TEXT("%s %s: %s"),
			bSuccess ? TEXT("Success") : TEXT("Fail"),
			*PartID.ToString(),
			*Reason)));
	}
}

void UDronePartSelectWidget::RetryRefreshFromController()
{
	RefreshRetryTimerHandle.Invalidate();
	RefreshFromController();
}

void UDronePartSelectWidget::HandleCorePrevClicked()
{
	MovePreview(EDronePartSlot::Core, -1);
}

void UDronePartSelectWidget::HandleCoreNextClicked()
{
	MovePreview(EDronePartSlot::Core, 1);
}

void UDronePartSelectWidget::HandleRightPrevClicked()
{
	MovePreview(EDronePartSlot::RightWeapon, -1);
}

void UDronePartSelectWidget::HandleRightNextClicked()
{
	MovePreview(EDronePartSlot::RightWeapon, 1);
}

void UDronePartSelectWidget::HandleLeftPrevClicked()
{
	MovePreview(EDronePartSlot::LeftWeapon, -1);
}

void UDronePartSelectWidget::HandleLeftNextClicked()
{
	MovePreview(EDronePartSlot::LeftWeapon, 1);
}

void UDronePartSelectWidget::HandleCombatStartClicked()
{
	if (!CachedRaidPlayerController)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Client] CombatStart skipped: Owning ARaidPlayerController is missing"));
		return;
	}

	CachedRaidPlayerController->RequestReadyForRaidFromUI();
}

void UDronePartSelectWidget::ApplyPlanningLayout()
{
	if (!WidgetTree)
	{
		return;
	}

	for (UButton* ArrowButton : {
		Button_CorePrev.Get(),
		Button_CoreNext.Get(),
		Button_RightPrev.Get(),
		Button_RightNext.Get(),
		Button_LeftPrev.Get(),
		Button_LeftNext.Get()})
	{
		if (ArrowButton)
		{
			ArrowButton->SetVisibility(ESlateVisibility::Visible);
		}
	}

	if (!Button_CombatStart)
	{
		return;
	}

	UCanvasPanel* Canvas = Cast<UCanvasPanel>(Button_CombatStart->GetParent());
	if (!Canvas)
	{
		return;
	}

	const auto AddPanel = [this, Canvas](
		FName PanelName,
		FVector2D Position,
		FVector2D Size,
		FLinearColor Color,
		int32 ZOrder,
		bool bStretchToCanvas = false)
	{
		UBorder* Panel = Cast<UBorder>(WidgetTree->FindWidget(PanelName));
		if (!Panel)
		{
			Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), PanelName);
			if (!Panel)
			{
				return;
			}
			Canvas->AddChildToCanvas(Panel);
		}

		Panel->SetBrushColor(Color);
		Panel->SetVisibility(ESlateVisibility::HitTestInvisible);

		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Panel->Slot))
		{
			if (bStretchToCanvas)
			{
				CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
				CanvasSlot->SetOffsets(FMargin(0.0f));
				CanvasSlot->SetAlignment(FVector2D::ZeroVector);
			}
			else
			{
				CanvasSlot->SetAnchors(FAnchors(0.5f, 0.5f));
				CanvasSlot->SetPosition(Position);
				CanvasSlot->SetSize(Size);
				CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			}
			CanvasSlot->SetZOrder(ZOrder);
		}
	};

	AddPanel(TEXT("Planning_Backdrop"), FVector2D::ZeroVector, FVector2D::ZeroVector,
		FLinearColor(0.94f, 0.96f, 0.98f, 1.0f), -100, true);
	AddPanel(TEXT("Planning_CoreCard"), FVector2D(0.0f, -350.0f), FVector2D(280.0f, 200.0f),
		FLinearColor(1.0f, 0.48f, 0.52f, 0.92f), -20);
	AddPanel(TEXT("Planning_RightCard"), FVector2D(0.0f, -70.0f), FVector2D(520.0f, 190.0f),
		FLinearColor(0.27f, 0.93f, 0.45f, 0.92f), -20);
	AddPanel(TEXT("Planning_LeftCard"), FVector2D(0.0f, 220.0f), FVector2D(520.0f, 190.0f),
		FLinearColor(0.33f, 0.62f, 0.96f, 0.92f), -20);
	// 설명 패널은 카드 바깥 가장자리에서 190px 떨어뜨리고 폭을 440으로 통일한다.
	// 종전에는 간격이 코어 190 / 우측 무기 120 / 좌측 무기 170으로 제각각이라
	// 우측 무기 설명이 화살표 버튼에 붙어 보였다. 폭 440은 좌우 화면 여백을 70으로 대칭시킨다
	// (앵커가 화면 중앙이므로 1920 기준 바깥 끝이 ±890이다).
	// 카드 절반폭: 코어 140, 무기 260. 설명 절반폭 220. 따라서 중심 오프셋은 카드절반 + 190 + 220이다.
	AddPanel(TEXT("Planning_CoreDescription"), FVector2D(550.0f, -350.0f), FVector2D(440.0f, 200.0f),
		FLinearColor(0.80f, 0.96f, 0.65f, 0.95f), -20);
	AddPanel(TEXT("Planning_RightDescription"), FVector2D(-670.0f, -70.0f), FVector2D(440.0f, 190.0f),
		FLinearColor(1.0f, 0.85f, 0.72f, 0.95f), -20);
	AddPanel(TEXT("Planning_LeftDescription"), FVector2D(670.0f, 220.0f), FVector2D(440.0f, 190.0f),
		FLinearColor(0.52f, 0.93f, 0.91f, 0.95f), -20);
	AddPanel(TEXT("Planning_Timer"), FVector2D(-660.0f, 410.0f), FVector2D(420.0f, 120.0f),
		FLinearColor(0.72f, 0.74f, 0.78f, 0.98f), -20);

	// 목업 `08` 확정 계약: `슬롯 하단 중앙에 회색 원형(남은 수량 표시 위치)이 붙는다`.
	// 원형은 에셋 없이 `RoundedBox` + `HalfHeightRadius`로 그린다 — 정사각형이면 정원이 된다.
	// 수량 텍스트를 배지의 Content로 넣어 세로·가로 중앙을 Border가 잡게 한다.
	// 이렇게 해야 배지 위치를 옮겨도 숫자가 항상 원 중심에 남는다.
	const auto AddCountBadge = [this, Canvas](FName BadgeName, FVector2D Position, UTextBlock* CountText)
	{
		constexpr float BadgeDiameter = 76.0f;

		UBorder* Badge = Cast<UBorder>(WidgetTree->FindWidget(BadgeName));
		if (!Badge)
		{
			Badge = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), BadgeName);
			if (!Badge)
			{
				return;
			}
			Canvas->AddChildToCanvas(Badge);
		}

		FSlateBrush CircleBrush;
		CircleBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
		CircleBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
		CircleBrush.TintColor = FSlateColor(FLinearColor(0.78f, 0.80f, 0.83f, 1.0f));
		Badge->SetBrush(CircleBrush);
		Badge->SetVisibility(ESlateVisibility::HitTestInvisible);
		Badge->SetPadding(FMargin(0.0f));
		Badge->SetHorizontalAlignment(HAlign_Center);
		Badge->SetVerticalAlignment(VAlign_Center);

		if (UCanvasPanelSlot* BadgeSlot = Cast<UCanvasPanelSlot>(Badge->Slot))
		{
			BadgeSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			BadgeSlot->SetPosition(Position);
			BadgeSlot->SetSize(FVector2D(BadgeDiameter, BadgeDiameter));
			BadgeSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			// 카드(-20)보다 위에 그려야 카드 경계에 걸친 원으로 보인다.
			BadgeSlot->SetZOrder(-10);
		}

		if (CountText)
		{
			CountText->SetJustification(ETextJustify::Center);
			Badge->SetContent(CountText);
		}
	};

	// 배지 중심을 카드 하단 경계에 두어 위쪽 절반이 카드에 걸치게 한다.
	// 카드 하단 = 중심 + 높이/2 → 코어 -350+100, 우측 무기 -70+95, 좌측 무기 220+95.
	AddCountBadge(TEXT("Planning_CoreCountBadge"), FVector2D(0.0f, -250.0f), Text_CoreCount.Get());
	AddCountBadge(TEXT("Planning_RightCountBadge"), FVector2D(0.0f, 25.0f), Text_RightWeaponCount.Get());
	AddCountBadge(TEXT("Planning_LeftCountBadge"), FVector2D(0.0f, 315.0f), Text_LeftWeaponCount.Get());

	if (!ServerErrorPopupPanel)
	{
		ServerErrorPopupPanel = Cast<UBorder>(WidgetTree->FindWidget(TEXT("Planning_ServerErrorPopup")));
	}
	if (!ServerErrorPopupPanel)
	{
		ServerErrorPopupPanel = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(),
			TEXT("Planning_ServerErrorPopup"));
		if (ServerErrorPopupPanel)
		{
			Canvas->AddChildToCanvas(ServerErrorPopupPanel);
		}
	}

	if (ServerErrorPopupPanel)
	{
		ServerErrorPopupPanel->SetBrushColor(FLinearColor(0.10f, 0.12f, 0.16f, 0.96f));
		ServerErrorPopupPanel->SetPadding(FMargin(24.0f, 18.0f));
		ServerErrorPopupPanel->SetVisibility(ESlateVisibility::Collapsed);
		if (UCanvasPanelSlot* PopupSlot = Cast<UCanvasPanelSlot>(ServerErrorPopupPanel->Slot))
		{
			PopupSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			PopupSlot->SetPosition(FVector2D::ZeroVector);
			PopupSlot->SetSize(FVector2D(760.0f, 100.0f));
			PopupSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			PopupSlot->SetZOrder(100);
		}
	}

	if (!ServerErrorPopupText)
	{
		ServerErrorPopupText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Planning_ServerErrorPopupText")));
	}
	if (!ServerErrorPopupText && ServerErrorPopupPanel)
	{
		ServerErrorPopupText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			TEXT("Planning_ServerErrorPopupText"));
		if (ServerErrorPopupText)
		{
			ServerErrorPopupPanel->SetContent(ServerErrorPopupText);
		}
	}
	if (ServerErrorPopupText)
	{
		ServerErrorPopupText->SetText(FText::FromString(TEXT("일시적인 오류가 발생했습니다. 다시 시도해주세요.")));
		ServerErrorPopupText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		ServerErrorPopupText->SetJustification(ETextJustify::Center);
	}

	const FSlateColor PrimaryTextColor(FLinearColor(0.05f, 0.08f, 0.12f, 1.0f));
	for (UTextBlock* TextBlock : {
		Text_ControlGuide.Get(),
		Text_CoreDescription.Get(),
		Text_RightWeaponDescription.Get(),
		Text_LeftWeaponDescription.Get(),
		Text_CoreCount.Get(),
		Text_RightWeaponCount.Get(),
		Text_LeftWeaponCount.Get(),
		TimerText.Get()})
	{
		if (TextBlock)
		{
			TextBlock->SetColorAndOpacity(PrimaryTextColor);
		}
	}

	for (UTextBlock* RedundantText : {
		CoreSelectedText.Get(),
		LeftWeaponSelectedText.Get(),
		RightWeaponSelectedText.Get(),
		ResultText.Get(),
		Text_FocusedSlot.Get()})
	{
		if (RedundantText)
		{
			RedundantText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UDronePartSelectWidget::InitializeCandidatesFromController()
{
	if (!CachedRaidPlayerController)
	{
		return;
	}

	CorePartIDs = CachedRaidPlayerController->GetCorePartIDs();
	WeaponPartIDs = CachedRaidPlayerController->GetWeaponPartIDs();

	CorePreviewIndex = CorePartIDs.IsValidIndex(CorePreviewIndex) ? CorePreviewIndex : 0;
	RightWeaponPreviewIndex = WeaponPartIDs.IsValidIndex(RightWeaponPreviewIndex) ? RightWeaponPreviewIndex : 0;
	LeftWeaponPreviewIndex = WeaponPartIDs.IsValidIndex(LeftWeaponPreviewIndex) ? LeftWeaponPreviewIndex : 0;
}

void UDronePartSelectWidget::SyncPreviewIndicesToSelection()
{
	if (!CachedRaidPlayerController)
	{
		return;
	}

	const int32 SelectedCoreIndex = CorePartIDs.IndexOfByKey(
		CachedRaidPlayerController->GetSelectedPartForSlot(EDronePartSlot::Core));
	if (SelectedCoreIndex != INDEX_NONE)
	{
		CorePreviewIndex = SelectedCoreIndex;
	}

	const int32 SelectedRightIndex = WeaponPartIDs.IndexOfByKey(
		CachedRaidPlayerController->GetSelectedPartForSlot(EDronePartSlot::RightWeapon));
	if (SelectedRightIndex != INDEX_NONE)
	{
		RightWeaponPreviewIndex = SelectedRightIndex;
	}

	const int32 SelectedLeftIndex = WeaponPartIDs.IndexOfByKey(
		CachedRaidPlayerController->GetSelectedPartForSlot(EDronePartSlot::LeftWeapon));
	if (SelectedLeftIndex != INDEX_NONE)
	{
		LeftWeaponPreviewIndex = SelectedLeftIndex;
	}
}

void UDronePartSelectWidget::BindButtonEvents()
{
	if (Button_CorePrev)
	{
		Button_CorePrev->OnClicked.RemoveDynamic(this, &UDronePartSelectWidget::HandleCorePrevClicked);
		Button_CorePrev->OnClicked.AddDynamic(this, &UDronePartSelectWidget::HandleCorePrevClicked);
	}
	if (Button_CoreNext)
	{
		Button_CoreNext->OnClicked.RemoveDynamic(this, &UDronePartSelectWidget::HandleCoreNextClicked);
		Button_CoreNext->OnClicked.AddDynamic(this, &UDronePartSelectWidget::HandleCoreNextClicked);
	}
	if (Button_RightPrev)
	{
		Button_RightPrev->OnClicked.RemoveDynamic(this, &UDronePartSelectWidget::HandleRightPrevClicked);
		Button_RightPrev->OnClicked.AddDynamic(this, &UDronePartSelectWidget::HandleRightPrevClicked);
	}
	if (Button_RightNext)
	{
		Button_RightNext->OnClicked.RemoveDynamic(this, &UDronePartSelectWidget::HandleRightNextClicked);
		Button_RightNext->OnClicked.AddDynamic(this, &UDronePartSelectWidget::HandleRightNextClicked);
	}
	if (Button_LeftPrev)
	{
		Button_LeftPrev->OnClicked.RemoveDynamic(this, &UDronePartSelectWidget::HandleLeftPrevClicked);
		Button_LeftPrev->OnClicked.AddDynamic(this, &UDronePartSelectWidget::HandleLeftPrevClicked);
	}
	if (Button_LeftNext)
	{
		Button_LeftNext->OnClicked.RemoveDynamic(this, &UDronePartSelectWidget::HandleLeftNextClicked);
		Button_LeftNext->OnClicked.AddDynamic(this, &UDronePartSelectWidget::HandleLeftNextClicked);
	}
	if (Button_CombatStart)
	{
		Button_CombatStart->OnClicked.RemoveDynamic(this, &UDronePartSelectWidget::HandleCombatStartClicked);
		Button_CombatStart->OnClicked.AddDynamic(this, &UDronePartSelectWidget::HandleCombatStartClicked);
	}
}

void UDronePartSelectWidget::UnbindButtonEvents()
{
	if (Button_CorePrev)
	{
		Button_CorePrev->OnClicked.RemoveDynamic(this, &UDronePartSelectWidget::HandleCorePrevClicked);
	}
	if (Button_CoreNext)
	{
		Button_CoreNext->OnClicked.RemoveDynamic(this, &UDronePartSelectWidget::HandleCoreNextClicked);
	}
	if (Button_RightPrev)
	{
		Button_RightPrev->OnClicked.RemoveDynamic(this, &UDronePartSelectWidget::HandleRightPrevClicked);
	}
	if (Button_RightNext)
	{
		Button_RightNext->OnClicked.RemoveDynamic(this, &UDronePartSelectWidget::HandleRightNextClicked);
	}
	if (Button_LeftPrev)
	{
		Button_LeftPrev->OnClicked.RemoveDynamic(this, &UDronePartSelectWidget::HandleLeftPrevClicked);
	}
	if (Button_LeftNext)
	{
		Button_LeftNext->OnClicked.RemoveDynamic(this, &UDronePartSelectWidget::HandleLeftNextClicked);
	}
	if (Button_CombatStart)
	{
		Button_CombatStart->OnClicked.RemoveDynamic(this, &UDronePartSelectWidget::HandleCombatStartClicked);
	}
}

void UDronePartSelectWidget::LogOptionalWidgetBindings() const
{
#if WITH_METADATA
	// FField::HasMetaData is compiled out when WITH_METADATA is 0 (Dedicated Server).
	// This is diagnostic logging for UMG binding, so the server simply skips it.
	for (TFieldIterator<FObjectProperty> PropertyIt(GetClass(), EFieldIteratorFlags::ExcludeSuper); PropertyIt; ++PropertyIt)
	{
		const FObjectProperty* Property = *PropertyIt;
		if (!Property || !Property->HasMetaData(TEXT("BindWidgetOptional")))
		{
			continue;
		}

		const UObject* BoundWidget = Property->GetObjectPropertyValue_InContainer(this);
		if (BoundWidget)
		{
			UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] WidgetBind Bound: WidgetName=%s WidgetClass=%s"),
				*Property->GetName(),
				*BoundWidget->GetClass()->GetName());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[DR_SUMMARY] WidgetBind Missing: WidgetName=%s"),
				*Property->GetName());
		}
	}
#endif // WITH_METADATA
}

void UDronePartSelectWidget::StartTimerTextRefresh()
{
	if (!CachedRaidPlayerController)
	{
		CachedRaidPlayerController = GetOwningRaidPlayerController();
	}

	RefreshTimerText();

	UWorld* World = GetWorld();
	if (!World || !ShouldRefreshTimerText())
	{
		return;
	}

	if (World->GetTimerManager().IsTimerActive(TimerTextRefreshTimerHandle))
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		TimerTextRefreshTimerHandle,
		this,
		&UDronePartSelectWidget::RefreshTimerText,
		TimerTextRefreshIntervalSeconds,
		true);
}

void UDronePartSelectWidget::StopTimerTextRefresh()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerTextRefreshTimerHandle);
	}
	TimerTextRefreshTimerHandle.Invalidate();
}

void UDronePartSelectWidget::RefreshTimerText()
{
	if (!CachedRaidPlayerController)
	{
		CachedRaidPlayerController = GetOwningRaidPlayerController();
	}

	const bool bShouldRefresh = ShouldRefreshTimerText();
	const float RemainingTime = bShouldRefresh && CachedRaidPlayerController
		? FMath::Max(0.0f, CachedRaidPlayerController->GetSelectionRemainingTime())
		: 0.0f;

	if (TimerText)
	{
		TimerText->SetText(FormatSelectionTimerText(RemainingTime));
	}

	if (!bShouldRefresh)
	{
		StopTimerTextRefresh();
	}
}

bool UDronePartSelectWidget::ShouldRefreshTimerText() const
{
	if (!TimerText || !CachedRaidPlayerController)
	{
		return false;
	}

	if (CachedRaidPlayerController->GetCurrentSelectionState() != EPlayerSelectionState::Selecting)
	{
		return false;
	}

	const ESlateVisibility CurrentVisibility = GetVisibility();
	return CurrentVisibility == ESlateVisibility::Visible
		|| CurrentVisibility == ESlateVisibility::SelfHitTestInvisible
		|| CurrentVisibility == ESlateVisibility::HitTestInvisible;
}

void UDronePartSelectWidget::SetFocusedSlot(EDronePartSlot NewFocusedSlot)
{
	// 가이드 2절: "부품/메뉴 포커스가 실제로 변경될 때 1회. 단순 마우스 이동에는 재생하지 않음."
	// 첫 설정은 화면을 여는 초기화라 울리지 않는다. 같은 슬롯을 다시 눌러도 마찬가지다.
	const bool bFocusActuallyMoved = bHasInitializedFocusAudio && FocusedSlot != NewFocusedSlot;
	bHasInitializedFocusAudio = true;

	FocusedSlot = NewFocusedSlot;
	bCombatStartFocused = false;
	RefreshFromController();
	SetKeyboardFocus();

	if (bFocusActuallyMoved && CachedRaidPlayerController)
	{
		CachedRaidPlayerController->PlayUIFocusSound();
	}
}

void UDronePartSelectWidget::RefreshSlot(
	EDronePartSlot PartSlot,
	FName PartID,
	UTextBlock* DescriptionText,
	UTextBlock* CountText,
	UImage* Image) const
{
	if (!CachedRaidPlayerController)
	{
		return;
	}

	if (PartID.IsNone())
	{
		if (DescriptionText)
		{
			DescriptionText->SetText(FText::FromString(TEXT("부품 없음")));
		}
		if (CountText)
		{
			CountText->SetText(FormatPartCountText(0));
		}
		if (Image)
		{
			Image->SetVisibility(ESlateVisibility::Hidden);
		}
		return;
	}

	const FText DisplayName = CachedRaidPlayerController->GetPartDisplayName(PartID);
	const FText Description = CachedRaidPlayerController->GetPartDescription(PartID);
	const int32 CurrentCount = CachedRaidPlayerController->GetPartCurrentCount(PartID);
	const FName SelectedPartID = CachedRaidPlayerController->GetSelectedPartForSlot(PartSlot);
	const bool bIsSelected = SelectedPartID == PartID;
	const bool bIsFocused = !bCombatStartFocused && FocusedSlot == PartSlot;

	if (DescriptionText)
	{
		DescriptionText->SetText(FText::Format(
			FText::FromString(TEXT("{0}{1}\n{2}")),
			bIsFocused ? FText::FromString(TEXT("> ")) : FText::GetEmpty(),
			DisplayName,
			Description));
	}

	if (CountText)
	{
		FString CountString = FormatPartCountText(CurrentCount).ToString();
		if (bIsSelected)
		{
			CountString += TEXT("\n[선택됨]");
		}
		CountText->SetText(FText::FromString(CountString));
	}

	if (Image)
	{
		if (UTexture2D* Icon = CachedRaidPlayerController->GetPartIcon(PartID))
		{
			Image->SetBrushFromTexture(Icon, true);
			Image->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			Image->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

int32& UDronePartSelectWidget::GetMutablePreviewIndexForSlot(EDronePartSlot PartSlot)
{
	switch (PartSlot)
	{
	case EDronePartSlot::Core:
		return CorePreviewIndex;
	case EDronePartSlot::RightWeapon:
		return RightWeaponPreviewIndex;
	case EDronePartSlot::LeftWeapon:
	default:
		return LeftWeaponPreviewIndex;
	}
}

int32 UDronePartSelectWidget::GetPreviewIndexForSlot(EDronePartSlot PartSlot) const
{
	switch (PartSlot)
	{
	case EDronePartSlot::Core:
		return CorePreviewIndex;
	case EDronePartSlot::RightWeapon:
		return RightWeaponPreviewIndex;
	case EDronePartSlot::LeftWeapon:
	default:
		return LeftWeaponPreviewIndex;
	}
}

const TArray<FName>& UDronePartSelectWidget::GetPartIDsForSlot(EDronePartSlot PartSlot) const
{
	switch (PartSlot)
	{
	case EDronePartSlot::Core:
		return CorePartIDs;
	case EDronePartSlot::RightWeapon:
	case EDronePartSlot::LeftWeapon:
	default:
		return WeaponPartIDs;
	}
}

FText UDronePartSelectWidget::GetSlotDisplayText(EDronePartSlot PartSlot) const
{
	switch (PartSlot)
	{
	case EDronePartSlot::Core:
		return FText::FromString(TEXT("Core"));
	case EDronePartSlot::RightWeapon:
		return FText::FromString(TEXT("RightWeapon"));
	case EDronePartSlot::LeftWeapon:
		return FText::FromString(TEXT("LeftWeapon"));
	default:
		return FText::FromString(TEXT("Unknown"));
	}
}

bool UDronePartSelectWidget::IsInventoryDataReadyForRefresh(FString& OutReason) const
{
	if (!CachedRaidPlayerController)
	{
		OutReason = TEXT("Owning ARaidPlayerController is missing");
		return false;
	}

	const UWorld* World = CachedRaidPlayerController->GetWorld();
	if (!World)
	{
		OutReason = TEXT("World is missing");
		return false;
	}

	const ARaidGameState* RaidGameState = World->GetGameState<ARaidGameState>();
	if (!RaidGameState)
	{
		OutReason = TEXT("RaidGameState is not replicated yet");
		return false;
	}

	if (!RaidGameState->GetDronePartInventory())
	{
		OutReason = TEXT("DronePartInventory is not replicated yet");
		return false;
	}

	const FName CorePartID = GetPreviewPartIDForSlot(EDronePartSlot::Core);
	if (!CorePartID.IsNone() && CachedRaidPlayerController->GetPartMaxCount(CorePartID) <= 0)
	{
		OutReason = FString::Printf(TEXT("MaxCount is 0 for %s"), *CorePartID.ToString());
		return false;
	}

	const FName RightPartID = GetPreviewPartIDForSlot(EDronePartSlot::RightWeapon);
	if (!RightPartID.IsNone() && CachedRaidPlayerController->GetPartMaxCount(RightPartID) <= 0)
	{
		OutReason = FString::Printf(TEXT("MaxCount is 0 for %s"), *RightPartID.ToString());
		return false;
	}

	const FName LeftPartID = GetPreviewPartIDForSlot(EDronePartSlot::LeftWeapon);
	if (!LeftPartID.IsNone() && CachedRaidPlayerController->GetPartMaxCount(LeftPartID) <= 0)
	{
		OutReason = FString::Printf(TEXT("MaxCount is 0 for %s"), *LeftPartID.ToString());
		return false;
	}

	return true;
}

void UDronePartSelectWidget::ScheduleRefreshRetry(const FString& Reason)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Client] DronePartSelectWidget refresh retry skipped: World missing Reason=%s"), *Reason);
		return;
	}

	if (RefreshRetryCount >= MaxRefreshRetryCount)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Client] DronePartSelectWidget refresh retry exhausted: Attempts=%d Reason=%s"),
			RefreshRetryCount,
			*Reason);
		return;
	}

	if (World->GetTimerManager().IsTimerActive(RefreshRetryTimerHandle))
	{
		return;
	}

	RefreshRetryCount++;
	UE_LOG(LogTemp, Log, TEXT("[Client] DronePartSelectWidget refresh retry scheduled: Attempt=%d/%d Delay=%.1fs Reason=%s"),
		RefreshRetryCount,
		MaxRefreshRetryCount,
		RefreshRetryDelaySeconds,
		*Reason);

	World->GetTimerManager().SetTimer(
		RefreshRetryTimerHandle,
		this,
		&UDronePartSelectWidget::RetryRefreshFromController,
		RefreshRetryDelaySeconds,
		false);
}

void UDronePartSelectWidget::ClearRefreshRetry()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshRetryTimerHandle);
	}
	RefreshRetryTimerHandle.Invalidate();
	RefreshRetryCount = 0;
}

void UDronePartSelectWidget::SetLoadingText(const FString& Reason)
{
	const FText LoadingText = FText::FromString(TEXT("부품 재고 동기화 중..."));
	const FText CountText = FText::FromString(TEXT("남은 수량: 동기화 중"));

	if (Text_CoreDescription)
	{
		Text_CoreDescription->SetText(LoadingText);
	}
	if (Text_RightWeaponDescription)
	{
		Text_RightWeaponDescription->SetText(LoadingText);
	}
	if (Text_LeftWeaponDescription)
	{
		Text_LeftWeaponDescription->SetText(LoadingText);
	}
	if (Text_CoreCount)
	{
		Text_CoreCount->SetText(CountText);
	}
	if (Text_RightWeaponCount)
	{
		Text_RightWeaponCount->SetText(CountText);
	}
	if (Text_LeftWeaponCount)
	{
		Text_LeftWeaponCount->SetText(CountText);
	}
	if (Text_FocusedSlot)
	{
		Text_FocusedSlot->SetText(FText::FromString(FString::Printf(TEXT("현재 선택: 동기화 대기 (%s)"), *Reason)));
	}
}
