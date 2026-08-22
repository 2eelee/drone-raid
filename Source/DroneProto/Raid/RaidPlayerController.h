#pragma once

#include "CoreMinimal.h"
#include "DroneCombatTypes.h"
#include "DronePart.h"
#include "DronePartReturnManager.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "RaidPlayerController.generated.h"

class ADronePartInventory;
class ADrone;
class ARaidBoss;
class UBossHUDWidget;
class UDroneReportWidget;
class UDataTable;
class UTexture2D;
class UDronePartReturnManager;
class USoundBase;
class UAudioComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnPartSelectionResult, EPartSlot, Slot, FName, PartID, bool, bSuccess, FString, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSelectedPartsChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPartSelectUIRefreshRequested);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPartSelectionServerError);

UENUM(BlueprintType)
enum class EPlayerSelectionState : uint8
{
	Selecting UMETA(DisplayName = "Selecting"),
	Locked    UMETA(DisplayName = "Locked"),
	InBattle  UMETA(DisplayName = "InBattle"),
};

UCLASS()
class DRONEPROTO_API ARaidPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ARaidPlayerController();

	UPROPERTY(BlueprintAssignable, Category = "Drone Parts")
	FOnPartSelectionResult OnPartSelectionResult;

	UPROPERTY(BlueprintAssignable, Category = "Drone Parts")
	FOnSelectedPartsChanged OnSelectedPartsChanged;

	UPROPERTY(BlueprintAssignable, Category = "UI")
	FOnPartSelectUIRefreshRequested OnPartSelectUIRefreshRequested;

	UPROPERTY(BlueprintAssignable, Category = "Drone Parts")
	FOnPartSelectionServerError OnPartSelectionServerError;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UUserWidget> DronePartSelectWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UDroneReportWidget> DroneReportWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UBossHUDWidget> BossHUDWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Drone|Report|Data", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDataTable> DroneReportBonusDataTable = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Drone|Report|Data", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDataTable> DroneReportSettingsDataTable = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Drone|Report|Data", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDataTable> DroneReportGradeDataTable = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
	bool bAutoShowDronePartSelectUI = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
	TMap<FName, TObjectPtr<UTexture2D>> PartIconOverrides;

	UFUNCTION(BlueprintCallable, Category = "Drone Parts")
	void RequestSelectPartFromUI(EDronePartSlot Slot, FName PartID);

	UFUNCTION(BlueprintCallable, Category = "Drone Parts")
	void RequestCancelPartFromUI(EDronePartSlot Slot);

	UFUNCTION(BlueprintCallable, Category = "Drone Parts")
	void RequestReadyForRaidFromUI();

	UFUNCTION(BlueprintCallable, Category = "Raid|Test")
	void RequestApplyTestDamageToDrone(int32 DamageAmount);

	UFUNCTION(BlueprintCallable, Category = "Raid|Test")
	void RequestRaidEndReturnTest(FName Reason);

	// PIE/debug-only bridge. Damage authority stays in ARaidBoss; clients only request a server-side debug trigger.
	UFUNCTION(BlueprintCallable, Exec, Category = "Raid|Boss|Debug")
	void DebugTriggerBossTelegraphAttack(float RadiusCm = -1.0f, int32 DamageAmount = -1, float TelegraphSeconds = -1.0f, float ForwardOffsetCm = -1.0f);

	UFUNCTION(BlueprintPure, Category = "Drone Parts")
	FName GetSelectedCorePartID() const;

	UFUNCTION(BlueprintPure, Category = "Drone Parts")
	FName GetSelectedLeftWeaponPartID() const;

	UFUNCTION(BlueprintPure, Category = "Drone Parts")
	FName GetSelectedRightWeaponPartID() const;

	UFUNCTION(BlueprintPure, Category = "Drone Parts")
	FName GetSelectedPartIDBySlot(EPartSlot Slot) const;

	UFUNCTION(BlueprintPure, Category = "Drone Parts")
	FName GetSelectedPartForSlot(EDronePartSlot Slot) const;

	UFUNCTION(BlueprintPure, Category = "Drone Parts")
	bool HasSelectedPartForSlot(EDronePartSlot Slot) const;

	UFUNCTION(BlueprintPure, Category = "Drone Parts")
	FName GetEquippedPartIDBySlot(EPartSlot Slot) const;

	UFUNCTION(BlueprintPure, Category = "Drone Parts")
	EPlayerSelectionState GetPlayerSelectionState() const;

	UFUNCTION(BlueprintPure, Category = "Drone Parts")
	EPlayerSelectionState GetCurrentSelectionState() const;

	UFUNCTION(BlueprintPure, Category = "Drone Parts")
	bool IsSelectionLocked() const;

	UFUNCTION(BlueprintPure, Category = "Drone Parts")
	float GetSelectionRemainingTime() const;

	UFUNCTION(BlueprintPure, Category = "Drone Parts")
	float GetSelectionEndServerTime() const;

	UFUNCTION(BlueprintPure, Category = "Drone Parts")
	TArray<FName> GetAvailablePartIDsForSlot(EDronePartSlot Slot) const;

	UFUNCTION(BlueprintPure, Category = "Drone Parts")
	TArray<FName> GetCorePartIDs() const;

	UFUNCTION(BlueprintPure, Category = "Drone Parts")
	TArray<FName> GetWeaponPartIDs() const;

	UFUNCTION(BlueprintPure, Category = "UI")
	FText GetPartDisplayName(FName PartID) const;

	UFUNCTION(BlueprintPure, Category = "UI")
	FText GetPartDescription(FName PartID) const;

	UFUNCTION(BlueprintPure, Category = "UI")
	UTexture2D* GetPartIcon(FName PartID) const;

	UFUNCTION(BlueprintPure, Category = "UI")
	int32 GetPartCurrentCount(FName PartID) const;

	UFUNCTION(BlueprintPure, Category = "UI")
	int32 GetPartMaxCount(FName PartID) const;

	static FString BuildStableControllerLogString(const AController* Controller);
	static const TCHAR* SelectionStateToLogString(EPlayerSelectionState State);

	UFUNCTION(BlueprintPure, Category = "Drone Parts")
	ADronePartInventory* GetDronePartInventory() const;

	UFUNCTION(BlueprintPure, Category = "Drone Parts")
	UDronePartReturnManager* GetDronePartReturnManager() const;

	UFUNCTION(BlueprintCallable, Category = "UI")
	bool RefreshDronePartInventoryBinding();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void ShowDronePartSelectUI();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void HideDronePartSelectUI();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void ShowDroneReportWidget(const FDroneReportData& ReportData);

	UFUNCTION(BlueprintCallable, Category = "UI")
	void HideDroneReportWidget();

	/**
	 * DroneReport 확인 버튼을 소유 컨트롤러가 먼저 처리할 기회.
	 *
	 * 기본 구현은 아무것도 하지 않고 false를 돌려, 위젯이 원문 계약대로 LobbyMap으로 이동하게 둔다.
	 * true를 돌리면 위젯은 이동을 건너뛴다 — 밸런스 샌드박스만 그 경로를 쓴다.
	 * 서버의 리포트 생성·산식·저장·중복 방지와는 무관하다. 표시 종료 처리만 가른다.
	 */
	virtual bool TryHandleDroneReportConfirmedForLocalPlayer(UDroneReportWidget* ReportWidget);

	UFUNCTION(BlueprintCallable, Category = "UI")
	void ShowBossHUDForLocalPlayer();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void HideBossHUDForLocalPlayer();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void RefreshBossHUDForLocalPlayer();

	UFUNCTION(BlueprintPure, Category = "UI")
	bool IsBossHUDVisibleForLocalPlayer() const;

	UFUNCTION(BlueprintCallable, Category = "UI")
	void RefreshSelectionUI();

	UFUNCTION(BlueprintCallable, Category = "Raid")
	void HandleRaidLoadFailedForClient(FName Reason, FName TargetMap);

	UFUNCTION(BlueprintImplementableEvent, Category = "Raid|UI")
	void BP_OnRaidLoadFailed(FName Reason, FName TargetMap);

	void SetSelectedPartIDForSlotForServer(EPartSlot Slot, FName PartID);
	void SetEquippedPartIDForSlotForServer(EPartSlot Slot, FName PartID);
	bool ReturnSelectedPartsForServer(EDronePartReturnReason Reason);
	bool ReturnEquippedPartsForServer(EDronePartReturnReason Reason);

	// 이 컨트롤러를 선택 단계로 되돌린다 — 보유 부품을 기존 반환 경로로 되돌리고, 드론을
	// 전투 이전 상태로 복구하고, 리포트 1회 제한을 풀고, 선택 타이머를 다시 건다.
	// 재고 변경은 전부 기존 반환 매니저를 그대로 탄다. 밸런스 반복 시험용 진입점이며
	// 프로덕션 입장·전투 경로에서는 호출하지 않는다.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Raid")
	bool RestartSelectionPhaseForServer(FName Reason);

	// DroneReport 밸런스 표 3종이 모두 로드됐는지. 밸런스 상태 패널 조회용이다.
	UFUNCTION(BlueprintPure, Category = "Raid|Report")
	bool AreDroneReportDataTablesLoaded() const;
	bool ReturnSingleSelectedPartForServer(EPartSlot Slot, EDronePartReturnReason Reason);
	bool ReturnSingleEquippedPartForServer(EPartSlot Slot, EDronePartReturnReason Reason);
	void FinalizeRaidEndForServer(FName Reason);
	void HandleSelectionTimerExpiredForServer();
	bool TryCreateDroneReportForServer(EDroneReportTrigger Trigger, bool bBossDefeated);

	UFUNCTION(BlueprintPure, Category = "Drone|Report")
	bool HasDroneReportGenerated() const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Raid|Targeting")
	bool AssignBossTargetForServer();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Raid|Targeting")
	void ClearBossTargetForServer(FName Reason);

	UFUNCTION(BlueprintPure, Category = "Raid|Targeting")
	ARaidBoss* GetCurrentTargetBoss() const;

	UFUNCTION(BlueprintPure, Category = "Raid|Targeting")
	bool IsTargetLocked() const;

	bool HasValidBossTargetForServer() const;
	bool HasValidBossTargetForServer(FName& OutInvalidReason) const;

	// Boss EndPlay/Destroy(pending kill) 중에는 안전 getter가 nullptr를 돌려주므로 raw 비교가 필요하다.
	bool IsTargetingBossForServer(const ARaidBoss* Boss) const;

	UFUNCTION(BlueprintPure, Category = "Raid|Targeting")
	bool HasValidBossTarget() const;

	UFUNCTION(BlueprintPure, Category = "Raid|Targeting")
	FVector GetTargetMarkerWorldLocation() const;

	UFUNCTION(BlueprintCallable, Category = "Raid|Targeting")
	void RefreshTargetMarkerUI();

	UFUNCTION(BlueprintNativeEvent, Category = "Raid|Targeting")
	void BP_OnTargetMarkerChanged(bool bVisible, ARaidBoss* TargetBoss);
