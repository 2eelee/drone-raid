#include "DronePartInventory.h"
#include "Net/UnrealNetwork.h"

ADronePartInventory::ADronePartInventory()
{
	bReplicates = true;
	SetReplicatingMovement(false);
	bAlwaysRelevant = true;
	NetDormancy = DORM_Never;
	SetNetUpdateFrequency(10.0f);

	InitializeDefaultStocks();
}

void ADronePartInventory::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADronePartInventory, PartStocks);
}

bool ADronePartInventory::TryConsumePart(FName PartID)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Client] TryConsumePart rejected locally: %s"), *PartID.ToString());
		return false;
	}

	FDronePartStock* Stock = FindStock(PartID);
	if (!Stock)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Server] TryConsumePart failed. Unknown PartID: %s"), *PartID.ToString());
		return false;
	}

	if (Stock->CurrentCount <= 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[Server] TryConsumePart failed. Out of stock: %s"), *PartID.ToString());
		return false;
	}

	Stock->CurrentCount--;
	UE_LOG(LogTemp, Log, TEXT("[Server] Consumed part: %s %d/%d"),
		*Stock->PartID.ToString(), Stock->CurrentCount, Stock->MaxCount);

	OnPartStocksChanged.Broadcast();
	ForceNetUpdate();
	return true;
}

bool ADronePartInventory::ReturnDronePart(FName PartID)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Client] ReturnPart rejected locally: %s"), *PartID.ToString());
		return false;
	}

	if (PartID.IsNone())
	{
		return false;
	}

	FDronePartStock* Stock = FindStock(PartID);
	if (!Stock)
	{
		UE_LOG(LogTemp, Warning, TEXT("ReturnPart Failed: Invalid PartID Part=%s"), *PartID.ToString());
		return false;
	}

	const int32 PreviousCount = Stock->CurrentCount;
	Stock->CurrentCount = FMath::Clamp(Stock->CurrentCount + 1, 0, Stock->MaxCount);
	UE_LOG(LogTemp, Log, TEXT("[Server] Returned part: %s %d/%d"),
		*Stock->PartID.ToString(), Stock->CurrentCount, Stock->MaxCount);

	if (Stock->CurrentCount != PreviousCount)
	{
		OnPartStocksChanged.Broadcast();
		ForceNetUpdate();
	}

	return true;
}

void ADronePartInventory::ReturnPart(FName PartID)
{
	ReturnDronePart(PartID);
}

TArray<FDronePartStock> ADronePartInventory::GetPartStocks() const
{
	return PartStocks;
}

int32 ADronePartInventory::GetCurrentCount(FName PartID) const
{
	if (const FDronePartStock* Stock = FindStock(PartID))
	{
		return Stock->CurrentCount;
	}

	return 0;
}

int32 ADronePartInventory::GetMaxCount(FName PartID) const
{
	if (const FDronePartStock* Stock = FindStock(PartID))
	{
		return Stock->MaxCount;
	}

	return 0;
}

bool ADronePartInventory::IsPartAvailable(FName PartID) const
{
	return GetCurrentCount(PartID) > 0;
}

bool ADronePartInventory::GetPartType(FName PartID, EDronePartType& OutType) const
{
	if (const FDronePartStock* Stock = FindStock(PartID))
	{
		OutType = Stock->PartType;
		return true;
	}

	return false;
}

FName ADronePartInventory::GetCoreZenithPartID()
{
	return TEXT("CORE_001");
}

FName ADronePartInventory::GetCoreBoosterPartID()
{
	return TEXT("CORE_002");
}

FName ADronePartInventory::GetCoreDrainPartID()
{
	return TEXT("CORE_003");
}

FName ADronePartInventory::GetPulseLaserPartID()
{
	return TEXT("WEAPON_001");
}

FName ADronePartInventory::GetFractureBurstPartID()
{
	return TEXT("WEAPON_002");
}

FName ADronePartInventory::GetVectorCannonPartID()
{
	return TEXT("WEAPON_003");
}

void ADronePartInventory::OnRep_PartStocks()
{
	UE_LOG(LogTemp, Log, TEXT("[Client] OnRep_PartStocks: Inventory=%s HasAuthority=%s LocalRole=%d Replicates=%s StockNum=%d"),
		*GetName(),
		HasAuthority() ? TEXT("true") : TEXT("false"),
		static_cast<int32>(GetLocalRole()),
		GetIsReplicated() ? TEXT("true") : TEXT("false"),
		PartStocks.Num());

	for (const FDronePartStock& Stock : PartStocks)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[Client] Part stock updated: %s %d/%d"),
			*Stock.PartID.ToString(), Stock.CurrentCount, Stock.MaxCount);
	}

	OnPartStocksChanged.Broadcast();
}

void ADronePartInventory::InitializeDefaultStocks()
{
	PartStocks = {
		{ GetCoreZenithPartID(), EDronePartType::Core, 5, 5 },
		{ GetCoreBoosterPartID(), EDronePartType::Core, 6, 6 },
		{ GetCoreDrainPartID(), EDronePartType::Core, 5, 5 },
		{ GetPulseLaserPartID(), EDronePartType::Weapon, 11, 11 },
		{ GetFractureBurstPartID(), EDronePartType::Weapon, 10, 10 },
		{ GetVectorCannonPartID(), EDronePartType::Weapon, 11, 11 }
	};
}

FDronePartStock* ADronePartInventory::FindStock(FName PartID)
{
	return PartStocks.FindByPredicate([PartID](const FDronePartStock& Stock)
	{
		return Stock.PartID == PartID;
	});
}

const FDronePartStock* ADronePartInventory::FindStock(FName PartID) const
{
	return PartStocks.FindByPredicate([PartID](const FDronePartStock& Stock)
	{
		return Stock.PartID == PartID;
	});
}
