#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "RaidAssignmentBase.h"
#include "RaidSessionSubsystem.generated.h"

UENUM(BlueprintType)
enum class ERaidEntryFailReason : uint8
{
	ServerListFailed,
	NoServerAvailable,
	MapLoadFailed,
	SpawnFailed,
	Cancelled,
};

UCLASS()
class DRONEPROTO_API URaidSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable, Category="Raid")
	void RequestRaidEntry(const FString& SlotId);

	UFUNCTION(BlueprintPure, Category="Raid")
	bool IsSlotEnabled(const FString& SlotId) const;

	// --- 팝업 제어 ---

	UFUNCTION(BlueprintCallable, Category="Raid|Popups")
	void ShowMatchmakingWait();

	UFUNCTION(BlueprintCallable, Category="Raid|Popups")
	void ShowNoServer();

	UFUNCTION(BlueprintCallable, Category="Raid|Popups")
	void ShowLoadFailed();

	UFUNCTION(BlueprintCallable, Category="Raid|Popups")
	void HideEntryPopups();

	// 팝업 닫고 LobbyMap으로 복귀 — MatchmakingWait 취소 버튼 + NoServer 확인 버튼 공용
	UFUNCTION(BlueprintCallable, Category="Raid|Popups")
	void CancelMatchmaking();

	// 에디터(BP 서브시스템 서브클래스)에서 WBP 클래스 지정
	UPROPERTY(EditDefaultsOnly, Category="Raid|Popups")
	TSubclassOf<UUserWidget> MatchmakingWaitWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category="Raid|Popups")
	TSubclassOf<UUserWidget> NoServerWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category="Raid|Popups")
	TSubclassOf<UUserWidget> LoadFailedWidgetClass;

private:
	// D11 교체 지점: 멀티 인스턴스 배정 전략으로 바꿀 때 이 한 줄만 수정
	UPROPERTY()
	TObjectPtr<URaidAssignmentBase> Assignment;

	UPROPERTY()
	TObjectPtr<UUserWidget> ActiveMatchmakingWidget;

	UPROPERTY()
	TObjectPtr<UUserWidget> ActiveNoServerWidget;

	UPROPERTY()
	TObjectPtr<UUserWidget> ActiveLoadFailedWidget;

	UUserWidget* CreateAndShowPopup(TSubclassOf<UUserWidget> WidgetClass);
};
