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
	// Set this to WBP_RaidLobby in BP_LobbyPlayerController.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI")
	TSubclassOf<URaidLobbyWidget> LobbyWidgetClass;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TObjectPtr<URaidLobbyWidget> ActiveLobbyWidget;
};
