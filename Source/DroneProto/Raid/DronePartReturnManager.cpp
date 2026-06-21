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
	default:
		return TEXT("Unknown");
	}
}
}

void UDronePartReturnManager::Initialize(ADronePartInventory* InInventory)
{
	Inventory = InInventory;
}

bool UDronePartReturnManager::ReturnSelectedParts(ARaidPlayerController* PC, EDronePartReturnReason Reason)
{
	bool bReturnedAny = false;
	bReturnedAny |= ReturnSingleSelectedPart(PC, EPartSlot::Core, Reason);
	bReturnedAny |= ReturnSingleSelectedPart(PC, EPartSlot::LeftWeapon, Reason);
	bReturnedAny |= ReturnSingleSelectedPart(PC, EPartSlot::RightWeapon, Reason);
	return bReturnedAny;
}

bool UDronePartReturnManager::ReturnEquippedParts(ARaidPlayerController* PC, EDronePartReturnReason Reason)
{
	bool bReturnedAny = false;
	bReturnedAny |= ReturnSingleEquippedPart(PC, EPartSlot::Core, Reason);
	bReturnedAny |= ReturnSingleEquippedPart(PC, EPartSlot::LeftWeapon, Reason);
	bReturnedAny |= ReturnSingleEquippedPart(PC, EPartSlot::RightWeapon, Reason);
	return bReturnedAny;
}

bool UDronePartReturnManager::ReturnSingleSelectedPart(ARaidPlayerController* PC, EPartSlot Slot, EDronePartReturnReason Reason)
{
	if (!PC || IsSelectedSlotAlreadyEmpty(PC, Slot))
	{
		UE_LOG(LogTemp, Log, TEXT("ReturnPart Skipped: Slot empty Slot=%s Reason=%s"),
			ToReturnSlotLogString(Slot),
			ToReturnReasonLogString(Reason));
		return false;
	}

	const FName PartID = PC->GetSelectedPartIDBySlot(Slot);
	if (!ReturnSinglePart(PC, PartID, Slot, Reason))
	{
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Return PC=%s Source=SelectedParts Part=%s Slot=%s Count=%d/%d Reason=%s"),
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
		return false;
	}

	const FName PartID = PC->GetEquippedPartIDBySlot(Slot);
	if (!ReturnSinglePart(PC, PartID, Slot, Reason))
	{
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Return PC=%s Source=EquippedParts Part=%s Slot=%s Count=%d/%d Reason=%s"),
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