#if WITH_DEV_AUTOMATION_TESTS
	void SetDronePartReturnManagerForTest(UDronePartReturnManager* InReturnManager);
	FDroneReportData GetLastDroneReportDataForTest() const;
	bool HasDroneReportGeneratedForTest() const;
	void ResetDroneReportForTest();
	void SetDroneReportDataTablesForTest(UDataTable* BonusTable, UDataTable* SettingsTable, UDataTable* GradeTable);
	int32 GetTargetMarkerChangedCountForTest() const;
	bool WasLastTargetMarkerVisibleForTest() const;
	ARaidBoss* GetLastTargetMarkerBossForTest() const;
	void SetSuppressRaidLoadFailedLobbyTravelForTest(bool bInSuppressTravel) { bSuppressRaidLoadFailedLobbyTravelForTest = bInSuppressTravel; }
	int32 GetRaidLoadFailedReturnToLobbyCountForTest() const { return RaidLoadFailedReturnToLobbyCountForTest; }
	FName GetLastRaidLoadFailedReasonForTest() const { return LastRaidLoadFailedReason; }
	FName GetLastRaidLoadFailedTargetMapForTest() const { return LastRaidLoadFailedTargetMap; }
#endif

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Drone Parts")
	void Server_RequestSelectPart(EPartSlot Slot, FName NewPartID);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Drone Parts")
	void Server_RequestCancelPart(EPartSlot Slot);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Raid")
	void Server_RequestReadyForRaid();

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Raid")
	void Server_RequestStartSelectionTimer();

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Raid|Test")
	void Server_RequestApplyTestDamageToDrone(int32 DamageAmount);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Raid|Test")
	void Server_RequestRaidEndReturnTest(FName Reason);

	UFUNCTION(Server, Reliable, Category = "Raid|Boss|Debug")
	void Server_DebugTriggerBossTelegraphAttack(float RadiusCm, int32 DamageAmount, float TelegraphSeconds, float ForwardOffsetCm);

	UFUNCTION(Server, Reliable, Category = "Raid|Boss|Debug")
	void Server_DebugSetBossStunned(bool bStunned);

	UFUNCTION(Client, Reliable, Category = "Drone Parts")
	void Client_NotifyPartSelectionResult(EPartSlot Slot, FName PartID, bool bSuccess, const FString& Reason);

	UFUNCTION(Client, Reliable, Category = "Drone Parts")
	void Client_RestorePartSelectionAfterServerError(
		FName AuthoritativeCorePartID,
		FName AuthoritativeLeftWeaponPartID,
		FName AuthoritativeRightWeaponPartID);

	UFUNCTION(Client, Reliable, Category = "Raid")
	void Client_NotifyRaidReadyResult(bool bSuccess, const FString& Reason, FName CorePartID, FName LeftWeaponPartID, FName RightWeaponPartID);

	UFUNCTION(Client, Reliable, Category = "Drone|Report")
	void Client_ReceiveDroneReport(const FDroneReportData& ReportData);

	UFUNCTION(Client, Reliable, Category = "Raid")
	void Client_NotifyRaidEndedForUI(FName Reason);

	UFUNCTION(Client, Reliable, Category = "Raid")
	void Client_NotifyRaidLoadFailed(FName Reason, FName TargetMap);

	// ---- UI SFX / BGM ----
	// 대부분은 아래 `Client_Notify*` 경로가 자동으로 낸다. UMG에서 따로 부를 필요가 없다.
	// 포커스 이동과 취소만 순수 UI 입력이라 위젯이 직접 불러야 한다.

	/** 부품·메뉴 포커스가 실제로 바뀐 순간 UMG에서 부른다. 단순 마우스 이동에는 부르지 않는다. */
	UFUNCTION(BlueprintCallable, Category = "Raid|Audio")
	void PlayUIFocusSound();

	/** 취소·뒤로 입력에서 UMG가 부른다. */
	UFUNCTION(BlueprintCallable, Category = "Raid|Audio")
	void PlayUICancelSound();

	/** 팝업 확인처럼 서버 왕복이 없는 확정에서 UMG가 부른다. 부품 선택 확정은 자동으로 난다. */
	UFUNCTION(BlueprintCallable, Category = "Raid|Audio")
	void PlayUIConfirmSound();

	/** 사용 불가·입력 실패에서 UMG가 부른다. 서버가 거부한 선택은 자동으로 난다. */
	UFUNCTION(BlueprintCallable, Category = "Raid|Audio")
	void PlayUIErrorSound();

	UFUNCTION(Exec)
	void D4SelectPart(FString SlotName, FString PartIDText);

	UFUNCTION(Exec)
	void D4CancelPart(FString SlotName);

	UFUNCTION(Exec)
	void D6KillDrone();

	UFUNCTION(Exec)
	void D6RaidEndReturn(FString ReasonText);

	// PIE 수동 검증용: DebugBossSetStunned 1 / DebugBossSetStunned 0
	UFUNCTION(Exec)
	void DebugBossSetStunned(int32 Stunned);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ---- UI SFX / BGM 슬롯 ----
	// 전부 2D Local이다. 소유 클라이언트에서만 재생되며 서버는 아무것도 하지 않는다.

	/** 포커스 이동. */
	UPROPERTY(EditDefaultsOnly, Category = "Raid|Audio|UI")
	TObjectPtr<USoundBase> SFX_UI_Focus = nullptr;

	/** 확정·OK. 부품 선택이 서버에서 수락된 순간 자동으로 난다. */
	UPROPERTY(EditDefaultsOnly, Category = "Raid|Audio|UI")
	TObjectPtr<USoundBase> SFX_UI_Confirm = nullptr;

	/** 취소·뒤로. */
	UPROPERTY(EditDefaultsOnly, Category = "Raid|Audio|UI")
	TObjectPtr<USoundBase> SFX_UI_Cancel = nullptr;

	/** 실패·사용 불가. 부품 수량 0, 준비 거부, 레이드 로드 실패에서 자동으로 난다. */
	UPROPERTY(EditDefaultsOnly, Category = "Raid|Audio|UI")
	TObjectPtr<USoundBase> SFX_UI_Error = nullptr;

	/** 선택 카운트다운. 남은 5초 동안 1초마다 1회. */
	UPROPERTY(EditDefaultsOnly, Category = "Raid|Audio|UI")
	TObjectPtr<USoundBase> SFX_UI_CountdownTick = nullptr;

	/** 레이드 입장 확정. 준비가 서버에서 수락된 순간 1회. */
	UPROPERTY(EditDefaultsOnly, Category = "Raid|Audio|UI")
	TObjectPtr<USoundBase> SFX_UI_MatchSuccess = nullptr;

	/** DRONE REPORT가 화면에 열린 순간 1회. */
	UPROPERTY(EditDefaultsOnly, Category = "Raid|Audio|UI")
	TObjectPtr<USoundBase> SFX_UI_DroneReport_Open = nullptr;

	/** 레이드 배경음. 컨트롤러 수명과 함께 시작·정지하므로 맵을 벗어나면 자동으로 멈춘다. */
	UPROPERTY(EditDefaultsOnly, Category = "Raid|Audio|BGM")
	TObjectPtr<USoundBase> BGM_Raid = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Raid|Audio|BGM", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float BGMVolumeMultiplier = 1.0f;

	// 소유 클라이언트에서만 2D로 재생한다. `Sound`가 nullptr이면 조용히 건너뛴다.
	void PlayUISound(USoundBase* Sound) const;

	void StartBGMLocally();
	void StopBGMLocally();

	// 남은 시간이 정수 초를 넘어설 때마다 1회. 마지막 5초에만 운다.
	void UpdateSelectionCountdownAudioLocally();

	// 선택 시간(15초) 종료 시 자동 확정 여부. 원문 (7)/3.(2)의 프로덕션 계약은 항상 true이므로
	// 여기서 값을 바꾸지 않는다. 시험자가 전투 시작 시점을 직접 잡아야 하는 밸런스 샌드박스만
	// false로 덮어 자동 확정 타이머를 걸지 않는다.
	virtual bool ShouldAutoConfirmSelectionForServer() const;

