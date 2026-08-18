#pragma once

#include "CoreMinimal.h"
#include "DronePart.h"
#include "DronePartInventory.h"
#include "UObject/Object.h"
#include "DronePartReturnManager.generated.h"

class ARaidPlayerController;

UENUM(BlueprintType)
enum class EDronePartReturnReason : uint8
{
	Cancel,
	Replace,
	Death,
	Disconnect,
	RaidEnd,
	Error,
	// 기획 원문 6종에는 없는 내부 값이다. `Error`(서버 오류 복구 반환)와 "사유를 지정하지 않은 레코드"를
	// 구분하려고 추가했으며, 기존 값의 정수 순서를 보존하려고 맨 뒤에 둔다.
	Unspecified
};

// 반환 요청이 선택 슬롯에서 왔는지 장착 슬롯에서 왔는지 구분한다. 재처리는 원래 소스와 같은 슬롯만 본다.
enum class EDronePartReturnSource : uint8
{
	Selected,
	Equipped
};

// 3슬롯 일괄 반환의 결과다. 빈 슬롯은 실패가 아니라 skip이므로 `FailedSlots`에 들어가지 않는다.
struct DRONEPROTO_API FDronePartReturnBatchResult
{
	int32 SucceededCount = 0;
	TArray<EPartSlot> FailedSlots;

	bool HasFailure() const { return FailedSlots.Num() > 0; }
	bool ReturnedAny() const { return SucceededCount > 0; }
};

// 재처리 1회의 집계다. `SkippedCount`는 슬롯이 이미 비어 있어 건너뛴 건수다(중복 반환 방지).
struct DRONEPROTO_API FDronePartReturnRetryResult
{
	int32 RecoveredCount = 0;
	int32 SkippedCount = 0;
	int32 UnrecoverableCount = 0;
};

USTRUCT(BlueprintType)
struct DRONEPROTO_API FReturnedDronePartLog
{
	GENERATED_BODY()

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Drone Parts|Return")
	int32 LogID = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Drone Parts|Return")
	FString PlayerID;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Drone Parts|Return")
	FName DronePartID = NAME_None;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Drone Parts|Return")
	EPartSlot Slot = EPartSlot::Core;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Drone Parts|Return")
	EDronePartReturnReason ReturnReason = EDronePartReturnReason::Unspecified;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Drone Parts|Return")
	FDateTime ReturnTime;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Drone Parts|Return")
	bool bIsProcessed = false;
};

UCLASS()
class DRONEPROTO_API UDronePartReturnManager : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(ADronePartInventory* InInventory);

	bool ReturnSelectedParts(ARaidPlayerController* PC, EDronePartReturnReason Reason);
	bool ReturnEquippedParts(ARaidPlayerController* PC, EDronePartReturnReason Reason);

	// 위 두 함수의 상세 결과판이다. 기존 bool API는 이쪽에 위임하므로 호출부 계약은 바뀌지 않는다.
	FDronePartReturnBatchResult ReturnSelectedPartsBatch(ARaidPlayerController* PC, EDronePartReturnReason Reason);
	FDronePartReturnBatchResult ReturnEquippedPartsBatch(ARaidPlayerController* PC, EDronePartReturnReason Reason);

	// 실패로 등록된 반환을 `Error` 사유로 다시 시도한다. PC가 null이면 등록된 전체를 대상으로 한다.
	// 실행 여부의 단일 판정 기준은 슬롯이 아직 채워져 있는가이며, 로그는 판정 권한을 갖지 않는다(RETURN-11 결정).
	FDronePartReturnRetryResult RetryPendingReturnsForServer(ARaidPlayerController* PC, FName ContextReason);
	int32 GetPendingReturnCount() const;
	bool ReturnSingleSelectedPart(ARaidPlayerController* PC, EPartSlot Slot, EDronePartReturnReason Reason);
	bool ReturnSingleEquippedPart(ARaidPlayerController* PC, EPartSlot Slot, EDronePartReturnReason Reason);
	bool ReturnSinglePart(ARaidPlayerController* PC, FName PartID, EPartSlot Slot, EDronePartReturnReason Reason);
	EDronePartSelectionCommitResult TryCommitSelectedPartChange(
		ARaidPlayerController* PC,
		EPartSlot Slot,
		FName NewPartID,
		FString& OutFailureReason);

	bool ValidateReturn(ARaidPlayerController* PC, FName PartID, EPartSlot Slot, EDronePartReturnReason Reason) const;
	bool IsSelectedSlotAlreadyEmpty(const ARaidPlayerController* PC, EPartSlot Slot) const;
	bool IsEquippedSlotAlreadyEmpty(const ARaidPlayerController* PC, EPartSlot Slot) const;

	const TArray<FReturnedDronePartLog>& GetReturnLogs() const;

private:
	// 재처리 후보 1건. 이 목록은 "무엇을 다시 시도할지"만 담고 중복 반환 여부는 판정하지 않는다.
	struct FPendingDronePartReturn
	{
		TWeakObjectPtr<ARaidPlayerController> PlayerController;
		EPartSlot Slot = EPartSlot::Core;
		EDronePartReturnSource Source = EDronePartReturnSource::Selected;
		FName DronePartID = NAME_None;
		FString PlayerID;
	};

	UPROPERTY()
	ADronePartInventory* Inventory = nullptr;

	UPROPERTY()
	TArray<FReturnedDronePartLog> ReturnLogs;

	TArray<FPendingDronePartReturn> PendingReturns;

	int32 NextLogID = 1;

	FDronePartReturnBatchResult ReturnPartsBatchInternal(
		ARaidPlayerController* PC,
		EDronePartReturnReason Reason,
		EDronePartReturnSource Source);
	bool IsSlotAlreadyEmptyForSource(const ARaidPlayerController* PC, EPartSlot Slot, EDronePartReturnSource Source) const;
	FName GetPartIDForSource(const ARaidPlayerController* PC, EPartSlot Slot, EDronePartReturnSource Source) const;
	bool ReturnSinglePartForSource(ARaidPlayerController* PC, EPartSlot Slot, EDronePartReturnSource Source, EDronePartReturnReason Reason);
	void RegisterPendingReturn(ARaidPlayerController* PC, EPartSlot Slot, EDronePartReturnSource Source, FName PartID);

	void SaveReturnLog(ARaidPlayerController* PC, FName PartID, EPartSlot Slot, EDronePartReturnReason Reason, bool bIsProcessed);
	FString BuildPlayerID(const ARaidPlayerController* PC) const;
};
