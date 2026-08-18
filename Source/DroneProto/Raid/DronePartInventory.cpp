#include "DronePartInventory.h"
#include "DroneDataTableRows.h"
#include "Engine/DataTable.h"
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

void ADronePartInventory::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		InitializeDefaultStocks();
		ValidateStockTotalsForServer();
	}
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

EDronePartSelectionCommitResult ADronePartInventory::TryCommitSelectionExchange(
	FName PreviousPartID,
	FName NewPartID,
	FString& OutFailureReason)
{
	OutFailureReason.Reset();

	if (!HasAuthority())
	{
		OutFailureReason = TEXT("Inventory has no authority.");
		return EDronePartSelectionCommitResult::ServerError;
	}

	FDronePartStock* NewStock = FindStock(NewPartID);
	if (!NewStock)
	{
		OutFailureReason = FString::Printf(TEXT("Unknown new PartID: %s"), *NewPartID.ToString());
		return EDronePartSelectionCommitResult::ServerError;
	}

	if (NewStock->CurrentCount <= 0)
	{
		OutFailureReason = FString::Printf(TEXT("Out of stock: %s"), *NewPartID.ToString());
		return EDronePartSelectionCommitResult::OutOfStock;
	}

	FDronePartStock* PreviousStock = nullptr;
	if (!PreviousPartID.IsNone())
	{
		PreviousStock = FindStock(PreviousPartID);
		if (!PreviousStock)
		{
			OutFailureReason = FString::Printf(TEXT("Unknown previous PartID: %s"), *PreviousPartID.ToString());
			return EDronePartSelectionCommitResult::ServerError;
		}

		if (PreviousStock->CurrentCount >= PreviousStock->MaxCount)
		{
			OutFailureReason = FString::Printf(TEXT("Previous part stock is already full: %s"), *PreviousPartID.ToString());
			return EDronePartSelectionCommitResult::ServerError;
		}
	}

	if (PreviousStock)
	{
		++PreviousStock->CurrentCount;
	}
	--NewStock->CurrentCount;

	UE_LOG(LogTemp, Log, TEXT("[Server] Atomic selection exchange committed. Previous=%s New=%s"),
		*PreviousPartID.ToString(),
		*NewPartID.ToString());
	OnPartStocksChanged.Broadcast();
	ForceNetUpdate();
	return EDronePartSelectionCommitResult::Success;
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
	if (InitializeStocksFromPartCountDataTable())
	{
		return;
	}

	InitializeFallbackStocks();
}

bool ADronePartInventory::InitializeStocksFromPartCountDataTable()
{
	if (!PartCountDataTable)
	{
		return false;
	}

	TArray<FDronePartCountRow*> Rows;
	static const FString ContextString(TEXT("ADronePartInventory::InitializeStocksFromPartCountDataTable"));
	PartCountDataTable->GetAllRows(ContextString, Rows);

	TArray<FDronePartStock> LoadedStocks;
	LoadedStocks.Reserve(Rows.Num());
	for (const FDronePartCountRow* Row : Rows)
	{
		if (!Row || Row->PartID.IsNone() || !Row->IsSelectable || Row->MaxCount < 0)
		{
			continue;
		}

		FDronePartStock Stock;
		Stock.PartID = Row->PartID;
		Stock.PartType = Row->Type;
		Stock.CurrentCount = Row->MaxCount;
		Stock.MaxCount = Row->MaxCount;
		LoadedStocks.Add(Stock);
	}

	if (LoadedStocks.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[DR_SUMMARY] PartStocks Source=DataTable Result=Fallback Reason=NoSelectableRows Table=%s"),
			*GetNameSafe(PartCountDataTable));
		return false;
	}

	PartStocks = MoveTemp(LoadedStocks);
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] PartStocks Source=DataTable Table=%s StockNum=%d"),
		*GetNameSafe(PartCountDataTable), PartStocks.Num());
	OnPartStocksChanged.Broadcast();
	ForceNetUpdate();
	return true;
}

void ADronePartInventory::InitializeFallbackStocks()
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

int32 ADronePartInventory::GetTotalMaxCountByType(EDronePartType PartType) const
{
	int32 TotalMaxCount = 0;
	for (const FDronePartStock& Stock : PartStocks)
	{
		if (Stock.PartType == PartType)
		{
			TotalMaxCount += Stock.MaxCount;
		}
	}

	return TotalMaxCount;
}

bool ADronePartInventory::ValidateStockTotalsForServer() const
{
	const int32 CoreTotal = GetTotalMaxCountByType(EDronePartType::Core);
	const int32 WeaponTotal = GetTotalMaxCountByType(EDronePartType::Weapon);
	const bool bIsValid = CoreTotal == ExpectedCoreTotalCount && WeaponTotal == ExpectedWeaponTotalCount;

	if (!bIsValid)
	{
		// 재고를 고치지 않는다. 여기서 보정하면 기획 데이터의 오류가 로그에서만 사라진다.
		UE_LOG(LogTemp, Warning,
			TEXT("[DR_SUMMARY] StockTotalsInvalid CoreTotal=%d ExpectedCoreTotal=%d WeaponTotal=%d ExpectedWeaponTotal=%d StockNum=%d"),
			CoreTotal, ExpectedCoreTotalCount, WeaponTotal, ExpectedWeaponTotalCount, PartStocks.Num());
	}

	return bIsValid;
}
