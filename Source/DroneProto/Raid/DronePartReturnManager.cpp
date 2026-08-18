#include "DronePartReturnManager.h"

#include "DronePartInventory.h"
#include "RaidPlayerController.h"
#include "GameFramework/PlayerState.h"

namespace
{
const TCHAR* ToReturnSlotLogString(EPartSlot Slot)
{
	switch (Slot)
	{
	case EPartSlot::Core:
		return TEXT("Core");
	case EPartSlot::LeftWeapon:
		return TEXT("LeftWeapon");
	case EPartSlot::RightWeapon:
		return TEXT("RightWeapon");
	default:
		return TEXT("Unknown");
	}
}

const TCHAR* ToReturnReasonLogString(EDronePartReturnReason Reason)
{
	switch (Reason)
	{
	case EDronePartReturnReason::Cancel:
		return TEXT("Cancel");
	case EDronePartReturnReason::Replace:
		return TEXT("Replace");
	case EDronePartReturnReason::Death:
		return TEXT("Death");
	case EDronePartReturnReason::Disconnect:
		return TEXT("Disconnect");
	case EDronePartReturnReason::RaidEnd:
		return TEXT("RaidEnd");
	case EDronePartReturnReason::Error:
		return TEXT("Error");
	case EDronePartReturnReason::Unspecified:
		return TEXT("Unspecified");
	default:
		return TEXT("Unknown");
	}
}

const TCHAR* ToReturnSourceLogString(EDronePartReturnSource Source)
{
	switch (Source)
	{
	case EDronePartReturnSource::Selected:
		return TEXT("Selected");
	case EDronePartReturnSource::Equipped:
		return TEXT("Equipped");
	default:
		return TEXT("Unknown");
	}
}

const EPartSlot ReturnSlotOrder[] = {EPartSlot::Core, EPartSlot::LeftWeapon, EPartSlot::RightWeapon};

FString BuildSlotListLogString(const TArray<EPartSlot>& Slots)
{
	TArray<FString> Names;
	Names.Reserve(Slots.Num());
	for (const EPartSlot Slot : Slots)
	{
		Names.Add(ToReturnSlotLogString(Slot));
	}
	return FString::Join(Names, TEXT(","));
}

const TCHAR* ToReturnSummaryLogName(EDronePartReturnReason Reason)
{
	switch (Reason)
	{
	case EDronePartReturnReason::Death:
		return TEXT("DeathReturn");
	case EDronePartReturnReason::RaidEnd:
		return TEXT("RaidEndReturn");
	default:
		return TEXT("Return");
	}
}
}

void UDronePartReturnManager::Initialize(ADronePartInventory* InInventory)
{
	Inventory = InInventory;
}

bool UDronePartReturnManager::ReturnSelectedParts(ARaidPlayerController* PC, EDronePartReturnReason Reason)
{
	return ReturnSelectedPartsBatch(PC, Reason).ReturnedAny();
}

bool UDronePartReturnManager::ReturnEquippedParts(ARaidPlayerController* PC, EDronePartReturnReason Reason)
{
	return ReturnEquippedPartsBatch(PC, Reason).ReturnedAny();
}

FDronePartReturnBatchResult UDronePartReturnManager::ReturnSelectedPartsBatch(
	ARaidPlayerController* PC,
	EDronePartReturnReason Reason)
{
	return ReturnPartsBatchInternal(PC, Reason, EDronePartReturnSource::Selected);
}

FDronePartReturnBatchResult UDronePartReturnManager::ReturnEquippedPartsBatch(
	ARaidPlayerController* PC,
	EDronePartReturnReason Reason)
{
	return ReturnPartsBatchInternal(PC, Reason, EDronePartReturnSource::Equipped);
}

