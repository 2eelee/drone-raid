#include "LobbyPlayerController.h"

#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Components/AudioComponent.h"

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

	// 위젯 생성이 실패해도 배경음은 나야 한다. 아래 조기 반환보다 앞에 둔다.
	if (BGM_Lobby && !BGMAudioComponent)
	{
		// bAutoDestroy = false — 핸들을 들고 있어야 레이드로 떠날 때 끊을 수 있다.
		BGMAudioComponent = UGameplayStatics::SpawnSound2D(
			this, BGM_Lobby, BGMVolumeMultiplier, 1.0f, 0.0f, nullptr, false, false);
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

void ALobbyPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 레이드로 떠나면 컨트롤러가 사라지므로 로비 BGM도 함께 멎는다.
	if (BGMAudioComponent)
	{
		BGMAudioComponent->Stop();
		BGMAudioComponent = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void ALobbyPlayerController::PlayUISound(USoundBase* Sound) const
{
	// 에셋 미지정은 정상 상태다. 배선이 끝나지 않은 소리를 로그로 시끄럽게 만들지 않는다.
	if (!Sound || GetNetMode() == NM_DedicatedServer || !IsLocalController())
	{
		return;
	}

	UGameplayStatics::PlaySound2D(this, Sound);
}

void ALobbyPlayerController::PlayUIFocusSound()
{
	PlayUISound(SFX_UI_Focus);
}

void ALobbyPlayerController::PlayUIConfirmSound()
{
	PlayUISound(SFX_UI_Confirm);
}

void ALobbyPlayerController::PlayUICancelSound()
{
	PlayUISound(SFX_UI_Cancel);
}

void ALobbyPlayerController::PlayUIErrorSound()
{
	PlayUISound(SFX_UI_Error);
}

void ALobbyPlayerController::PlayUIMatchSuccessSound()
{
	PlayUISound(SFX_UI_MatchSuccess);
}
