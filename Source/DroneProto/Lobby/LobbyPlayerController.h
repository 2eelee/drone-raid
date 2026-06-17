#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RaidLobbyWidget.h"
#include "LobbyPlayerController.generated.h"

UCLASS()
class DRONEPROTO_API ALobbyPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	// WBP_RaidLobby(URaidLobbyWidget 상속)를 에디터에서 지정
	UPROPERTY(EditDefaultsOnly, Category="UI")
	TSubclassOf<URaidLobbyWidget> LobbyWidgetClass;

protected:
	virtual void BeginPlay() override;
};
