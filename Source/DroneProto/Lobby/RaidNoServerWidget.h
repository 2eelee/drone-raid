#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RaidNoServerWidget.generated.h"

// C++ 베이스: "현재 입장 가능한 서버가 없습니다. 잠시 후 다시 시도해주세요." + 확인 버튼 로직
// 레이아웃은 WBP_RaidNoServer(이 클래스 상속)에서 구성
UCLASS()
class DRONEPROTO_API URaidNoServerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 확인 버튼 → 로비 복귀 (팝업 닫고 LobbyMap OpenLevel)
	UFUNCTION(BlueprintCallable, Category="Raid")
	void OnConfirmClicked();
};
