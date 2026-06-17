#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RaidLobbyWidget.generated.h"

class URaidSessionSubsystem;

// C++ 베이스: 로직만. 레이아웃·버튼 배치는 WBP_RaidLobby(이 클래스 상속)에서 구성.
UCLASS()
class DRONEPROTO_API URaidLobbyWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 슬롯 버튼 클릭 → 이 함수 호출 (BP 이벤트 그래프에서 연결)
	UFUNCTION(BlueprintCallable, Category="Raid")
	void RequestEntry(const FString& SlotId);

	// 슬롯 활성 여부 — BP에서 버튼 IsEnabled 설정에 사용
	UFUNCTION(BlueprintPure, Category="Raid")
	bool IsSlotEnabled(const FString& SlotId) const;

protected:
	virtual void NativeConstruct() override;

private:
	UPROPERTY()
	TObjectPtr<URaidSessionSubsystem> RaidSubsystem;
};
