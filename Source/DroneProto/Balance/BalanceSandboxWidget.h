#pragma once

#include "CoreMinimal.h"
#include "Balance/BalanceSandboxStatus.h"
#include "Blueprint/UserWidget.h"
#include "BalanceSandboxWidget.generated.h"

class UButton;
class UComboBoxString;
class UTextBlock;
class ABalanceSandboxGameMode;

/**
 * 밸런스 샌드박스 조작 UI의 C++ 부모.
 *
 * 판정과 계산은 하나도 하지 않는다. 세 슬롯의 선택값을 문자열로 들고 있다가
 * `ABalanceSandboxGameMode`의 기존 서버 함수로 넘기기만 한다 — 콘솔 명령과 정확히 같은 경로다.
 *
 * 위젯 바인딩은 전부 `BindWidgetOptional`이라 기획자가 필요한 것만 배치해도 동작한다.
 * 콤보 박스를 두지 않고 직접 만든 버튼을 쓰고 싶으면 `SetCoreSelection` 계열을 호출하면 된다.
 *
 * 프로덕션 Raid UI와 `ARaidPlayerController`에는 노출하지 않는다. 이 위젯은 BalanceMap의
 * 샌드박스 GameMode 아래에서만 의미가 있다.
 */
UCLASS(Blueprintable, BlueprintType)
class DRONEPROTO_API UBalanceSandboxWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 코어 후보: None / Zenith / Booster / Drain */
	UFUNCTION(BlueprintPure, Category = "Balance Sandbox")
	static TArray<FString> GetCoreOptions();

	/** 무기 후보: None / Pulse / Fracture / Vector */
	UFUNCTION(BlueprintPure, Category = "Balance Sandbox")
	static TArray<FString> GetWeaponOptions();

	UFUNCTION(BlueprintCallable, Category = "Balance Sandbox")
	void SetCoreSelection(const FString& CoreAlias);

	UFUNCTION(BlueprintCallable, Category = "Balance Sandbox")
	void SetLeftWeaponSelection(const FString& WeaponAlias);

	UFUNCTION(BlueprintCallable, Category = "Balance Sandbox")
	void SetRightWeaponSelection(const FString& WeaponAlias);

	UFUNCTION(BlueprintPure, Category = "Balance Sandbox")
	FString GetCoreSelection() const { return CoreSelection; }

	UFUNCTION(BlueprintPure, Category = "Balance Sandbox")
	FString GetLeftWeaponSelection() const { return LeftWeaponSelection; }

	UFUNCTION(BlueprintPure, Category = "Balance Sandbox")
	FString GetRightWeaponSelection() const { return RightWeaponSelection; }

	/** 현재 선택값 3개를 기존 선택 경로로 보낸다. */
	UFUNCTION(BlueprintCallable, Category = "Balance Sandbox")
	bool ApplyLoadout();

	UFUNCTION(BlueprintCallable, Category = "Balance Sandbox")
	bool StartBattle();

	UFUNCTION(BlueprintCallable, Category = "Balance Sandbox")
	bool RunCorruptedPattern();

	UFUNCTION(BlueprintCallable, Category = "Balance Sandbox")
	bool RunStellarPattern();

	UFUNCTION(BlueprintCallable, Category = "Balance Sandbox")
	bool CreateDroneReport();

	UFUNCTION(BlueprintCallable, Category = "Balance Sandbox")
	bool ResetSandbox();

	/** 콤보 박스 후보 채우기와 상태 표시를 다시 맞춘다. */
	UFUNCTION(BlueprintCallable, Category = "Balance Sandbox")
	void RefreshSandboxUI();

	/**
	 * 현재 전투 상태를 한 번에 읽어 온다. 저장하지 않고 호출할 때마다 드론·보스·GameState·재고에서
	 * 조립한다. 새 계산은 하지 않으며 마지막 공격 분해값은 공격 경로가 기록해 둔 것을 그대로 쓴다.
	 */
	UFUNCTION(BlueprintPure, Category = "Balance Sandbox|Status")
	FBalanceSandboxStatus GetSandboxStatus() const;

	/** 상태 패널 텍스트를 현재 값으로 갱신한다. 배치한 TextBlock만 채운다. */
	UFUNCTION(BlueprintCallable, Category = "Balance Sandbox|Status")
	void RefreshStatusPanel();

	/** 패널 자동 갱신 주기(초). 0 이하면 자동 갱신하지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balance Sandbox|Status")
	float StatusRefreshIntervalSeconds = 0.25f;

	UFUNCTION(BlueprintPure, Category = "Balance Sandbox")
	ABalanceSandboxGameMode* GetSandboxGameMode() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> ComboBox_Core = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> ComboBox_LeftWeapon = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> ComboBox_RightWeapon = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_ApplyLoadout = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_StartBattle = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Corrupted = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Stellar = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Report = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Reset = nullptr;

	/** 마지막 조작 결과 한 줄. 없어도 동작한다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_SandboxStatus = nullptr;

	// 상태 패널. 전부 선택 바인딩이라 필요한 것만 배치해도 된다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Loadout = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Player = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_LastAttack = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_WeaponCoreState = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Raid = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Record = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Stock = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_DataSource = nullptr;

private:
	UFUNCTION()
	void HandleApplyLoadoutClicked();

	UFUNCTION()
	void HandleStartBattleClicked();

	UFUNCTION()
	void HandleCorruptedClicked();

	UFUNCTION()
	void HandleStellarClicked();

	UFUNCTION()
	void HandleReportClicked();

	UFUNCTION()
	void HandleResetClicked();

	UFUNCTION()
	void HandleCoreSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandleLeftWeaponSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandleRightWeaponSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	void BindSandboxWidgetEvents();
	void UnbindSandboxWidgetEvents();
	void PopulateComboBox(UComboBoxString* ComboBox, const TArray<FString>& Options, const FString& CurrentSelection);
	void SetStatusText(const FString& Status);
	void StartStatusRefreshTimer();
	void StopStatusRefreshTimer();

	FTimerHandle StatusRefreshTimerHandle;

	UPROPERTY()
	FString CoreSelection = TEXT("None");

	UPROPERTY()
	FString LeftWeaponSelection = TEXT("None");

	UPROPERTY()
	FString RightWeaponSelection = TEXT("None");
};
