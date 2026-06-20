#include "DronePartSelectWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "InputCoreTypes.h"
#include "RaidGameState.h"
#include "RaidPlayerController.h"
#include "Engine/World.h"

void UDronePartSelectWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);
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
		CachedRaidPlayerController->RefreshDronePartInventoryBinding();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Client] DronePartSelectWidget has no ARaidPlayerController owning player"));
	}

	BindButtonEvents();
	RefreshFromController();
	SetKeyboardFocus();
}

void UDronePartSelectWidget::NativeDestruct()
{
	UnbindButtonEvents();
	ClearRefreshRetry();

	if (CachedRaidPlayerController)
	{
		CachedRaidPlayerController->OnPartSelectUIRefreshRequested.RemoveDynamic(
			this,
			&UDronePartSelectWidget::HandlePartSelectUIRefreshRequested);
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
		MovePreview(FocusedSlot, -1);
		return FReply::Handled();
	}

	if (Key == EKeys::Right)
	{
		MovePreview(FocusedSlot, 1);
		return FReply::Handled();
	}

	if (Key == EKeys::Z)
	{
		SelectFocusedPart();
		return FReply::Handled();
	}

	if (Key == EKeys::C)
	{
		CancelFocusedPart();
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
		Text_ControlGuide->SetText(FText::FromString(TEXT("↑/↓ 슬롯 이동   ←/→ 부품 변경   Z 선택   C 취소")));
	}

	UE_LOG(LogTemp, VeryVerbose, TEXT("[Client] DronePartSelectWidget RefreshFromController: Player=%s FocusedSlot=%s Core=%s Right=%s Left=%s"),
		*CachedRaidPlayerController->GetName(),
		*GetSlotDisplayText(FocusedSlot).ToString(),
		*GetPreviewPartIDForSlot(EDronePartSlot::Core).ToString(),
		*GetPreviewPartIDForSlot(EDronePartSlot::RightWeapon).ToString(),
		*GetPreviewPartIDForSlot(EDronePartSlot::LeftWeapon).ToString());

	if (Text_FocusedSlot)
	{
		Text_FocusedSlot->SetText(FText::Format(
			FText::FromString(TEXT("현재 선택: {0}")),
			GetSlotDisplayText(FocusedSlot)));
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
		SetFocusedSlot(EDronePartSlot::Core);
		break;
	}
}

void UDronePartSelectWidget::FocusPrevSlot()
{
	switch (FocusedSlot)
	{
	case EDronePartSlot::Core:
		SetFocusedSlot(EDronePartSlot::LeftWeapon);
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
	if (!CachedRaidPlayerController)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Client] CancelFocusedPart skipped: Owning ARaidPlayerController is missing"));
		return;
	}

	CachedRaidPlayerController->RequestCancelPartFromUI(FocusedSlot);
}

void UDronePartSelectWidget::HandlePartSelectUIRefreshRequested()
{
	RefreshFromController();
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

void UDronePartSelectWidget::SetFocusedSlot(EDronePartSlot NewFocusedSlot)
{
	FocusedSlot = NewFocusedSlot;
	RefreshFromController();
	SetKeyboardFocus();
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
			CountText->SetText(FText::FromString(TEXT("남은 수량: 0 / 0")));
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
	const int32 MaxCount = CachedRaidPlayerController->GetPartMaxCount(PartID);
	const FName SelectedPartID = CachedRaidPlayerController->GetSelectedPartForSlot(PartSlot);
	const bool bIsSelected = SelectedPartID == PartID;
	const bool bIsFocused = FocusedSlot == PartSlot;

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
		FString CountString = FString::Printf(TEXT("남은 수량: %d / %d"), CurrentCount, MaxCount);
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