private:
	static constexpr float SelectionDurationSeconds = 15.0f;

	// 카운트다운이 우는 마지막 구간. 가이드가 "마지막 5초에 1초마다"를 권장한다.
	static constexpr int32 SelectionCountdownAudioStartSecond = 5;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> BGMAudioComponent = nullptr;

	// 마지막으로 운 남은 초. 같은 초에 두 번 울지 않게 하는 잠금이다.
	int32 LastPlayedCountdownSecond = -1;

	UPROPERTY(ReplicatedUsing = OnRep_PlayerSelectionState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Drone Parts", meta = (AllowPrivateAccess = "true"))
	EPlayerSelectionState PlayerSelectionState = EPlayerSelectionState::Selecting;

	UPROPERTY(ReplicatedUsing = OnRep_SelectionEndServerTime, VisibleInstanceOnly, BlueprintReadOnly, Category = "Drone Parts", meta = (AllowPrivateAccess = "true"))
	float SelectionEndServerTime = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentTargetBoss, VisibleInstanceOnly, BlueprintReadOnly, Category = "Raid|Targeting", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ARaidBoss> CurrentTargetBoss = nullptr;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Raid|Targeting", meta = (AllowPrivateAccess = "true"))
	bool bIsTargetLocked = false;

	UPROPERTY(VisibleInstanceOnly, Category = "Drone Parts")
	FName SelectedCorePartID = NAME_None;

	UPROPERTY(VisibleInstanceOnly, Category = "Drone Parts")
	FName SelectedLeftWeaponPartID = NAME_None;

	UPROPERTY(VisibleInstanceOnly, Category = "Drone Parts")
	FName SelectedRightWeaponPartID = NAME_None;

	UPROPERTY(VisibleInstanceOnly, Category = "Drone Parts")
	FName EquippedCorePartID = NAME_None;

	UPROPERTY(VisibleInstanceOnly, Category = "Drone Parts")
	FName EquippedLeftWeaponPartID = NAME_None;

	UPROPERTY(VisibleInstanceOnly, Category = "Drone Parts")
	FName EquippedRightWeaponPartID = NAME_None;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> DronePartSelectWidget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UDroneReportWidget> CurrentDroneReportWidget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UBossHUDWidget> BossHUDWidget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ADronePartInventory> BoundDronePartInventory = nullptr;

	UPROPERTY(Transient)
	bool bDroneReportGenerated = false;

	UPROPERTY(Transient)
	FDroneReportData LastDroneReportData;

	FDroneReportResolvedConfig CachedDroneReportConfig;
	bool bDroneReportConfigResolved = false;
	bool bDroneReportConfigUsesDataTables = false;
	FString DroneReportConfigFallbackReason;

	UPROPERTY(Transient)
	bool bRaidLoadFailedReturnToLobbyRequested = false;

	UPROPERTY(Transient)
	FName LastRaidLoadFailedReason = NAME_None;

	UPROPERTY(Transient)
	FName LastRaidLoadFailedTargetMap = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Raid|Boss|Debug", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
	float DebugBossTelegraphRadiusCm = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Raid|Boss|Debug", meta = (AllowPrivateAccess = "true", ClampMin = "0"))
	int32 DebugBossTelegraphDamageAmount = 25;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Raid|Boss|Debug", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "s"))
	float DebugBossTelegraphDelaySeconds = 1.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Raid|Boss|Debug", meta = (AllowPrivateAccess = "true", Units = "cm"))
	float DebugBossTelegraphForwardOffsetCm = 0.0f;

#if WITH_DEV_AUTOMATION_TESTS
	int32 TargetMarkerChangedCountForTest = 0;
	bool bLastTargetMarkerVisibleForTest = false;
	TWeakObjectPtr<ARaidBoss> LastTargetMarkerBossForTest;
	bool bSuppressRaidLoadFailedLobbyTravelForTest = false;
	int32 RaidLoadFailedReturnToLobbyCountForTest = 0;
#endif
#if WITH_DEV_AUTOMATION_TESTS
	UDronePartReturnManager* TestDronePartReturnManager = nullptr;
#endif

	FTimerHandle SelectionTimerHandle;

	FName* GetSelectedPartIDForSlot(EPartSlot Slot);
	const FName* GetSelectedPartIDForSlot(EPartSlot Slot) const;
	FName* GetEquippedPartIDForSlot(EPartSlot Slot);
	const FName* GetEquippedPartIDForSlot(EPartSlot Slot) const;
	void SetSelectedPartIDForSlot(EPartSlot Slot, FName PartID);
	void SetEquippedPartIDForSlot(EPartSlot Slot, FName PartID);
	bool IsPartTypeAllowedForSlot(EPartSlot Slot, EDronePartType PartType) const;
	bool TryParsePartSlot(const FString& SlotName, EPartSlot& OutSlot) const;
	bool ValidateSelectedLoadoutForServer(FString& OutReason) const;
	void MoveSelectedPartsToEquippedForServer();
	void SetPlayerSelectionStateForServer(EPlayerSelectionState NewState);
	float GetSelectionServerTimeSeconds() const;
	void StartSelectionTimerForServer();
	void StopSelectionTimerForServer(const FString& Reason, bool bLogSummary);
	bool ProcessReadyForRaidForServer(bool bAutoReady);
	void HandleDebugTriggerBossTelegraphAttackForServer(float RadiusCm, int32 DamageAmount, float TelegraphSeconds, float ForwardOffsetCm);
	void HandleDebugSetBossStunnedForServer(bool bStunned);
	void ReturnToLobbyForRaidLoadFailure(FName Reason, FName TargetMap);
	static const TCHAR* ReportGradeToLogString(EDroneReportGrade Grade);
	static const TCHAR* ReportTriggerToLogString(EDroneReportTrigger Trigger);
	static bool ReportHasBonus(const FDroneReportData& ReportData, EDroneReportBonusType BonusType);
	const FDroneReportResolvedConfig& ResolveDroneReportConfigForServer();

	UFUNCTION()
	void HandleDronePartStocksChanged();

	UFUNCTION()
	void OnRep_PlayerSelectionState();

	UFUNCTION()
	void OnRep_SelectionEndServerTime();

	UFUNCTION()
	void OnRep_CurrentTargetBoss();
};
