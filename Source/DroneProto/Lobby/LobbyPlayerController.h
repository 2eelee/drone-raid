#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RaidLobbyWidget.h"
#include "LobbyPlayerController.generated.h"

class USoundBase;
class UAudioComponent;

UCLASS()
class DRONEPROTO_API ALobbyPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	// Set this to WBP_RaidLobby in BP_LobbyPlayerController.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI")
	TSubclassOf<URaidLobbyWidget> LobbyWidgetClass;

	// ---- UI SFX ----
	// 로비의 UI 입력은 전부 UMG 쪽에 있으므로 위젯이 직접 부른다.
	// 레이드 쪽과 달리 서버 판정을 거치는 지점이 없어 자동으로 걸 자리가 없다.

	/** 메뉴 포커스가 실제로 바뀐 순간. 단순 마우스 이동에는 부르지 않는다. */
	UFUNCTION(BlueprintCallable, Category = "Lobby|Audio")
	void PlayUIFocusSound();

	/** 확정·OK. */
	UFUNCTION(BlueprintCallable, Category = "Lobby|Audio")
	void PlayUIConfirmSound();

	/** 취소·뒤로. 매칭 취소를 포함한다. */
	UFUNCTION(BlueprintCallable, Category = "Lobby|Audio")
	void PlayUICancelSound();

	/** 입력 실패·매칭 실패. */
	UFUNCTION(BlueprintCallable, Category = "Lobby|Audio")
	void PlayUIErrorSound();

	/** 서버 접속·매칭 성공. */
	UFUNCTION(BlueprintCallable, Category = "Lobby|Audio")
	void PlayUIMatchSuccessSound();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Audio|UI")
	TObjectPtr<USoundBase> SFX_UI_Focus = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Audio|UI")
	TObjectPtr<USoundBase> SFX_UI_Confirm = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Audio|UI")
	TObjectPtr<USoundBase> SFX_UI_Cancel = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Audio|UI")
	TObjectPtr<USoundBase> SFX_UI_Error = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Audio|UI")
	TObjectPtr<USoundBase> SFX_UI_MatchSuccess = nullptr;

	/** 로비 배경음. 컨트롤러 수명과 함께 시작·정지하므로 레이드로 떠나면 자동으로 멈춘다. */
	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Audio|BGM")
	TObjectPtr<USoundBase> BGM_Lobby = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Audio|BGM", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float BGMVolumeMultiplier = 1.0f;

	// 소유 클라이언트에서만 2D로 재생한다. `Sound`가 nullptr이면 조용히 건너뛴다.
	void PlayUISound(USoundBase* Sound) const;

private:
	UPROPERTY()
	TObjectPtr<URaidLobbyWidget> ActiveLobbyWidget;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> BGMAudioComponent = nullptr;
};