FDronePartReturnBatchResult UDronePartReturnManager::ReturnPartsBatchInternal(
	ARaidPlayerController* PC,
	EDronePartReturnReason Reason,
	EDronePartReturnSource Source)
{
	FDronePartReturnBatchResult Result;

	for (const EPartSlot Slot : ReturnSlotOrder)
	{
		// 빈 슬롯은 실패가 아니라 skip이다. 반환 시도 전에 판정해야 둘을 구분할 수 있다.
		const bool bAlreadyEmpty = IsSlotAlreadyEmptyForSource(PC, Slot, Source);
		const FName PartID = GetPartIDForSource(PC, Slot, Source);

		if (ReturnSinglePartForSource(PC, Slot, Source, Reason))
		{
			Result.SucceededCount++;
			continue;
		}

		if (!PC || bAlreadyEmpty)
		{
			continue;
		}

		Result.FailedSlots.Add(Slot);
		RegisterPendingReturn(PC, Slot, Source, PartID);
	}

	if (Result.HasFailure())
	{
		// 부분 실패를 "다음 트리거까지 조용히 대기"가 아니라 즉시 관측 가능한 사건으로 만든다.
		UE_LOG(LogTemp, Warning, TEXT("[DR_SUMMARY] ReturnPartial PC=%s Source=%s Reason=%s Succeeded=%d Failed=%d Slots=%s"),
			*BuildPlayerID(PC),
			ToReturnSourceLogString(Source),
			ToReturnReasonLogString(Reason),
			Result.SucceededCount,
			Result.FailedSlots.Num(),
			*BuildSlotListLogString(Result.FailedSlots));
	}

	return Result;
}

FDronePartReturnRetryResult UDronePartReturnManager::RetryPendingReturnsForServer(
	ARaidPlayerController* PC,
	FName ContextReason)
{
	FDronePartReturnRetryResult Result;
	const FString ContextText = ContextReason.IsNone() ? TEXT("Manual") : ContextReason.ToString();

	for (int32 Index = PendingReturns.Num() - 1; Index >= 0; --Index)
	{
		const FPendingDronePartReturn Pending = PendingReturns[Index];
		ARaidPlayerController* PendingPC = Pending.PlayerController.Get();

		if (PC && PendingPC != PC)
		{
			continue;
		}

		if (!PendingPC)
		{
			// 슬롯을 확인할 대상이 사라져 중복 반환 여부를 판정할 수 없다. 조용히 지우지 않고 남긴다.
			UE_LOG(LogTemp, Warning, TEXT("[DR_SUMMARY] ReturnUnrecoverable PC=%s Part=%s Slot=%s Source=%s Context=%s Reason=ControllerGone"),
				*Pending.PlayerID,
				*Pending.DronePartID.ToString(),
				ToReturnSlotLogString(Pending.Slot),
				ToReturnSourceLogString(Pending.Source),
				*ContextText);
			Result.UnrecoverableCount++;
			PendingReturns.RemoveAt(Index);
			continue;
		}

		// 중복 반환 방지의 단일 기준이다 — 슬롯이 비었으면 이미 반환된 것이므로 재고를 두 번 늘리지 않는다.
		if (IsSlotAlreadyEmptyForSource(PendingPC, Pending.Slot, Pending.Source))
		{
			UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ReturnRetrySkipped PC=%s Slot=%s Source=%s Context=%s Reason=AlreadyEmpty"),
				*BuildPlayerID(PendingPC),
				ToReturnSlotLogString(Pending.Slot),
				ToReturnSourceLogString(Pending.Source),
				*ContextText);
			Result.SkippedCount++;
			PendingReturns.RemoveAt(Index);
			continue;
		}

		if (ReturnSinglePartForSource(PendingPC, Pending.Slot, Pending.Source, EDronePartReturnReason::Error))
		{
			Result.RecoveredCount++;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[DR_SUMMARY] ReturnUnrecoverable PC=%s Part=%s Slot=%s Source=%s Context=%s Reason=RetryFailed"),
				*BuildPlayerID(PendingPC),
				*Pending.DronePartID.ToString(),
				ToReturnSlotLogString(Pending.Slot),
				ToReturnSourceLogString(Pending.Source),
				*ContextText);
			Result.UnrecoverableCount++;
		}

		PendingReturns.RemoveAt(Index);
	}

	if (Result.RecoveredCount > 0 || Result.SkippedCount > 0 || Result.UnrecoverableCount > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ReturnRetry PC=%s Context=%s Recovered=%d Skipped=%d Unrecoverable=%d Pending=%d"),
			PC ? *BuildPlayerID(PC) : TEXT("All"),
			*ContextText,
			Result.RecoveredCount,
			Result.SkippedCount,
			Result.UnrecoverableCount,
			PendingReturns.Num());
	}

	return Result;
}

