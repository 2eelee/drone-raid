#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RaidMatchmakingWaitWidget.generated.h"

// C++ 베이스: "서버를 찾고 있습니다…" + 취소 버튼 로직
// 레이아웃은 WBP_RaidMatchmakingWait(이 클래스 상속)에서 구성
UCLASS()
class DRONEPROTO_API URaidMatchmakingWaitWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 취소 버튼 → BP 이벤트 그래프에서 연결
	UFUNCTION(BlueprintCallable, Category="Raid")
	void OnCancelClicked();
};
