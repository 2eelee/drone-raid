#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RaidLoadFailedWidget.generated.h"

// C++ 베이스: "로드에 실패했습니다." 중앙 표시 전용
// 레이아웃은 WBP_RaidLoadFailed(이 클래스 상속)에서 구성
UCLASS()
class DRONEPROTO_API URaidLoadFailedWidget : public UUserWidget
{
	GENERATED_BODY()
};