int32 UDronePartReturnManager::GetPendingReturnCount() const
{
	return PendingReturns.Num();
}

bool UDronePartReturnManager::IsSlotAlreadyEmptyForSource(
	const ARaidPlayerController* PC,
	EPartSlot Slot,
	EDronePartReturnSource Source) const
{
	return Source == EDronePartReturnSource::Selected
		? IsSelectedSlotAlreadyEmpty(PC, Slot)
		: IsEquippedSlotAlreadyEmpty(PC, Slot);
}

FName UDronePartReturnManager::GetPartIDForSource(
	const ARaidPlayerController* PC,
	EPartSlot Slot,
	EDronePartReturnSource Source) const
{
	if (!PC)
	{
		return NAME_None;
	}

	return Source == EDronePartReturnSource::Selected
		? PC->GetSelectedPartIDBySlot(Slot)
		: PC->GetEquippedPartIDBySlot(Slot);
}

bool UDronePartReturnManager::ReturnSinglePartForSource(
	ARaidPlayerController* PC,
	EPartSlot Slot,
	EDronePartReturnSource Source,
	EDronePartReturnReason Reason)
{
	return Source == EDronePartReturnSource::Selected
		? ReturnSingleSelectedPart(PC, Slot, Reason)
		: ReturnSingleEquippedPart(PC, Slot, Reason);
}

void UDronePartReturnManager::RegisterPendingReturn(
	ARaidPlayerController* PC,
	EPartSlot Slot,
	EDronePartReturnSource Source,
	FName PartID)
{
	if (!PC)
	{
		return;
	}

	// 같은 슬롯이 반복 실패해도 후보는 1건만 유지한다.
	for (FPendingDronePartReturn& Existing : PendingReturns)
	{
		if (Existing.PlayerController.Get() == PC && Existing.Slot == Slot && Existing.Source == Source)
		{
			Existing.DronePartID = PartID;
			return;
		}
	}

	FPendingDronePartReturn& Pending = PendingReturns.AddDefaulted_GetRef();
	Pending.PlayerController = PC;
	Pending.Slot = Slot;
	Pending.Source = Source;
	Pending.DronePartID = PartID;
	Pending.PlayerID = BuildPlayerID(PC);
}

bool UDronePartReturnManager::ReturnSingleSelectedPart(ARaidPlayerController* PC, EPartSlot Slot, EDronePartReturnReason Reason)
{
	if (!PC || IsSelectedSlotAlreadyEmpty(PC, Slot))
	{
		UE_LOG(LogTemp, Log, TEXT("ReturnPart Skipped: Slot empty Slot=%s Reason=%s"),
			ToReturnSlotLogString(Slot),
			ToReturnReasonLogString(Reason));
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ReturnSkipped PC=%s Reason=AlreadyEmpty Slot=%s"),
			*BuildPlayerID(PC),
			ToReturnSlotLogString(Slot));
		return false;
	}

	const FName PartID = PC->GetSelectedPartIDBySlot(Slot);
	if (!ReturnSinglePart(PC, PartID, Slot, Reason))
	{
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] %s PC=%s Part=%s Slot=%s Count=%d/%d Result=Success Reason=%s"),
		ToReturnSummaryLogName(Reason),
		*BuildPlayerID(PC),
		*PartID.ToString(),
		ToReturnSlotLogString(Slot),
		Inventory ? Inventory->GetCurrentCount(PartID) : 0,
		Inventory ? Inventory->GetMaxCount(PartID) : 0,
		ToReturnReasonLogString(Reason));

	PC->SetSelectedPartIDForSlotForServer(Slot, NAME_None);
	return true;
}

