#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "DronePart.h"
#include "DronePartInventory.generated.h"

class UDataTable;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDronePartStocksChanged);

enum class EDronePartSelectionCommitResult : uint8
{
	Success,
	OutOfStock,
	ServerError
};

USTRUCT(BlueprintType)
struct DRONEPROTO_API FDronePartStock
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Drone Parts")
	FName PartID = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Drone Parts")
	EDronePartType PartType = EDronePartType::Core;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Drone Parts")
	int32 CurrentCount = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Drone Parts")
	int32 MaxCount = 0;
};

UCLASS()
class DRONEPROTO_API ADronePartInventory : public AInfo
{
	GENERATED_BODY()

public:
	ADronePartInventory();

	virtual void BeginPlay() override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Drone Parts|Server")
	bool TryConsumePart(FName PartID);

	EDronePartSelectionCommitResult TryCommitSelectionExchange(
		FName PreviousPartID,
		FName NewPartID,
		FString& OutFailureReason);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Drone Parts|Server")
	bool ReturnDronePart(FName PartID);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Drone Parts|Server")
	void ReturnPart(FName PartID);

	UFUNCTION(BlueprintPure, Category = "Drone Parts")
	TArray<FDronePartStock> GetPartStocks() const;

	UFUNCTION(BlueprintPure, Category = "Drone Parts")
	int32 GetCurrentCount(FName PartID) const;

	UFUNCTION(BlueprintPure, Category = "Drone Parts")
	int32 GetMaxCount(FName PartID) const;

	UFUNCTION(BlueprintPure, Category = "Drone Parts")
	bool IsPartAvailable(FName PartID) const;

	UFUNCTION(BlueprintPure, Category = "Drone Parts")
	bool GetPartType(FName PartID, EDronePartType& OutType) const;

	// 16인 레이드 전제: 코어는 인원수만큼 16개, 무기는 좌·우 슬롯이 있어 32개다.
	// 이 총량은 지금까지 DataTable 행 MaxCount의 합이라는 부수 효과로만 성립했고,
	// 표가 바뀌면 조용히 깨졌다. 기대값을 상수로 고정한다 (STOCK-09).
	static constexpr int32 ExpectedCoreTotalCount = 16;
	static constexpr int32 ExpectedWeaponTotalCount = 32;

	UFUNCTION(BlueprintPure, Category = "Drone Parts")
	int32 GetTotalMaxCountByType(EDronePartType PartType) const;

	// 총량이 기대값과 다르면 Warning만 남기고 재고는 고치지 않는다.
	// 서버가 임의로 보정하면 기획 데이터의 오류를 코드가 덮어써 원인이 감춰진다.
	bool ValidateStockTotalsForServer() const;

	UFUNCTION(BlueprintPure, Category = "Drone Parts|Part IDs")
	static FName GetCoreZenithPartID();

	UFUNCTION(BlueprintPure, Category = "Drone Parts|Part IDs")
	static FName GetCoreBoosterPartID();

	UFUNCTION(BlueprintPure, Category = "Drone Parts|Part IDs")
	static FName GetCoreDrainPartID();

	UFUNCTION(BlueprintPure, Category = "Drone Parts|Part IDs")
	static FName GetPulseLaserPartID();

	UFUNCTION(BlueprintPure, Category = "Drone Parts|Part IDs")
	static FName GetFractureBurstPartID();

	UFUNCTION(BlueprintPure, Category = "Drone Parts|Part IDs")
	static FName GetVectorCannonPartID();

	UPROPERTY(BlueprintAssignable, Category = "Drone Parts")
	FOnDronePartStocksChanged OnPartStocksChanged;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Drone Parts|Data")
	TObjectPtr<UDataTable> PartCountDataTable = nullptr;

	UPROPERTY(ReplicatedUsing = OnRep_PartStocks, VisibleInstanceOnly, BlueprintReadOnly, Category = "Drone Parts")
	TArray<FDronePartStock> PartStocks;

	UFUNCTION()
	void OnRep_PartStocks();

private:
	void InitializeDefaultStocks();
	bool InitializeStocksFromPartCountDataTable();
	void InitializeFallbackStocks();
	FDronePartStock* FindStock(FName PartID);
	const FDronePartStock* FindStock(FName PartID) const;
};
