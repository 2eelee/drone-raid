#include "LobbyPlayerController.h"

void ALobbyPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalPlayerController()) return;

	if (LobbyWidgetClass)
	{
		if (URaidLobbyWidget* Widget = CreateWidget<URaidLobbyWidget>(this, LobbyWidgetClass))
		{
			Widget->AddToViewport();
		}
	}

	FInputModeUIOnly InputMode;
	SetInputMode(InputMode);
	SetShowMouseCursor(true);
}