bool UDronePartReturnManager::ReturnSingleEquippedPart(ARaidPlayerController* PC, EPartSlot Slot, EDronePartReturnReason Reason)
{
	if (!PC || IsEquippedSlotAlreadyEmpty(PC, Slot))
	{
		UE_LOG(LogTemp, Log, TEXT("ReturnPart Skipped: Slot empty Slot=%s Reason=%s"),
			ToReturnSlotLogString(Slot),
			ToReturnReasonLogString(Reason));
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ReturnSkipped PC=%s Reason=AlreadyEmpty Slot=%s"),
			*BuildPlayerID(PC),
			ToReturnSlotLogString(Slot));
		return false;
	}

	const FName PartID = PC->GetEquippedPartIDBySlot(Slot);
	if (!ReturnSinglePart(PC, PartID, Slot, Reason))
	{
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] %s PC=%s Part=%s Slot=%s Count=%d/%d Result=Success Reason=%s"),
		ToReturnSummaryLogName(Reason),
		*BuildPlayerID(PC),
		*PartID.ToString(),
		ToReturnSlotLogString(Slot),
		Inventory ? Inventory->GetCurrentCount(PartID) : 0,
		Inventory ? Inventory->GetMaxCount(PartID) : 0,
		ToReturnReasonLogString(Reason));

	PC->SetEquippedPartIDForSlotForServer(Slot, NAME_None);
	return true;
}

bool UDronePartReturnManager::ReturnSinglePart(
	ARaidPlayerController* PC,
	FName PartID,
	EPartSlot Slot,
	EDronePartReturnReason Reason)
{
	if (!ValidateReturn(PC, PartID, Slot, Reason))
	{
		SaveReturnLog(PC, PartID, Slot, Reason, false);
		return false;
	}

	const bool bReturned = Inventory->ReturnDronePart(PartID);
	SaveReturnLog(PC, PartID, Slot, Reason, bReturned);

	if (bReturned)
	{
		UE_LOG(LogTemp, Log, TEXT("ReturnPart Success: Player=%s Part=%s Slot=%s Reason=%s Count=%d/%d"),
			*BuildPlayerID(PC),
			*PartID.ToString(),
			ToReturnSlotLogString(Slot),
			ToReturnReasonLogString(Reason),
			Inventory->GetCurrentCount(PartID),
			Inventory->GetMaxCount(PartID));
	}

	return bReturned;
}

EDronePartSelectionCommitResult UDronePartReturnManager::TryCommitSelectedPartChange(
	ARaidPlayerController* PC,
	EPartSlot Slot,
	FName NewPartID,
	FString& OutFailureReason)
{
	OutFailureReason.Reset();

	if (!PC || !PC->HasAuthority())
	{
		OutFailureReason = TEXT("Player controller authority is missing.");
		return EDronePartSelectionCommitResult::ServerError;
	}

	if (!Inventory || !Inventory->HasAuthority())
	{
		OutFailureReason = TEXT("Inventory authority is missing.");
		return EDronePartSelectionCommitResult::ServerError;
	}

	if (NewPartID.IsNone())
	{
		OutFailureReason = TEXT("New PartID is None.");
		return EDronePartSelectionCommitResult::ServerError;
	}

	switch (Slot)
	{
	case EPartSlot::Core:
	case EPartSlot::LeftWeapon:
	case EPartSlot::RightWeapon:
		break;
	default:
		OutFailureReason = TEXT("Invalid selection slot.");
		return EDronePartSelectionCommitResult::ServerError;
	}

	const FName PreviousPartID = PC->GetSelectedPartIDBySlot(Slot);
	if (PreviousPartID == NewPartID)
	{
		return EDronePartSelectionCommitResult::Success;
	}

	const EDronePartSelectionCommitResult CommitResult = Inventory->TryCommitSelectionExchange(
		PreviousPartID,
		NewPartID,
		OutFailureReason);
	if (CommitResult != EDronePartSelectionCommitResult::Success)
	{
		return CommitResult;
	}

	PC->SetSelectedPartIDForSlotForServer(Slot, NewPartID);
	if (!PreviousPartID.IsNone())
	{
		SaveReturnLog(PC, PreviousPartID, Slot, EDronePartReturnReason::Replace, true);
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Return PC=%s Part=%s Slot=%s Count=%d/%d Result=Success Reason=Replace"),
			*BuildPlayerID(PC),
			*PreviousPartID.ToString(),
			ToReturnSlotLogString(Slot),
			Inventory->GetCurrentCount(PreviousPartID),
			Inventory->GetMaxCount(PreviousPartID));
	}

	return EDronePartSelectionCommitResult::Success;
}

