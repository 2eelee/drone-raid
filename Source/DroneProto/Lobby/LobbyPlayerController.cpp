#include "LobbyPlayerController.h"

namespace
{
const TCHAR* ToLobbyNetModeText(ENetMode NetMode)
{
	switch (NetMode)
	{
	case NM_Standalone:
		return TEXT("Standalone");
	case NM_DedicatedServer:
		return TEXT("DedicatedServer");
	case NM_ListenServer:
		return TEXT("ListenServer");
	case NM_Client:
		return TEXT("Client");
	default:
		return TEXT("Unknown");
	}
}
}

void ALobbyPlayerController::BeginPlay()
{
	Super::BeginPlay();

	const ENetMode NetMode = GetNetMode();
	const bool bIsDedicatedServer = NetMode == NM_DedicatedServer;
	const bool bIsLocalController = IsLocalController();

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] LobbyPCBeginPlay PC=%s NetMode=%s IsLocalController=%d IsDedicatedServer=%d WidgetClassValid=%d WidgetClass=%s"),
		*GetName(),
		ToLobbyNetModeText(NetMode),
		bIsLocalController ? 1 : 0,
		bIsDedicatedServer ? 1 : 0,
		LobbyWidgetClass ? 1 : 0,
		*GetPathNameSafe(LobbyWidgetClass.Get()));

	if (bIsDedicatedServer || !bIsLocalController)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] LobbyWidgetCreateAttempt PC=%s WidgetClassValid=%d WidgetClass=%s"),
		*GetName(),
		LobbyWidgetClass ? 1 : 0,
		*GetPathNameSafe(LobbyWidgetClass.Get()));

	if (!LobbyWidgetClass)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] LobbyWidgetMissingClass PC=%s"), *GetName());
		return;
	}

	URaidLobbyWidget* Widget = CreateWidget<URaidLobbyWidget>(this, LobbyWidgetClass);
	if (!Widget)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] LobbyWidgetCreateFailed PC=%s WidgetClass=%s"),
			*GetName(),
			*GetPathNameSafe(LobbyWidgetClass.Get()));
		return;
	}

	ActiveLobbyWidget = Widget;
	ActiveLobbyWidget->AddToViewport();

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(ActiveLobbyWidget->TakeWidget());
	SetInputMode(InputMode);
	SetShowMouseCursor(true);

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] LobbyWidgetShown PC=%s Widget=%s WidgetClass=%s InputMode=UIOnly bShowMouseCursor=%d"),
		*GetName(),
		*GetNameSafe(ActiveLobbyWidget),
		*GetPathNameSafe(LobbyWidgetClass.Get()),
		bShowMouseCursor ? 1 : 0);
}