bool UDronePartReturnManager::ValidateReturn(
	ARaidPlayerController* PC,
	FName PartID,
	EPartSlot Slot,
	EDronePartReturnReason Reason) const
{
	if (!Inventory)
	{
		UE_LOG(LogTemp, Warning, TEXT("ReturnPart Failed: Inventory missing Player=%s Part=%s Slot=%s Reason=%s"),
			*BuildPlayerID(PC),
			*PartID.ToString(),
			ToReturnSlotLogString(Slot),
			ToReturnReasonLogString(Reason));
		return false;
	}

	if (!Inventory->HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("ReturnPart Failed: No authority Player=%s Part=%s Slot=%s Reason=%s"),
			*BuildPlayerID(PC),
			*PartID.ToString(),
			ToReturnSlotLogString(Slot),
			ToReturnReasonLogString(Reason));
		return false;
	}

	if (PartID.IsNone())
	{
		return false;
	}

	EDronePartType IgnoredType = EDronePartType::Core;
	if (!Inventory->GetPartType(PartID, IgnoredType))
	{
		UE_LOG(LogTemp, Warning, TEXT("ReturnPart Failed: Invalid PartID Player=%s Part=%s Slot=%s Reason=%s"),
			*BuildPlayerID(PC),
			*PartID.ToString(),
			ToReturnSlotLogString(Slot),
			ToReturnReasonLogString(Reason));
		return false;
	}

	return true;
}

bool UDronePartReturnManager::IsSelectedSlotAlreadyEmpty(const ARaidPlayerController* PC, EPartSlot Slot) const
{
	return !PC || PC->GetSelectedPartIDBySlot(Slot).IsNone();
}

bool UDronePartReturnManager::IsEquippedSlotAlreadyEmpty(const ARaidPlayerController* PC, EPartSlot Slot) const
{
	return !PC || PC->GetEquippedPartIDBySlot(Slot).IsNone();
}

const TArray<FReturnedDronePartLog>& UDronePartReturnManager::GetReturnLogs() const
{
	return ReturnLogs;
}

void UDronePartReturnManager::SaveReturnLog(
	ARaidPlayerController* PC,
	FName PartID,
	EPartSlot Slot,
	EDronePartReturnReason Reason,
	bool bIsProcessed)
{
	FReturnedDronePartLog& Log = ReturnLogs.AddDefaulted_GetRef();
	Log.LogID = NextLogID++;
	Log.PlayerID = BuildPlayerID(PC);
	Log.DronePartID = PartID;
	Log.Slot = Slot;
	Log.ReturnReason = Reason;
	Log.ReturnTime = FDateTime::UtcNow();
	Log.bIsProcessed = bIsProcessed;
}

FString UDronePartReturnManager::BuildPlayerID(const ARaidPlayerController* PC) const
{
	return ARaidPlayerController::BuildStableControllerLogString(PC);
}
