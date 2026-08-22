#include "RaidPlayerController.h"
#include "BalanceTelemetryComponent.h"
#include "BossHUDWidget.h"
#include "Drone.h"
#include "DronePartInventory.h"
#include "DroneReportDataTableResolver.h"
#include "DroneReportWidget.h"
#include "Lobby/RaidSessionSubsystem.h"
#include "RaidBoss.h"
#include "RaidGameMode.h"
#include "RaidGameState.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Components/AudioComponent.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
constexpr const TCHAR* RaidLoadFailureLobbyMapName = TEXT("LobbyMap");

const TCHAR* ToSelectionSlotLogString(EPartSlot Slot)
{
	switch (Slot)
	{
	case EPartSlot::Core:
		return TEXT("Core");
	case EPartSlot::LeftWeapon:
		return TEXT("LeftWeapon");
	case EPartSlot::RightWeapon:
		return TEXT("RightWeapon");
	default:
		return TEXT("Unknown");
	}
}

const TCHAR* ToPlayerSelectionStateLogString(EPlayerSelectionState State)
{
	return ARaidPlayerController::SelectionStateToLogString(State);
}

const TCHAR* ToRaidStateLogStringForPlayerController(ERaidState State)
{
	switch (State)
	{
	case ERaidState::Waiting:
		return TEXT("Waiting");
	case ERaidState::Drafting:
		return TEXT("Drafting");
	case ERaidState::Battle:
		return TEXT("Battle");
	case ERaidState::End:
		return TEXT("End");
	default:
		return TEXT("Unknown");
	}
}

FString GetRaidStateLogString(const APlayerController* PC)
{
	const UWorld* World = PC ? PC->GetWorld() : nullptr;
	const ARaidGameState* RaidGameState = World ? World->GetGameState<ARaidGameState>() : nullptr;
	return RaidGameState ? ToRaidStateLogStringForPlayerController(RaidGameState->RaidState) : TEXT("None");
}

const TCHAR* ToNetModeLogString(ENetMode NetMode)
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

const TCHAR* ToRaidPCNetRoleLogString(ENetRole Role)
{
	switch (Role)
	{
	case ROLE_None:
		return TEXT("None");
	case ROLE_SimulatedProxy:
		return TEXT("SimulatedProxy");
	case ROLE_AutonomousProxy:
		return TEXT("AutonomousProxy");
	case ROLE_Authority:
		return TEXT("Authority");
	default:
		return TEXT("Unknown");
	}
}

FString BuildControllerLogString(const APlayerController* PC)
{
	return ARaidPlayerController::BuildStableControllerLogString(PC);
}

FString BuildInventoryLookupDebugString(const ADronePartInventory* Inventory)
{
	if (!Inventory)
	{
		return TEXT("Inventory=None");
	}

	const FString LocalRoleString = ToRaidPCNetRoleLogString(Inventory->GetLocalRole());
	const FString RemoteRoleString = ToRaidPCNetRoleLogString(Inventory->GetRemoteRole());

	return FString::Printf(
		TEXT("Inventory=%s HasAuthority=%s LocalRole=%s RemoteRole=%s Replicates=%s AlwaysRelevant=%s Dormancy=%d"),
		*Inventory->GetName(),
		Inventory->HasAuthority() ? TEXT("true") : TEXT("false"),
		*LocalRoleString,
		*RemoteRoleString,
		Inventory->GetIsReplicated() ? TEXT("true") : TEXT("false"),
		Inventory->bAlwaysRelevant ? TEXT("true") : TEXT("false"),
		static_cast<int32>(Inventory->NetDormancy));
}

EPartSlot ToInternalPartSlot(EDronePartSlot Slot)
{
	switch (Slot)
	{
	case EDronePartSlot::Core:
		return EPartSlot::Core;
	case EDronePartSlot::RightWeapon:
		return EPartSlot::RightWeapon;
	case EDronePartSlot::LeftWeapon:
		return EPartSlot::LeftWeapon;
	default:
		return EPartSlot::Core;
	}
}

FText GetFallbackPartDisplayName(FName PartID)
{
	if (PartID == ADronePartInventory::GetCoreZenithPartID())
	{
		return FText::FromString(TEXT("Zenith Core"));
	}
	if (PartID == ADronePartInventory::GetCoreBoosterPartID())
	{
		return FText::FromString(TEXT("Booster Core"));
	}
	if (PartID == ADronePartInventory::GetCoreDrainPartID())
	{
		return FText::FromString(TEXT("Drain Core"));
	}
	if (PartID == ADronePartInventory::GetPulseLaserPartID())
	{
		return FText::FromString(TEXT("Pulse Laser"));
	}
	if (PartID == ADronePartInventory::GetFractureBurstPartID())
	{
		return FText::FromString(TEXT("Fracture Burst"));
	}
	if (PartID == ADronePartInventory::GetVectorCannonPartID())
	{
		return FText::FromString(TEXT("Vector Cannon"));
	}

	return PartID.IsNone() ? FText::GetEmpty() : FText::FromName(PartID);
}

FText GetFallbackPartDescription(FName PartID)
{
	if (PartID == ADronePartInventory::GetCoreZenithPartID())
	{
		return FText::FromString(TEXT("High-output core tuned for maximum raid durability."));
	}
	if (PartID == ADronePartInventory::GetCoreBoosterPartID())
	{
		return FText::FromString(TEXT("Balanced core with stable booster support."));
	}
	if (PartID == ADronePartInventory::GetCoreDrainPartID())
	{
		return FText::FromString(TEXT("Risk-oriented core that channels enemy pressure into power."));
	}
	if (PartID == ADronePartInventory::GetPulseLaserPartID())
	{
		return FText::FromString(TEXT("Reliable targeting weapon for steady boss damage."));
	}
	if (PartID == ADronePartInventory::GetFractureBurstPartID())
	{
		return FText::FromString(TEXT("Burst weapon built around short windows of amplified damage."));
	}
	if (PartID == ADronePartInventory::GetVectorCannonPartID())
	{
		return FText::FromString(TEXT("Directional cannon for precise ranged pressure."));
	}

	return PartID.IsNone() ? FText::GetEmpty() : FText::FromString(TEXT("No description registered."));
}
}

ARaidPlayerController::ARaidPlayerController()
{
	static ConstructorHelpers::FObjectFinder<UDataTable> BonusTableFinder(
		TEXT("/Game/Data/DroneReport/DT_DroneReportBonus.DT_DroneReportBonus"));
	static ConstructorHelpers::FObjectFinder<UDataTable> SettingsTableFinder(
		TEXT("/Game/Data/DroneReport/DT_DroneReportSettings.DT_DroneReportSettings"));
	static ConstructorHelpers::FObjectFinder<UDataTable> GradeTableFinder(
		TEXT("/Game/Data/DroneReport/DT_DroneReportGrade.DT_DroneReportGrade"));
	DroneReportBonusDataTable = BonusTableFinder.Object;
	DroneReportSettingsDataTable = SettingsTableFinder.Object;
	DroneReportGradeDataTable = GradeTableFinder.Object;
}

void ARaidPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// [확인3] 맵 로드 타임아웃 / 폰 Spawn 실패 (후순위, D11)
	//   단일 정상 환경에선 미발동. 구현 시 아래 패턴 사용:
	//   UGameInstance* GI = GetGameInstance();
	//   URaidSessionSubsystem* SS = GI ? GI->GetSubsystem<URaidSessionSubsystem>() : nullptr;
	//   if (LoadTimedOut || !GetPawn()) { if (SS) SS->ShowLoadFailed(); ReturnToLobby(); }   // TODO

	if (HasAuthority())
	{
		StartSelectionTimerForServer();
	}

	if (bAutoShowDronePartSelectUI)
	{
		GetWorldTimerManager().SetTimerForNextTick(this, &ARaidPlayerController::ShowDronePartSelectUI);
	}

	StartBGMLocally();
}

void ARaidPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	HideBossHUDForLocalPlayer();
	// 맵을 벗어나면 컨트롤러가 사라지므로 BGM도 함께 멎는다.
	StopBGMLocally();

	if (HasAuthority())
	{
		ClearBossTargetForServer(FName(TEXT("Travel")));
		StopSelectionTimerForServer(TEXT("EndPlay"), false);
	}

	Super::EndPlay(EndPlayReason);
}

void ARaidPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateSelectionCountdownAudioLocally();
}

void ARaidPlayerController::PlayUISound(USoundBase* Sound) const
{
	// 에셋 미지정은 정상 상태다. 배선이 끝나지 않은 소리를 로그로 시끄럽게 만들지 않는다.
	if (!Sound || GetNetMode() == NM_DedicatedServer || !IsLocalController())
	{
		return;
	}

	UGameplayStatics::PlaySound2D(this, Sound);
}

void ARaidPlayerController::PlayUIFocusSound()
{
	PlayUISound(SFX_UI_Focus);
}

void ARaidPlayerController::PlayUICancelSound()
{
	PlayUISound(SFX_UI_Cancel);
}

void ARaidPlayerController::PlayUIConfirmSound()
{
	PlayUISound(SFX_UI_Confirm);
}

void ARaidPlayerController::PlayUIErrorSound()
{
	PlayUISound(SFX_UI_Error);
}

void ARaidPlayerController::StartBGMLocally()
{
	if (!BGM_Raid || BGMAudioComponent || GetNetMode() == NM_DedicatedServer || !IsLocalController())
	{
		return;
	}

	// bAutoDestroy = false — 핸들을 들고 있어야 맵을 벗어날 때 끊을 수 있다.
	BGMAudioComponent = UGameplayStatics::SpawnSound2D(
		this, BGM_Raid, BGMVolumeMultiplier, 1.0f, 0.0f, nullptr, false, false);
}

void ARaidPlayerController::StopBGMLocally()
{
	if (BGMAudioComponent)
	{
		BGMAudioComponent->Stop();
		BGMAudioComponent = nullptr;
	}
}

void ARaidPlayerController::UpdateSelectionCountdownAudioLocally()
{
	if (!SFX_UI_CountdownTick || GetNetMode() == NM_DedicatedServer || !IsLocalController())
	{
		return;
	}

	// 선택 중이 아니면 다음 선택 페이즈를 위해 잠금을 푼다.
	if (GetCurrentSelectionState() != EPlayerSelectionState::Selecting)
	{
		LastPlayedCountdownSecond = -1;
		return;
	}

	const float RemainingSeconds = GetSelectionRemainingTime();
	if (RemainingSeconds <= 0.0f)
	{
		return;
	}

	// 5.0초 남았을 때 5, 4.2초면 5가 아니라 5(올림)다. 초가 바뀌는 순간마다 정확히 한 번 운다.
	const int32 RemainingSecondMark = FMath::CeilToInt(RemainingSeconds);
	if (RemainingSecondMark > SelectionCountdownAudioStartSecond
		|| RemainingSecondMark == LastPlayedCountdownSecond)
	{
		return;
	}

	LastPlayedCountdownSecond = RemainingSecondMark;
	PlayUISound(SFX_UI_CountdownTick);
}

void ARaidPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARaidPlayerController, PlayerSelectionState);
	DOREPLIFETIME_CONDITION(ARaidPlayerController, SelectionEndServerTime, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ARaidPlayerController, CurrentTargetBoss, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ARaidPlayerController, bIsTargetLocked, COND_OwnerOnly);
}

void ARaidPlayerController::RequestSelectPartFromUI(EDronePartSlot Slot, FName PartID)
{
	Server_RequestSelectPart(ToInternalPartSlot(Slot), PartID);
}

void ARaidPlayerController::RequestCancelPartFromUI(EDronePartSlot Slot)
{
	Server_RequestCancelPart(ToInternalPartSlot(Slot));
}

void ARaidPlayerController::RequestReadyForRaidFromUI()
{
	Server_RequestReadyForRaid();
}

void ARaidPlayerController::RequestApplyTestDamageToDrone(int32 DamageAmount)
{
	Server_RequestApplyTestDamageToDrone(DamageAmount);
}

void ARaidPlayerController::RequestRaidEndReturnTest(FName Reason)
{
	Server_RequestRaidEndReturnTest(Reason);
}

void ARaidPlayerController::DebugTriggerBossTelegraphAttack(float RadiusCm, int32 DamageAmount, float TelegraphSeconds, float ForwardOffsetCm)
{
	const float RequestedRadiusCm = RadiusCm < 0.0f ? DebugBossTelegraphRadiusCm : RadiusCm;
	const int32 RequestedDamageAmount = DamageAmount < 0 ? DebugBossTelegraphDamageAmount : DamageAmount;
	const float RequestedTelegraphSeconds = TelegraphSeconds < 0.0f ? DebugBossTelegraphDelaySeconds : TelegraphSeconds;
	const float RequestedForwardOffsetCm = ForwardOffsetCm < 0.0f ? DebugBossTelegraphForwardOffsetCm : ForwardOffsetCm;

	if (HasAuthority())
	{
		HandleDebugTriggerBossTelegraphAttackForServer(
			RequestedRadiusCm,
			RequestedDamageAmount,
			RequestedTelegraphSeconds,
			RequestedForwardOffsetCm);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossTelegraphDebugTrigger PC=%s Result=RequestServer Scope=DebugOnly Radius=%.2f Damage=%d Delay=%.2f ForwardOffset=%.2f"),
		*BuildControllerLogString(this),
		RequestedRadiusCm,
		RequestedDamageAmount,
		RequestedTelegraphSeconds,
		RequestedForwardOffsetCm);

	Server_DebugTriggerBossTelegraphAttack(
		RequestedRadiusCm,
		RequestedDamageAmount,
		RequestedTelegraphSeconds,
		RequestedForwardOffsetCm);
}

FName ARaidPlayerController::GetSelectedCorePartID() const
{
	return SelectedCorePartID;
}

FName ARaidPlayerController::GetSelectedLeftWeaponPartID() const
{
	return SelectedLeftWeaponPartID;
}

FName ARaidPlayerController::GetSelectedRightWeaponPartID() const
{
	return SelectedRightWeaponPartID;
}

FName ARaidPlayerController::GetSelectedPartIDBySlot(EPartSlot Slot) const
{
	if (const FName* SelectedPartID = GetSelectedPartIDForSlot(Slot))
	{
		return *SelectedPartID;
	}

	return NAME_None;
}

FName ARaidPlayerController::GetSelectedPartForSlot(EDronePartSlot Slot) const
{
	return GetSelectedPartIDBySlot(ToInternalPartSlot(Slot));
}

bool ARaidPlayerController::HasSelectedPartForSlot(EDronePartSlot Slot) const
{
	return !GetSelectedPartForSlot(Slot).IsNone();
}

FName ARaidPlayerController::GetEquippedPartIDBySlot(EPartSlot Slot) const
{
	if (const FName* EquippedPartID = GetEquippedPartIDForSlot(Slot))
	{
		return *EquippedPartID;
	}

	return NAME_None;
}

EPlayerSelectionState ARaidPlayerController::GetPlayerSelectionState() const
{
	return PlayerSelectionState;
}

EPlayerSelectionState ARaidPlayerController::GetCurrentSelectionState() const
{
	return PlayerSelectionState;
}

bool ARaidPlayerController::IsSelectionLocked() const
{
	return PlayerSelectionState != EPlayerSelectionState::Selecting;
}

float ARaidPlayerController::GetSelectionRemainingTime() const
{
	if (PlayerSelectionState != EPlayerSelectionState::Selecting || SelectionEndServerTime <= 0.0f)
	{
		return 0.0f;
	}

	const float RemainingTime = SelectionEndServerTime - GetSelectionServerTimeSeconds();
	return FMath::Clamp(RemainingTime, 0.0f, SelectionDurationSeconds);
}

float ARaidPlayerController::GetSelectionEndServerTime() const
{
	return SelectionEndServerTime;
}

TArray<FName> ARaidPlayerController::GetAvailablePartIDsForSlot(EDronePartSlot Slot) const
{
	switch (Slot)
	{
	case EDronePartSlot::Core:
		return GetCorePartIDs();
	case EDronePartSlot::RightWeapon:
	case EDronePartSlot::LeftWeapon:
		return GetWeaponPartIDs();
	default:
		return {};
	}
}

TArray<FName> ARaidPlayerController::GetCorePartIDs() const
{
	return {
		ADronePartInventory::GetCoreZenithPartID(),
		ADronePartInventory::GetCoreBoosterPartID(),
		ADronePartInventory::GetCoreDrainPartID()
	};
}

TArray<FName> ARaidPlayerController::GetWeaponPartIDs() const
{
	return {
		ADronePartInventory::GetPulseLaserPartID(),
		ADronePartInventory::GetFractureBurstPartID(),
		ADronePartInventory::GetVectorCannonPartID()
	};
}

FText ARaidPlayerController::GetPartDisplayName(FName PartID) const
{
	return GetFallbackPartDisplayName(PartID);
}

FText ARaidPlayerController::GetPartDescription(FName PartID) const
{
	return GetFallbackPartDescription(PartID);
}

UTexture2D* ARaidPlayerController::GetPartIcon(FName PartID) const
{
	if (const TObjectPtr<UTexture2D>* Icon = PartIconOverrides.Find(PartID))
	{
		return Icon->Get();
	}

	return nullptr;
}

int32 ARaidPlayerController::GetPartCurrentCount(FName PartID) const
{
	if (const ADronePartInventory* Inventory = GetDronePartInventory())
	{
		return Inventory->GetCurrentCount(PartID);
	}

	return 0;
}

int32 ARaidPlayerController::GetPartMaxCount(FName PartID) const
{
	if (const ADronePartInventory* Inventory = GetDronePartInventory())
	{
		return Inventory->GetMaxCount(PartID);
	}

	return 0;
}

FString ARaidPlayerController::BuildStableControllerLogString(const AController* Controller)
{
	if (!Controller)
	{
		return TEXT("None");
	}

	const FString ControllerName = GetNameSafe(Controller);
	if (const APlayerState* PS = Controller->PlayerState)
	{
		FString PlayerName = PS->GetPlayerName();
		PlayerName.TrimStartAndEndInline();
		if (!PlayerName.IsEmpty())
		{
			return FString::Printf(TEXT("%s:%d"), *PlayerName, PS->GetPlayerId());
		}
	}

	return ControllerName.IsEmpty() ? Controller->GetPathName() : ControllerName;
}

const TCHAR* ARaidPlayerController::SelectionStateToLogString(EPlayerSelectionState State)
{
	switch (State)
	{
	case EPlayerSelectionState::Selecting:
		return TEXT("Selecting");
	case EPlayerSelectionState::Locked:
		return TEXT("Locked");
	case EPlayerSelectionState::InBattle:
		return TEXT("InBattle");
	default:
		return TEXT("Unknown");
	}
}

const TCHAR* ARaidPlayerController::ReportGradeToLogString(EDroneReportGrade Grade)
{
	switch (Grade)
	{
	case EDroneReportGrade::S:
		return TEXT("S");
	case EDroneReportGrade::A:
		return TEXT("A");
	case EDroneReportGrade::B:
		return TEXT("B");
	case EDroneReportGrade::C:
		return TEXT("C");
	default:
		return TEXT("Unknown");
	}
}

const TCHAR* ARaidPlayerController::ReportTriggerToLogString(EDroneReportTrigger Trigger)
{
	switch (Trigger)
	{
	case EDroneReportTrigger::Death:
		return TEXT("Death");
	case EDroneReportTrigger::BossDefeated:
		return TEXT("BossDefeated");
	case EDroneReportTrigger::RaidTimeLimit:
		return TEXT("RaidTimeLimit");
	default:
		return TEXT("Unknown");
	}
}

bool ARaidPlayerController::ReportHasBonus(const FDroneReportData& ReportData, EDroneReportBonusType BonusType)
{
	return ReportData.AchievedBonusList.Contains(BonusType);
}

void ARaidPlayerController::Server_RequestSelectPart_Implementation(EPartSlot Slot, FName NewPartID)
{
	if (!HasAuthority())
	{
		return;
	}

	const FName SnapshotCorePartID = SelectedCorePartID;
	const FName SnapshotLeftWeaponPartID = SelectedLeftWeaponPartID;
	const FName SnapshotRightWeaponPartID = SelectedRightWeaponPartID;

	const FString PlayerLog = BuildControllerLogString(this);
	if (PlayerSelectionState == EPlayerSelectionState::Selecting)
	{
		if (ARaidGameState* RaidGameState = GetWorld() ? GetWorld()->GetGameState<ARaidGameState>() : nullptr)
		{
			if (RaidGameState->RaidState == ERaidState::Waiting)
			{
				RaidGameState->SetRaidStateForServer(ERaidState::Drafting);
			}
		}
	}
	const FString RaidStateLog = GetRaidStateLogString(this);
	const auto LogSelectSummary = [this, Slot, &PlayerLog, &RaidStateLog](FName PartID, bool bSuccess, const FString& Reason)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Select PC=%s Slot=%s Part=%s Result=%s Reason=%s Count=%d/%d SelectionState=%s RaidState=%s"),
			*PlayerLog,
			ToSelectionSlotLogString(Slot),
			*PartID.ToString(),
			bSuccess ? TEXT("Success") : TEXT("Fail"),
			*Reason,
			GetPartCurrentCount(PartID),
			GetPartMaxCount(PartID),
			ToPlayerSelectionStateLogString(PlayerSelectionState),
			*RaidStateLog);
	};
	const auto RestoreSelectionAfterServerError = [this, SnapshotCorePartID, SnapshotLeftWeaponPartID, SnapshotRightWeaponPartID]()
	{
		SetSelectedPartIDForSlotForServer(EPartSlot::Core, SnapshotCorePartID);
		SetSelectedPartIDForSlotForServer(EPartSlot::LeftWeapon, SnapshotLeftWeaponPartID);
		SetSelectedPartIDForSlotForServer(EPartSlot::RightWeapon, SnapshotRightWeaponPartID);
		Client_RestorePartSelectionAfterServerError(
			SnapshotCorePartID,
			SnapshotLeftWeaponPartID,
			SnapshotRightWeaponPartID);
	};

	UE_LOG(LogTemp, Log, TEXT("[Server] SelectPart Request: Player=%s Slot=%s NewPart=%s PlayerSelectionState=%s RaidState=%s"),
		*PlayerLog,
		ToSelectionSlotLogString(Slot),
		*NewPartID.ToString(),
		ToPlayerSelectionStateLogString(PlayerSelectionState),
		*RaidStateLog);

	if (PlayerSelectionState != EPlayerSelectionState::Selecting)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Server] SelectPart Failed: Player=%s Slot=%s NewPart=%s Reason=Selection locked PlayerSelectionState=%s RaidState=%s"),
			*PlayerLog,
			ToSelectionSlotLogString(Slot),
			*NewPartID.ToString(),
			ToPlayerSelectionStateLogString(PlayerSelectionState),
			*RaidStateLog);
		LogSelectSummary(NewPartID, false, TEXT("Selection locked"));
		Client_NotifyPartSelectionResult(Slot, NewPartID, false, TEXT("Selection locked"));
		return;
	}

	if (NewPartID.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Server] SelectPart Failed: Player=%s Slot=%s PreviousPart=None NewPart=None Reason=PartID is None"),
			*PlayerLog,
			ToSelectionSlotLogString(Slot));
		LogSelectSummary(NewPartID, false, TEXT("PartID is None. Use cancel."));
		Client_NotifyPartSelectionResult(Slot, NewPartID, false, TEXT("PartID is None. Use cancel."));
		return;
	}

	FName* SelectedPartID = GetSelectedPartIDForSlot(Slot);
	if (!SelectedPartID)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Server] SelectPart Failed: Player=%s Slot=%s PreviousPart=None NewPart=%s Reason=Invalid slot"),
			*PlayerLog,
			ToSelectionSlotLogString(Slot),
			*NewPartID.ToString());
		LogSelectSummary(NewPartID, false, TEXT("Invalid slot"));
		Client_NotifyPartSelectionResult(Slot, NewPartID, false, TEXT("Invalid slot"));
		return;
	}

	const FName PreviousPartID = *SelectedPartID;
	if (*SelectedPartID == NewPartID)
	{
		UE_LOG(LogTemp, Log, TEXT("[Server] SelectPart NoOp: Player=%s Slot=%s PreviousPart=%s NewPart=%s Reason=Already selected Count=%d/%d"),
			*PlayerLog,
			ToSelectionSlotLogString(Slot),
			*PreviousPartID.ToString(),
			*NewPartID.ToString(),
			GetPartCurrentCount(NewPartID),
			GetPartMaxCount(NewPartID));
		LogSelectSummary(NewPartID, true, TEXT("Already selected"));
		Client_NotifyPartSelectionResult(Slot, NewPartID, true, TEXT("Already selected"));
		return;
	}

	ADronePartInventory* Inventory = GetDronePartInventory();
	if (!Inventory)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Server] SelectPart Failed: Player=%s Slot=%s PreviousPart=%s NewPart=%s Reason=Inventory not ready"),
			*PlayerLog,
			ToSelectionSlotLogString(Slot),
			*PreviousPartID.ToString(),
			*NewPartID.ToString());
		LogSelectSummary(NewPartID, false, TEXT("Server error: Inventory not ready"));
		RestoreSelectionAfterServerError();
		return;
	}

	EDronePartType PartType = EDronePartType::Core;
	if (!Inventory->GetPartType(NewPartID, PartType))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Server] SelectPart Failed: Player=%s Slot=%s PreviousPart=%s NewPart=%s Reason=Unknown part"),
			*PlayerLog,
			ToSelectionSlotLogString(Slot),
			*PreviousPartID.ToString(),
			*NewPartID.ToString());
		LogSelectSummary(NewPartID, false, TEXT("Unknown part"));
		Client_NotifyPartSelectionResult(Slot, NewPartID, false, TEXT("Unknown part"));
		return;
	}

	if (!IsPartTypeAllowedForSlot(Slot, PartType))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Server] SelectPart Failed: Player=%s Slot=%s PreviousPart=%s NewPart=%s Reason=Part type mismatch"),
			*PlayerLog,
			ToSelectionSlotLogString(Slot),
			*PreviousPartID.ToString(),
			*NewPartID.ToString());
		LogSelectSummary(NewPartID, false, TEXT("Part type does not match slot"));
		Client_NotifyPartSelectionResult(Slot, NewPartID, false, TEXT("Part type does not match slot"));
		return;
	}

	UDronePartReturnManager* ReturnManager = GetDronePartReturnManager();
	if (!ReturnManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Server] SelectPart Failed: Player=%s Slot=%s PreviousPart=%s NewPart=%s Reason=Return manager not ready"),
			*PlayerLog,
			ToSelectionSlotLogString(Slot),
			*PreviousPartID.ToString(),
			*NewPartID.ToString());
		LogSelectSummary(NewPartID, false, TEXT("Server error: Return manager not ready"));
		RestoreSelectionAfterServerError();
		return;
	}

	FString CommitFailureReason;
	const EDronePartSelectionCommitResult CommitResult = ReturnManager->TryCommitSelectedPartChange(
		this,
		Slot,
		NewPartID,
		CommitFailureReason);
	if (CommitResult == EDronePartSelectionCommitResult::OutOfStock)
	{
		UE_LOG(LogTemp, Log, TEXT("Replace Failed: Player=%s Slot=%s PreviousPart=%s NewPart=%s Reason=New part out of stock, keeping old selection Count=%d/%d"),
			*PlayerLog,
			ToSelectionSlotLogString(Slot),
			*PreviousPartID.ToString(),
			*NewPartID.ToString(),
			Inventory->GetCurrentCount(NewPartID),
			Inventory->GetMaxCount(NewPartID));
		LogSelectSummary(NewPartID, false, TEXT("Out of stock"));
		Client_NotifyPartSelectionResult(Slot, NewPartID, false, TEXT("Out of stock"));
		return;
	}

	if (CommitResult == EDronePartSelectionCommitResult::ServerError)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Server] SelectPart Failed: Player=%s Slot=%s PreviousPart=%s NewPart=%s Reason=Atomic commit server error Detail=%s"),
			*PlayerLog,
			ToSelectionSlotLogString(Slot),
			*PreviousPartID.ToString(),
			*NewPartID.ToString(),
			*CommitFailureReason);
		LogSelectSummary(NewPartID, false, TEXT("Server error: Atomic commit failed"));
		RestoreSelectionAfterServerError();
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Server] SelectPart Success: Player=%s Slot=%s PreviousPart=%s NewPart=%s Count=%d/%d"),
		*PlayerLog,
		ToSelectionSlotLogString(Slot),
		*PreviousPartID.ToString(),
		*NewPartID.ToString(),
		Inventory->GetCurrentCount(NewPartID),
		Inventory->GetMaxCount(NewPartID));

	LogSelectSummary(NewPartID, true, PreviousPartID.IsNone() ? TEXT("Selected") : TEXT("Replaced"));
	Client_NotifyPartSelectionResult(Slot, NewPartID, true, PreviousPartID.IsNone() ? TEXT("Selected") : TEXT("Replaced"));
}

bool ARaidPlayerController::AreDroneReportDataTablesLoaded() const
{
	return DroneReportBonusDataTable != nullptr
		&& DroneReportSettingsDataTable != nullptr
		&& DroneReportGradeDataTable != nullptr;
}

bool ARaidPlayerController::RestartSelectionPhaseForServer(FName Reason)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Client] RestartSelectionPhaseForServer rejected: server authority required"));
		return false;
	}

	const FName ResolvedReason = Reason.IsNone() ? FName(TEXT("SelectionRestart")) : Reason;

	// 순서가 중요하다. 반환을 먼저 해야 슬롯이 비고, 슬롯이 빈 뒤에 상태를 되돌려야
	// 다음 Ready가 빈 선택에서 다시 시작한다. 반환은 기존 매니저 경로를 그대로 탄다.
	ReturnEquippedPartsForServer(EDronePartReturnReason::RaidEnd);
	ReturnSelectedPartsForServer(EDronePartReturnReason::RaidEnd);

	StopSelectionTimerForServer(ResolvedReason.ToString(), false);
	SetPlayerSelectionStateForServer(EPlayerSelectionState::Selecting);

	// 드론 복구는 상태를 선택 단계로 되돌린 뒤에 해야 한다 — RecalculateStats가 InBattle이면
	// 스스로 거부하기 때문이다.
	if (ADrone* ControlledDrone = Cast<ADrone>(GetPawn()))
	{
		ControlledDrone->ResetForSelectionPhaseForServer(ResolvedReason);
	}

	ClearBossTargetForServer(ResolvedReason);

	bDroneReportGenerated = false;
	LastDroneReportData = FDroneReportData();

	StartSelectionTimerForServer();

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] SelectionRestart PC=%s Reason=%s SelectionState=%s Core=%s Left=%s Right=%s"),
		*BuildControllerLogString(this),
		*ResolvedReason.ToString(),
		ToPlayerSelectionStateLogString(PlayerSelectionState),
		*SelectedCorePartID.ToString(),
		*SelectedLeftWeaponPartID.ToString(),
		*SelectedRightWeaponPartID.ToString());

	return true;
}

void ARaidPlayerController::Server_RequestCancelPart_Implementation(EPartSlot Slot)
{
	if (!HasAuthority())
	{
		return;
	}

	const FString PlayerLog = BuildControllerLogString(this);
	const FString RaidStateLog = GetRaidStateLogString(this);
	const auto LogCancelSummary = [this, Slot, &PlayerLog](FName PartID, bool bSuccess)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Cancel PC=%s Slot=%s Part=%s Result=%s Count=%d/%d"),
			*PlayerLog,
			ToSelectionSlotLogString(Slot),
			*PartID.ToString(),
			bSuccess ? TEXT("Success") : TEXT("Fail"),
			GetPartCurrentCount(PartID),
			GetPartMaxCount(PartID));
	};

	UE_LOG(LogTemp, Log, TEXT("[Server] CancelPart Request: Player=%s Slot=%s PlayerSelectionState=%s RaidState=%s"),
		*PlayerLog,
		ToSelectionSlotLogString(Slot),
		ToPlayerSelectionStateLogString(PlayerSelectionState),
		*RaidStateLog);

	if (PlayerSelectionState != EPlayerSelectionState::Selecting)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Server] CancelPart Failed: Player=%s Slot=%s Reason=Selection locked PlayerSelectionState=%s RaidState=%s"),
			*PlayerLog,
			ToSelectionSlotLogString(Slot),
			ToPlayerSelectionStateLogString(PlayerSelectionState),
			*RaidStateLog);
		LogCancelSummary(NAME_None, false);
		Client_NotifyPartSelectionResult(Slot, NAME_None, false, TEXT("Selection locked"));
		return;
	}

	FName* SelectedPartID = GetSelectedPartIDForSlot(Slot);
	if (!SelectedPartID)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Server] CancelPart Failed: Player=%s Slot=%s PreviousPart=None Reason=Invalid slot"),
			*PlayerLog,
			ToSelectionSlotLogString(Slot));
		LogCancelSummary(NAME_None, false);
		Client_NotifyPartSelectionResult(Slot, NAME_None, false, TEXT("Invalid slot"));
		return;
	}

	if (SelectedPartID->IsNone())
	{
		UE_LOG(LogTemp, Log, TEXT("[Server] CancelPart NoOp: Player=%s Slot=%s PreviousPart=None Reason=Already empty"),
			*PlayerLog,
			ToSelectionSlotLogString(Slot));
		LogCancelSummary(NAME_None, true);
		Client_NotifyPartSelectionResult(Slot, NAME_None, true, TEXT("Already empty"));
		return;
	}

	UDronePartReturnManager* ReturnManager = GetDronePartReturnManager();
	if (!ReturnManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Server] CancelPart Failed: Player=%s Slot=%s PreviousPart=%s Reason=Return manager not ready"),
			*PlayerLog,
			ToSelectionSlotLogString(Slot),
			*SelectedPartID->ToString());
		LogCancelSummary(*SelectedPartID, false);
		Client_NotifyPartSelectionResult(Slot, *SelectedPartID, false, TEXT("Return manager not ready"));
		return;
	}

	const FName ReturnedPartID = *SelectedPartID;
	if (!ReturnManager->ReturnSingleSelectedPart(this, Slot, EDronePartReturnReason::Cancel))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Server] CancelPart Failed: Player=%s Slot=%s PreviousPart=%s Reason=Return failed"),
			*PlayerLog,
			ToSelectionSlotLogString(Slot),
			*ReturnedPartID.ToString());
		LogCancelSummary(ReturnedPartID, false);
		Client_NotifyPartSelectionResult(Slot, ReturnedPartID, false, TEXT("Return failed"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Server] CancelPart Success: Player=%s Slot=%s PreviousPart=%s NewPart=None Count=%d/%d"),
		*PlayerLog,
		ToSelectionSlotLogString(Slot),
		*ReturnedPartID.ToString(),
		GetPartCurrentCount(ReturnedPartID),
		GetPartMaxCount(ReturnedPartID));

	LogCancelSummary(ReturnedPartID, true);
	Client_NotifyPartSelectionResult(Slot, ReturnedPartID, true, TEXT("Cancelled"));
}

void ARaidPlayerController::Server_RequestReadyForRaid_Implementation()
{
	ProcessReadyForRaidForServer(false);
}

void ARaidPlayerController::Server_RequestStartSelectionTimer_Implementation()
{
	if (!HasAuthority())
	{
		return;
	}

	StartSelectionTimerForServer();
}

void ARaidPlayerController::Server_RequestApplyTestDamageToDrone_Implementation(int32 DamageAmount)
{
	if (!HasAuthority())
	{
		return;
	}

	ADrone* ControlledDrone = Cast<ADrone>(GetPawn());
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] D6KillDrone RequestPC=%s TargetPC=%s TargetDrone=%s"),
		*BuildControllerLogString(this),
		*BuildControllerLogString(this),
		ControlledDrone ? *ControlledDrone->GetName() : TEXT("None"));

	if (ControlledDrone)
	{
		ControlledDrone->ApplyDamageForServer(DamageAmount, FName(TEXT("D6Test")));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Server] D6 test damage skipped: Player=%s Pawn is not ADrone"),
			*BuildControllerLogString(this));
	}
}

void ARaidPlayerController::Server_RequestRaidEndReturnTest_Implementation(FName Reason)
{
	if (!HasAuthority())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (ARaidGameMode* RaidGameMode = World->GetAuthGameMode<ARaidGameMode>())
		{
			RaidGameMode->ReturnAllEquippedPartsForRaidEnd(Reason.IsNone() ? FName(TEXT("D6Test")) : Reason);
			return;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[Server] D6 raid end return skipped: RaidGameMode missing Player=%s"),
		*BuildControllerLogString(this));
}

void ARaidPlayerController::Server_DebugTriggerBossTelegraphAttack_Implementation(float RadiusCm, int32 DamageAmount, float TelegraphSeconds, float ForwardOffsetCm)
{
	// Debug-only C2S bridge for PIE/manual testing. The server still validates and ARaidBoss owns execution/damage.
	HandleDebugTriggerBossTelegraphAttackForServer(RadiusCm, DamageAmount, TelegraphSeconds, ForwardOffsetCm);
}

void ARaidPlayerController::DebugBossSetStunned(int32 Stunned)
{
	const bool bStunned = Stunned != 0;
	if (HasAuthority())
	{
		HandleDebugSetBossStunnedForServer(bStunned);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossStunDebugTrigger PC=%s Result=RequestServer Scope=DebugOnly Stunned=%s"),
		*BuildControllerLogString(this),
		bStunned ? TEXT("true") : TEXT("false"));
	Server_DebugSetBossStunned(bStunned);
}

void ARaidPlayerController::Server_DebugSetBossStunned_Implementation(bool bStunned)
{
	// Debug-only C2S bridge. 상태 변경 검증/적용은 ARaidBoss::SetStunnedForServer가 소유한다.
	HandleDebugSetBossStunnedForServer(bStunned);
}

void ARaidPlayerController::HandleDebugSetBossStunnedForServer(bool bStunned)
{
	if (!HasAuthority())
	{
		return;
	}

	ARaidGameState* RaidGameState = GetWorld() ? GetWorld()->GetGameState<ARaidGameState>() : nullptr;
	ARaidBoss* Boss = RaidGameState ? RaidGameState->GetRaidBoss() : nullptr;
	if (!Boss)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossStun Ignored: Reason=NoBoss PC=%s"),
			*BuildControllerLogString(this));
		return;
	}

	Boss->SetStunnedForServer(bStunned, FName(TEXT("DebugExec")));
}

void ARaidPlayerController::Client_NotifyPartSelectionResult_Implementation(
	EPartSlot Slot,
	FName PartID,
	bool bSuccess,
	const FString& Reason)
{
	UE_LOG(LogTemp, Log, TEXT("[Client] Part selection result: Slot=%s Part=%s Success=%s Reason=%s"),
		ToSelectionSlotLogString(Slot),
		*PartID.ToString(),
		bSuccess ? TEXT("true") : TEXT("false"),
		*Reason);

	// 가이드 2절: 확정은 Confirm, 사용 불가(부품 수량 0 포함)는 Error.
	// 서버 판정 결과가 도착하는 이 지점이 "실제로 확정됐는가"를 아는 유일한 곳이다.
	PlayUISound(bSuccess ? SFX_UI_Confirm : SFX_UI_Error);

	bool bSelectedPartsChanged = false;
	if (bSuccess && (Reason == TEXT("Selected") || Reason == TEXT("Replaced")))
	{
		SetSelectedPartIDForSlot(Slot, PartID);
		bSelectedPartsChanged = true;
	}
	else if (bSuccess && Reason == TEXT("Cancelled"))
	{
		SetSelectedPartIDForSlot(Slot, NAME_None);
		bSelectedPartsChanged = true;
	}

	if (bSelectedPartsChanged)
	{
		OnSelectedPartsChanged.Broadcast();
		UE_LOG(LogTemp, VeryVerbose, TEXT("[Client] UI Refresh Requested: Player=%s Source=SelectionResult Slot=%s Part=%s Reason=%s"),
			*BuildControllerLogString(this),
			ToSelectionSlotLogString(Slot),
			*PartID.ToString(),
			*Reason);
		RefreshSelectionUI();
	}

	OnPartSelectionResult.Broadcast(Slot, PartID, bSuccess, Reason);
}

void ARaidPlayerController::Client_RestorePartSelectionAfterServerError_Implementation(
	FName AuthoritativeCorePartID,
	FName AuthoritativeLeftWeaponPartID,
	FName AuthoritativeRightWeaponPartID)
{
	SetSelectedPartIDForSlot(EPartSlot::Core, AuthoritativeCorePartID);
	SetSelectedPartIDForSlot(EPartSlot::LeftWeapon, AuthoritativeLeftWeaponPartID);
	SetSelectedPartIDForSlot(EPartSlot::RightWeapon, AuthoritativeRightWeaponPartID);

	UE_LOG(LogTemp, Warning, TEXT("[Client] Part selection restored after server error: Core=%s Left=%s Right=%s"),
		*AuthoritativeCorePartID.ToString(),
		*AuthoritativeLeftWeaponPartID.ToString(),
		*AuthoritativeRightWeaponPartID.ToString());
	OnSelectedPartsChanged.Broadcast();
	RefreshSelectionUI();
	OnPartSelectionServerError.Broadcast();
}

void ARaidPlayerController::Client_NotifyRaidReadyResult_Implementation(
	bool bSuccess,
	const FString& Reason,
	FName CorePartID,
	FName LeftWeaponPartID,
	FName RightWeaponPartID)
{
	UE_LOG(LogTemp, Log, TEXT("[Client] Raid ready result: Success=%s Reason=%s Core=%s Left=%s Right=%s"),
		bSuccess ? TEXT("true") : TEXT("false"),
		*Reason,
		*CorePartID.ToString(),
		*LeftWeaponPartID.ToString(),
		*RightWeaponPartID.ToString());

	// 가이드 2절: 입장 확정 시 1회. 준비가 거부되면 실패음으로 갈린다.
	PlayUISound(bSuccess ? SFX_UI_MatchSuccess : SFX_UI_Error);

	if (!bSuccess)
	{
		return;
	}

	PlayerSelectionState = EPlayerSelectionState::InBattle;
	SetEquippedPartIDForSlot(EPartSlot::Core, CorePartID);
	SetEquippedPartIDForSlot(EPartSlot::LeftWeapon, LeftWeaponPartID);
	SetEquippedPartIDForSlot(EPartSlot::RightWeapon, RightWeaponPartID);
	SetSelectedPartIDForSlot(EPartSlot::Core, NAME_None);
	SetSelectedPartIDForSlot(EPartSlot::LeftWeapon, NAME_None);
	SetSelectedPartIDForSlot(EPartSlot::RightWeapon, NAME_None);
	OnSelectedPartsChanged.Broadcast();
	RefreshSelectionUI();

	HideDronePartSelectUI();
	ShowBossHUDForLocalPlayer();
}

void ARaidPlayerController::Client_ReceiveDroneReport_Implementation(const FDroneReportData& ServerReportData)
{
	FDroneReportData ReportData = ServerReportData;
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (const URaidSessionSubsystem* Session = GameInstance->GetSubsystem<URaidSessionSubsystem>())
		{
			ReportData.Callsign = Session->GetCallsign();
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ReportReceived Player=%s Grade=%s BonusScore=%d"),
		*BuildControllerLogString(this),
		ReportGradeToLogString(ReportData.Grade),
		ReportData.BonusScore);

	if (IsLocalController())
	{
		HideBossHUDForLocalPlayer();
		ShowDroneReportWidget(ReportData);
	}
}

void ARaidPlayerController::Client_NotifyRaidEndedForUI_Implementation(FName Reason)
{
	PlayerSelectionState = EPlayerSelectionState::Locked;
	SetSelectedPartIDForSlot(EPartSlot::Core, NAME_None);
	SetSelectedPartIDForSlot(EPartSlot::LeftWeapon, NAME_None);
	SetSelectedPartIDForSlot(EPartSlot::RightWeapon, NAME_None);
	SetEquippedPartIDForSlot(EPartSlot::Core, NAME_None);
	SetEquippedPartIDForSlot(EPartSlot::LeftWeapon, NAME_None);
	SetEquippedPartIDForSlot(EPartSlot::RightWeapon, NAME_None);

	OnSelectedPartsChanged.Broadcast();
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] RaidEndClientRefresh Player=%s Reason=%s State=%s Core=None Left=None Right=None"),
		*BuildControllerLogString(this),
		Reason.IsNone() ? TEXT("RaidEnd") : *Reason.ToString(),
		ToPlayerSelectionStateLogString(PlayerSelectionState));
	RefreshSelectionUI();
	HideBossHUDForLocalPlayer();
}

void ARaidPlayerController::Client_NotifyRaidLoadFailed_Implementation(FName Reason, FName TargetMap)
{
	HandleRaidLoadFailedForClient(Reason, TargetMap);
}

void ARaidPlayerController::HandleRaidLoadFailedForClient(FName Reason, FName TargetMap)
{
	const FName NormalizedReason = Reason.IsNone() ? FName(TEXT("Unknown")) : Reason;
	const FName LobbyTargetMap = TargetMap.IsNone() ? FName(RaidLoadFailureLobbyMapName) : TargetMap;

	LastRaidLoadFailedReason = NormalizedReason;
	LastRaidLoadFailedTargetMap = LobbyTargetMap;

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] RaidLoadFailed Player=%s Reason=%s TargetMap=%s"),
		*BuildControllerLogString(this),
		*NormalizedReason.ToString(),
		*LobbyTargetMap.ToString());

	BP_OnRaidLoadFailed(NormalizedReason, LobbyTargetMap);
	// 가이드 2절이 "매칭/로드 실패 팝업"을 Error에 함께 묶었다.
	PlayUISound(SFX_UI_Error);
	HideBossHUDForLocalPlayer();
	ReturnToLobbyForRaidLoadFailure(NormalizedReason, LobbyTargetMap);
}

void ARaidPlayerController::ReturnToLobbyForRaidLoadFailure(FName Reason, FName TargetMap)
{
	if (bRaidLoadFailedReturnToLobbyRequested)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ReturnToLobbyIgnored Reason=AlreadyRequested Source=RaidLoadFailed Player=%s"),
			*BuildControllerLogString(this));
		return;
	}

	bRaidLoadFailedReturnToLobbyRequested = true;

#if WITH_DEV_AUTOMATION_TESTS
	if (bSuppressRaidLoadFailedLobbyTravelForTest)
	{
		++RaidLoadFailedReturnToLobbyCountForTest;
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ReturnToLobby Reason=%s Target=%s Source=RaidLoadFailed Player=%s Mode=SuppressedForAutomation"),
			Reason.IsNone() ? TEXT("Unknown") : *Reason.ToString(),
			TargetMap.IsNone() ? RaidLoadFailureLobbyMapName : *TargetMap.ToString(),
			*BuildControllerLogString(this));
		return;
	}
#endif

	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ReturnToLobbyIgnored Reason=InvalidWorldOrDedicatedServer Source=RaidLoadFailed Player=%s"),
			*BuildControllerLogString(this));
		return;
	}

	if (!IsLocalController())
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ReturnToLobbyIgnored Reason=NotLocalController Source=RaidLoadFailed Player=%s"),
			*BuildControllerLogString(this));
		return;
	}

	const FName LobbyTargetMap = TargetMap.IsNone() ? FName(RaidLoadFailureLobbyMapName) : TargetMap;
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ReturnToLobby Reason=%s Target=%s Source=RaidLoadFailed Player=%s Method=OpenLevel"),
		Reason.IsNone() ? TEXT("Unknown") : *Reason.ToString(),
		*LobbyTargetMap.ToString(),
		*BuildControllerLogString(this));

	UGameplayStatics::OpenLevel(World, LobbyTargetMap);
}

void ARaidPlayerController::D4SelectPart(FString SlotName, FString PartIDText)
{
	EPartSlot Slot = EPartSlot::Core;
	if (!TryParsePartSlot(SlotName, Slot))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Client] D4SelectPart failed. Unknown slot: %s"), *SlotName);
		return;
	}

	PartIDText.TrimStartAndEndInline();
	if (PartIDText.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Client] D4SelectPart failed. PartID is empty"));
		return;
	}

	Server_RequestSelectPart(Slot, FName(*PartIDText));
}

void ARaidPlayerController::D4CancelPart(FString SlotName)
{
	EPartSlot Slot = EPartSlot::Core;
	if (!TryParsePartSlot(SlotName, Slot))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Client] D4CancelPart failed. Unknown slot: %s"), *SlotName);
		return;
	}

	Server_RequestCancelPart(Slot);
}

void ARaidPlayerController::D6KillDrone()
{
	// Exec uses the console-owning PlayerController; an editor/server console can therefore target the server-side PC.
	Server_RequestApplyTestDamageToDrone(999999);
}

void ARaidPlayerController::D6RaidEndReturn(FString ReasonText)
{
	ReasonText.TrimStartAndEndInline();
	const FName Reason = ReasonText.IsEmpty() ? FName(TEXT("Manual")) : FName(*ReasonText);
	Server_RequestRaidEndReturnTest(Reason);
}

ADronePartInventory* ARaidPlayerController::GetDronePartInventory() const
{
	if (const UWorld* World = GetWorld())
	{
		if (const ARaidGameState* GS = World->GetGameState<ARaidGameState>())
		{
			ADronePartInventory* Inventory = GS->GetDronePartInventory();
			if (!Inventory)
			{
				UE_LOG(LogTemp, Verbose, TEXT("[%s] DronePartInventory lookup: Player=%s GameState=%s GameStateLocalRole=%s PC_LocalRole=%s Inventory=None"),
					HasAuthority() ? TEXT("Server") : TEXT("Client"),
					*BuildControllerLogString(this),
					*GS->GetName(),
					ToRaidPCNetRoleLogString(GS->GetLocalRole()),
					ToRaidPCNetRoleLogString(GetLocalRole()));
			}
			else
			{
				UE_LOG(LogTemp, Verbose, TEXT("[%s] DronePartInventory lookup: Player=%s NetMode=%s %s"),
					HasAuthority() ? TEXT("Server") : TEXT("Client"),
					*BuildControllerLogString(this),
					ToNetModeLogString(World->GetNetMode()),
					*BuildInventoryLookupDebugString(Inventory));
			}
			return Inventory;
		}

		UE_LOG(LogTemp, Verbose, TEXT("[%s] DronePartInventory lookup: Player=%s NetMode=%s GameState=None PC_LocalRole=%s"),
			HasAuthority() ? TEXT("Server") : TEXT("Client"),
			*BuildControllerLogString(this),
			ToNetModeLogString(World->GetNetMode()),
			ToRaidPCNetRoleLogString(GetLocalRole()));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] DronePartInventory lookup: Player=%s World=None PC_LocalRole=%s"),
			HasAuthority() ? TEXT("Server") : TEXT("Client"),
			*BuildControllerLogString(this),
			ToRaidPCNetRoleLogString(GetLocalRole()));
	}

	return nullptr;
}

UDronePartReturnManager* ARaidPlayerController::GetDronePartReturnManager() const
{
#if WITH_DEV_AUTOMATION_TESTS
	if (TestDronePartReturnManager)
	{
		return TestDronePartReturnManager;
	}
#endif

	if (const UWorld* World = GetWorld())
	{
		if (ARaidGameMode* GM = World->GetAuthGameMode<ARaidGameMode>())
		{
			return GM->GetDronePartReturnManager();
		}
	}

	return nullptr;
}

bool ARaidPlayerController::HasDroneReportGenerated() const
{
	return bDroneReportGenerated;
}


bool ARaidPlayerController::AssignBossTargetForServer()
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Client] AssignBossTargetForServer rejected: server authority required"));
		return false;
	}

	ARaidBoss* Boss = nullptr;
	if (ARaidGameState* RaidGameState = GetWorld() ? GetWorld()->GetGameState<ARaidGameState>() : nullptr)
	{
		Boss = RaidGameState->GetRaidBoss();
	}

	if (!Boss)
	{
		ClearBossTargetForServer(FName(TEXT("NoBoss")));
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Target Set: Result=Failed Reason=NoBoss Player=%s"),
			*BuildControllerLogString(this));
		return false;
	}

	if (!Boss->IsAliveForTargeting())
	{
		ClearBossTargetForServer(FName(TEXT("BossDead")));
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Target Set: Result=Failed Reason=BossDead Player=%s Boss=%s BossID=%s"),
			*BuildControllerLogString(this),
			*Boss->GetName(),
			*Boss->GetBossID().ToString());
		return false;
	}

	if (!Boss->IsTargetableForDrone())
	{
		ClearBossTargetForServer(FName(TEXT("NotTargetable")));
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Target Set: Result=Failed Reason=NotTargetable Player=%s Boss=%s BossID=%s"),
			*BuildControllerLogString(this),
			*Boss->GetName(),
			*Boss->GetBossID().ToString());
		return false;
	}

	CurrentTargetBoss = Boss;
	bIsTargetLocked = true;
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Target Set: Player=%s Boss=%s BossID=%s Result=Success"),
		*BuildControllerLogString(this),
		*Boss->GetName(),
		*Boss->GetBossID().ToString());
	RefreshTargetMarkerUI();
	return true;
}

void ARaidPlayerController::ClearBossTargetForServer(FName Reason)
{
	if (!HasAuthority())
	{
		return;
	}

	ARaidBoss* PreviousTarget = CurrentTargetBoss.Get();
	const bool bHadTargetState = PreviousTarget != nullptr || bIsTargetLocked;
	CurrentTargetBoss = nullptr;
	bIsTargetLocked = false;
	ForceNetUpdate();

	if (bHadTargetState)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Target Clear: Player=%s Boss=%s Reason=%s"),
			*BuildControllerLogString(this),
			PreviousTarget ? *PreviousTarget->GetName() : TEXT("None"),
			Reason.IsNone() ? TEXT("Cleanup") : *Reason.ToString());
		RefreshTargetMarkerUI();
	}
}

ARaidBoss* ARaidPlayerController::GetCurrentTargetBoss() const
{
	ARaidBoss* TargetBoss = CurrentTargetBoss.Get();
	return IsValid(TargetBoss) ? TargetBoss : nullptr;
}

bool ARaidPlayerController::IsTargetingBossForServer(const ARaidBoss* Boss) const
{
	return Boss != nullptr && CurrentTargetBoss.Get() == Boss;
}

bool ARaidPlayerController::IsTargetLocked() const
{
	return bIsTargetLocked;
}

bool ARaidPlayerController::HasValidBossTargetForServer() const
{
	FName InvalidReason;
	return HasValidBossTargetForServer(InvalidReason);
}

bool ARaidPlayerController::HasValidBossTargetForServer(FName& OutInvalidReason) const
{
	OutInvalidReason = NAME_None;

	if (PlayerSelectionState != EPlayerSelectionState::InBattle)
	{
		OutInvalidReason = FName(TEXT("NotInBattle"));
		return false;
	}

	if (const ADrone* Drone = Cast<ADrone>(GetPawn()))
	{
		if (Drone->IsDead())
		{
			OutInvalidReason = FName(TEXT("DroneDead"));
			return false;
		}
	}

	ARaidBoss* TargetBoss = GetCurrentTargetBoss();
	if (!TargetBoss)
	{
		if (const ARaidGameState* RaidGameState = GetWorld() ? GetWorld()->GetGameState<ARaidGameState>() : nullptr)
		{
			if (const ARaidBoss* RaidBoss = RaidGameState->GetRaidBoss())
			{
				if (RaidBoss->IsDefeated())
				{
					OutInvalidReason = FName(TEXT("BossDead"));
					return false;
				}
			}
		}
		OutInvalidReason = FName(TEXT("NoTarget"));
		return false;
	}

	if (!bIsTargetLocked)
	{
		OutInvalidReason = FName(TEXT("TargetUnlocked"));
		return false;
	}

	if (!TargetBoss->IsAliveForTargeting())
	{
		OutInvalidReason = FName(TEXT("BossDead"));
		return false;
	}

	if (!TargetBoss->IsTargetableForDrone())
	{
		OutInvalidReason = FName(TEXT("NotTargetable"));
		return false;
	}

	return true;
}

bool ARaidPlayerController::HasValidBossTarget() const
{
	return HasValidBossTargetForServer();
}

FVector ARaidPlayerController::GetTargetMarkerWorldLocation() const
{
	const ARaidBoss* TargetBoss = GetCurrentTargetBoss();
	return TargetBoss ? TargetBoss->GetTargetMarkerWorldLocation() : FVector::ZeroVector;
}

void ARaidPlayerController::RefreshTargetMarkerUI()
{
	if (!IsLocalController() || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	ARaidBoss* TargetBoss = GetCurrentTargetBoss();
	const bool bVisible = bIsTargetLocked && TargetBoss && TargetBoss->IsTargetableForDrone();
	BP_OnTargetMarkerChanged(bVisible, bVisible ? TargetBoss : nullptr);
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Target Marker: Player=%s Visible=%s Boss=%s Location=%s"),
		*BuildControllerLogString(this),
		bVisible ? TEXT("true") : TEXT("false"),
		TargetBoss ? *TargetBoss->GetName() : TEXT("None"),
		*GetTargetMarkerWorldLocation().ToString());
}

void ARaidPlayerController::BP_OnTargetMarkerChanged_Implementation(bool bVisible, ARaidBoss* TargetBoss)
{
#if WITH_DEV_AUTOMATION_TESTS
	TargetMarkerChangedCountForTest++;
	bLastTargetMarkerVisibleForTest = bVisible;
	LastTargetMarkerBossForTest = TargetBoss;
#endif
}

#if WITH_DEV_AUTOMATION_TESTS
void ARaidPlayerController::SetDronePartReturnManagerForTest(UDronePartReturnManager* InReturnManager)
{
	TestDronePartReturnManager = InReturnManager;
}

FDroneReportData ARaidPlayerController::GetLastDroneReportDataForTest() const
{
	return LastDroneReportData;
}

bool ARaidPlayerController::HasDroneReportGeneratedForTest() const
{
	return bDroneReportGenerated;
}


int32 ARaidPlayerController::GetTargetMarkerChangedCountForTest() const
{
	return TargetMarkerChangedCountForTest;
}

bool ARaidPlayerController::WasLastTargetMarkerVisibleForTest() const
{
	return bLastTargetMarkerVisibleForTest;
}

ARaidBoss* ARaidPlayerController::GetLastTargetMarkerBossForTest() const
{
	return LastTargetMarkerBossForTest.Get();
}

void ARaidPlayerController::ResetDroneReportForTest()
{
	bDroneReportGenerated = false;
	LastDroneReportData = FDroneReportData();
}

void ARaidPlayerController::SetDroneReportDataTablesForTest(
	UDataTable* BonusTable,
	UDataTable* SettingsTable,
	UDataTable* GradeTable)
{
	DroneReportBonusDataTable = BonusTable;
	DroneReportSettingsDataTable = SettingsTable;
	DroneReportGradeDataTable = GradeTable;
	bDroneReportConfigResolved = false;
	bDroneReportConfigUsesDataTables = false;
	DroneReportConfigFallbackReason.Reset();
}
#endif

bool ARaidPlayerController::RefreshDronePartInventoryBinding()
{
	ADronePartInventory* Inventory = GetDronePartInventory();
	if (!Inventory)
	{
		return false;
	}

	if (BoundDronePartInventory == Inventory)
	{
		return true;
	}

	if (BoundDronePartInventory)
	{
		BoundDronePartInventory->OnPartStocksChanged.RemoveDynamic(this, &ARaidPlayerController::HandleDronePartStocksChanged);
	}

	BoundDronePartInventory = Inventory;
	BoundDronePartInventory->OnPartStocksChanged.RemoveDynamic(this, &ARaidPlayerController::HandleDronePartStocksChanged);
	BoundDronePartInventory->OnPartStocksChanged.AddDynamic(this, &ARaidPlayerController::HandleDronePartStocksChanged);
	UE_LOG(LogTemp, VeryVerbose, TEXT("[Client] UI Refresh Requested: Player=%s Source=InventoryBinding"),
		*BuildControllerLogString(this));
	RefreshSelectionUI();
	return true;
}

void ARaidPlayerController::ShowDronePartSelectUI()
{
	if (!IsLocalController())
	{
		return;
	}

	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (!DronePartSelectWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Client] ShowDronePartSelectUI skipped: DronePartSelectWidgetClass is not set"));
		return;
	}

	if (!DronePartSelectWidget)
	{
		DronePartSelectWidget = CreateWidget<UUserWidget>(this, DronePartSelectWidgetClass);
	}

	if (!DronePartSelectWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Client] ShowDronePartSelectUI failed: widget could not be created"));
		return;
	}

	if (DronePartSelectWidget && !DronePartSelectWidget->IsInViewport())
	{
		DronePartSelectWidget->AddToViewport();
		RefreshDronePartInventoryBinding();

		SetShowMouseCursor(true);
		FInputModeGameAndUI InputMode;
		InputMode.SetWidgetToFocus(DronePartSelectWidget->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
		DronePartSelectWidget->SetKeyboardFocus();
		Server_RequestStartSelectionTimer();

		UE_LOG(LogTemp, Log, TEXT("[Client] Drone part select UI shown"));
	}
}

void ARaidPlayerController::HideDronePartSelectUI()
{
	if (!IsLocalController())
	{
		return;
	}

	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (!DronePartSelectWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Client] HideDronePartSelectUI skipped: widget has not been created"));
		return;
	}

	if (DronePartSelectWidget->IsInViewport())
	{
		DronePartSelectWidget->RemoveFromParent();
	}

	SetShowMouseCursor(false);
	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);

	UE_LOG(LogTemp, Log, TEXT("[Client] Drone part select UI hidden"));
}

void ARaidPlayerController::ShowDroneReportWidget(const FDroneReportData& ReportData)
{
	// 표시가 막히는 지점을 한 줄로 읽을 수 있어야 한다. 서버 생성(ReportCreated)과
	// 수신(ReportReceived)이 정상인데도 화면이 비면, 원인은 전부 이 함수 안의 조기 반환이다.
	const auto LogWidgetSkipped = [this, &ReportData](const TCHAR* Reason)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ReportWidgetSkipped Reason=%s Player=%s Grade=%s BonusScore=%d"),
			Reason,
			*BuildControllerLogString(this),
			ReportGradeToLogString(ReportData.Grade),
			ReportData.BonusScore);
	};

	if (!IsLocalController())
	{
		LogWidgetSkipped(TEXT("NotLocalController"));
		return;
	}

	if (GetNetMode() == NM_DedicatedServer)
	{
		LogWidgetSkipped(TEXT("DedicatedServer"));
		return;
	}

	if (!DroneReportWidgetClass)
	{
		LogWidgetSkipped(TEXT("NoWidgetClass"));
		return;
	}

	if (!CurrentDroneReportWidget)
	{
		CurrentDroneReportWidget = CreateWidget<UDroneReportWidget>(this, DroneReportWidgetClass);
	}

	if (!CurrentDroneReportWidget)
	{
		LogWidgetSkipped(TEXT("CreateWidgetFailed"));
		return;
	}

	const bool bAlreadyInViewport = CurrentDroneReportWidget->IsInViewport();
	CurrentDroneReportWidget->RefreshReport(ReportData);

	if (!bAlreadyInViewport)
	{
		CurrentDroneReportWidget->AddToViewport();
		SetShowMouseCursor(true);
		FInputModeGameAndUI InputMode;
		InputMode.SetWidgetToFocus(CurrentDroneReportWidget->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
		CurrentDroneReportWidget->SetKeyboardFocus();

		// 가이드 4절 인수검사: "Report Open → 화면 표시당 1회".
		// 이미 떠 있는 리포트를 갱신하는 경우는 새로 연 것이 아니므로 울리지 않는다.
		PlayUISound(SFX_UI_DroneReport_Open);
	}

	// 새로 띄웠든 이미 떠 있던 것을 갱신했든, 결과는 "화면에 리포트가 있다"로 같다.
	// 필드는 RefreshReport의 결과가 아니라 "이 호출 전에 이미 떠 있었는가"다. RefreshReport는
	// 반환값이 없다. 첫 표시는 항상 AlreadyVisible=false이며 그것이 정상이다.
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ReportWidgetShown Player=%s Grade=%s BonusScore=%d AlreadyVisible=%s"),
		*BuildControllerLogString(this),
		ReportGradeToLogString(ReportData.Grade),
		ReportData.BonusScore,
		bAlreadyInViewport ? TEXT("true") : TEXT("false"));
}

bool ARaidPlayerController::TryHandleDroneReportConfirmedForLocalPlayer(UDroneReportWidget* ReportWidget)
{
	// 프로덕션 계약: 확인 → LobbyMap. 위젯의 기존 travel 경로를 그대로 태운다.
	(void)ReportWidget;
	return false;
}

void ARaidPlayerController::HideDroneReportWidget()
{
	if (!IsLocalController())
	{
		return;
	}

	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (CurrentDroneReportWidget && CurrentDroneReportWidget->IsInViewport())
	{
		CurrentDroneReportWidget->RemoveFromParent();
	}

	if (DronePartSelectWidget && DronePartSelectWidget->IsInViewport())
	{
		SetShowMouseCursor(true);
		FInputModeGameAndUI InputMode;
		InputMode.SetWidgetToFocus(DronePartSelectWidget->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
	}
	else
	{
		SetShowMouseCursor(false);
		FInputModeGameOnly InputMode;
		SetInputMode(InputMode);
	}

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ReportWidgetHidden Player=%s"),
		*BuildControllerLogString(this));
}

void ARaidPlayerController::ShowBossHUDForLocalPlayer()
{
	if (!IsLocalController())
	{
		return;
	}

	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (!BossHUDWidgetClass)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] UIRefresh Source=BossHUDSkipped Reason=MissingClass Player=%s"),
			*BuildControllerLogString(this));
		return;
	}

	if (!BossHUDWidget)
	{
		BossHUDWidget = CreateWidget<UBossHUDWidget>(this, BossHUDWidgetClass);
	}

	if (!BossHUDWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Client] ShowBossHUDForLocalPlayer failed: widget could not be created Player=%s"),
			*BuildControllerLogString(this));
		return;
	}

	const bool bAlreadyInViewport = BossHUDWidget->IsInViewport();
	BossHUDWidget->RefreshBossHUD();

	if (!bAlreadyInViewport)
	{
		BossHUDWidget->AddToViewport();
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] UIRefresh Source=BossHUDShown Player=%s"),
			*BuildControllerLogString(this));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] UIRefresh Source=BossHUDRefreshed Player=%s"),
		*BuildControllerLogString(this));
}

void ARaidPlayerController::HideBossHUDForLocalPlayer()
{
	if (!IsLocalController())
	{
		return;
	}

	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (BossHUDWidget && BossHUDWidget->IsInViewport())
	{
		BossHUDWidget->RemoveFromParent();
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] UIRefresh Source=BossHUDHidden Player=%s"),
			*BuildControllerLogString(this));
	}
}

void ARaidPlayerController::RefreshBossHUDForLocalPlayer()
{
	if (!IsLocalController())
	{
		return;
	}

	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (BossHUDWidget)
	{
		BossHUDWidget->RefreshBossHUD();
	}
}

bool ARaidPlayerController::IsBossHUDVisibleForLocalPlayer() const
{
	return BossHUDWidget && BossHUDWidget->IsInViewport();
}

void ARaidPlayerController::RefreshSelectionUI()
{
	const bool bUseEquippedParts = PlayerSelectionState != EPlayerSelectionState::Selecting;
	const FName SummaryCorePartID = bUseEquippedParts ? EquippedCorePartID : SelectedCorePartID;
	const FName SummaryLeftWeaponPartID = bUseEquippedParts ? EquippedLeftWeaponPartID : SelectedLeftWeaponPartID;
	const FName SummaryRightWeaponPartID = bUseEquippedParts ? EquippedRightWeaponPartID : SelectedRightWeaponPartID;

	if (IsLocalController())
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] UIRefresh PC=%s TimeLeft=%.2f State=%s Core=%s Left=%s Right=%s"),
			*BuildControllerLogString(this),
			GetSelectionRemainingTime(),
			ToPlayerSelectionStateLogString(PlayerSelectionState),
			*SummaryCorePartID.ToString(),
			*SummaryLeftWeaponPartID.ToString(),
			*SummaryRightWeaponPartID.ToString());
	}

	OnPartSelectUIRefreshRequested.Broadcast();
}

FName* ARaidPlayerController::GetSelectedPartIDForSlot(EPartSlot Slot)
{
	switch (Slot)
	{
	case EPartSlot::Core:
		return &SelectedCorePartID;
	case EPartSlot::LeftWeapon:
		return &SelectedLeftWeaponPartID;
	case EPartSlot::RightWeapon:
		return &SelectedRightWeaponPartID;
	default:
		return nullptr;
	}
}

const FName* ARaidPlayerController::GetSelectedPartIDForSlot(EPartSlot Slot) const
{
	switch (Slot)
	{
	case EPartSlot::Core:
		return &SelectedCorePartID;
	case EPartSlot::LeftWeapon:
		return &SelectedLeftWeaponPartID;
	case EPartSlot::RightWeapon:
		return &SelectedRightWeaponPartID;
	default:
		return nullptr;
	}
}

FName* ARaidPlayerController::GetEquippedPartIDForSlot(EPartSlot Slot)
{
	switch (Slot)
	{
	case EPartSlot::Core:
		return &EquippedCorePartID;
	case EPartSlot::LeftWeapon:
		return &EquippedLeftWeaponPartID;
	case EPartSlot::RightWeapon:
		return &EquippedRightWeaponPartID;
	default:
		return nullptr;
	}
}

const FName* ARaidPlayerController::GetEquippedPartIDForSlot(EPartSlot Slot) const
{
	switch (Slot)
	{
	case EPartSlot::Core:
		return &EquippedCorePartID;
	case EPartSlot::LeftWeapon:
		return &EquippedLeftWeaponPartID;
	case EPartSlot::RightWeapon:
		return &EquippedRightWeaponPartID;
	default:
		return nullptr;
	}
}

void ARaidPlayerController::SetSelectedPartIDForSlot(EPartSlot Slot, FName PartID)
{
	if (FName* SelectedPartID = GetSelectedPartIDForSlot(Slot))
	{
		*SelectedPartID = PartID;
	}
}

void ARaidPlayerController::SetEquippedPartIDForSlot(EPartSlot Slot, FName PartID)
{
	if (FName* EquippedPartID = GetEquippedPartIDForSlot(Slot))
	{
		*EquippedPartID = PartID;
	}
}

void ARaidPlayerController::SetSelectedPartIDForSlotForServer(EPartSlot Slot, FName PartID)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Client] SetSelectedPartIDForSlotForServer rejected: Slot=%s Part=%s"),
			ToSelectionSlotLogString(Slot),
			*PartID.ToString());
		return;
	}

	SetSelectedPartIDForSlot(Slot, PartID);
}

void ARaidPlayerController::SetEquippedPartIDForSlotForServer(EPartSlot Slot, FName PartID)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Client] SetEquippedPartIDForSlotForServer rejected: Slot=%s Part=%s"),
			ToSelectionSlotLogString(Slot),
			*PartID.ToString());
		return;
	}

	SetEquippedPartIDForSlot(Slot, PartID);
}

void ARaidPlayerController::SetPlayerSelectionStateForServer(EPlayerSelectionState NewState)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Client] SetPlayerSelectionStateForServer rejected: NewState=%s"),
			ToPlayerSelectionStateLogString(NewState));
		return;
	}

	if (PlayerSelectionState == NewState)
	{
		return;
	}

	const EPlayerSelectionState PreviousState = PlayerSelectionState;
	PlayerSelectionState = NewState;
	ForceNetUpdate();

	UE_LOG(LogTemp, Log, TEXT("[Server] PlayerSelectionState changed: Player=%s Previous=%s New=%s RaidState=%s"),
		*BuildControllerLogString(this),
		ToPlayerSelectionStateLogString(PreviousState),
		ToPlayerSelectionStateLogString(PlayerSelectionState),
		*GetRaidStateLogString(this));

	if (ARaidGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ARaidGameState>() : nullptr)
	{
		if (ARaidBoss* Boss = GameState->GetRaidBoss())
		{
			Boss->NotifyPatternPopulationChangedForServer(FName(TEXT("SelectionStateChanged")));
		}
	}
}

float ARaidPlayerController::GetSelectionServerTimeSeconds() const
{
	if (const UWorld* World = GetWorld())
	{
		if (const AGameStateBase* GameState = World->GetGameState())
		{
			return GameState->GetServerWorldTimeSeconds();
		}

		return World->GetTimeSeconds();
	}

	return 0.0f;
}

bool ARaidPlayerController::ShouldAutoConfirmSelectionForServer() const
{
	// 원문 (7) "15초 종료 시 자동으로 현재 선택된 드론부품으로만 확정 — 바로 <전투 참가>".
	// 프로덕션은 예외 없이 이 계약을 따른다.
	return true;
}

void ARaidPlayerController::StartSelectionTimerForServer()
{
	if (!HasAuthority())
	{
		return;
	}

	if (!ShouldAutoConfirmSelectionForServer())
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] SelectTimerStart PC=%s Result=Skipped Reason=AutoConfirmDisabled SelectionState=%s RaidState=%s"),
			*BuildControllerLogString(this),
			ToPlayerSelectionStateLogString(PlayerSelectionState),
			*GetRaidStateLogString(this));
		return;
	}

	if (PlayerSelectionState != EPlayerSelectionState::Selecting)
	{
		UE_LOG(LogTemp, Log, TEXT("[Server] SelectTimerStart ignored: Player=%s PlayerSelectionState=%s RaidState=%s"),
			*BuildControllerLogString(this),
			ToPlayerSelectionStateLogString(PlayerSelectionState),
			*GetRaidStateLogString(this));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Server] SelectTimerStart failed: Player=%s Reason=World missing"),
			*BuildControllerLogString(this));
		return;
	}

	if (World->GetTimerManager().IsTimerActive(SelectionTimerHandle) && SelectionEndServerTime > GetSelectionServerTimeSeconds())
	{
		return;
	}

	SelectionEndServerTime = GetSelectionServerTimeSeconds() + SelectionDurationSeconds;
	World->GetTimerManager().SetTimer(
		SelectionTimerHandle,
		this,
		&ARaidPlayerController::HandleSelectionTimerExpiredForServer,
		SelectionDurationSeconds,
		false);
	ForceNetUpdate();

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] SelectTimerStart PC=%s Duration=%.2f SelectionState=%s RaidState=%s"),
		*BuildControllerLogString(this),
		SelectionDurationSeconds,
		ToPlayerSelectionStateLogString(PlayerSelectionState),
		*GetRaidStateLogString(this));
	RefreshSelectionUI();
}

void ARaidPlayerController::StopSelectionTimerForServer(const FString& Reason, bool bLogSummary)
{
	if (!HasAuthority())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SelectionTimerHandle);
	}

	SelectionEndServerTime = 0.0f;
	ForceNetUpdate();

	if (bLogSummary)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] SelectTimerStop PC=%s Reason=%s"),
			*BuildControllerLogString(this),
			*Reason);
	}

	RefreshSelectionUI();
}

void ARaidPlayerController::HandleSelectionTimerExpiredForServer()
{
	if (!HasAuthority())
	{
		return;
	}

	if (PlayerSelectionState != EPlayerSelectionState::Selecting)
	{
		const FString IgnoreReason = PlayerSelectionState == EPlayerSelectionState::InBattle
			? TEXT("AlreadyInBattle")
			: TEXT("SelectionLocked");
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] AutoReady PC=%s Result=Ignored Reason=%s SelectionState=%s"),
			*BuildControllerLogString(this),
			*IgnoreReason,
			ToPlayerSelectionStateLogString(PlayerSelectionState));
		StopSelectionTimerForServer(TEXT("AutoReadyIgnored"), false);
		return;
	}

	if (!ProcessReadyForRaidForServer(true))
	{
		StopSelectionTimerForServer(TEXT("AutoReadyFailed"), false);
	}
}

bool ARaidPlayerController::ProcessReadyForRaidForServer(bool bAutoReady)
{
	if (!HasAuthority())
	{
		return false;
	}

	const FString PlayerLog = BuildControllerLogString(this);
	const FString RaidStateLog = GetRaidStateLogString(this);
	const EPlayerSelectionState PreviousSelectionState = PlayerSelectionState;
	const auto LogReadySummary = [&PlayerLog, PreviousSelectionState](
		bool bSuccess,
		const FString& Reason,
		EPlayerSelectionState NewSelectionState,
		FName CorePartID,
		FName LeftWeaponPartID,
		FName RightWeaponPartID,
		const ADrone* ControlledDrone)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] Ready PC=%s Result=%s Reason=%s SelectionState=%s->%s Core=%s Left=%s Right=%s AttackPower=%d"),
			*PlayerLog,
			bSuccess ? TEXT("Success") : TEXT("Fail"),
			*Reason,
			ToPlayerSelectionStateLogString(PreviousSelectionState),
			ToPlayerSelectionStateLogString(NewSelectionState),
			*CorePartID.ToString(),
			*LeftWeaponPartID.ToString(),
			*RightWeaponPartID.ToString(),
			ControlledDrone ? ControlledDrone->GetAttackPower() : 0);
	};
	const auto LogAutoReadyFailure = [&PlayerLog, PreviousSelectionState](const FString& Reason)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] AutoReady PC=%s Result=Fail Reason=%s SelectionState=%s"),
			*PlayerLog,
			*Reason,
			ToPlayerSelectionStateLogString(PreviousSelectionState));
	};

	UE_LOG(LogTemp, Log, TEXT("[Server] RequestReadyForRaid received: Player=%s Source=%s PlayerSelectionState=%s RaidState=%s Core=%s Left=%s Right=%s"),
		*PlayerLog,
		bAutoReady ? TEXT("AutoReady") : TEXT("ManualReady"),
		ToPlayerSelectionStateLogString(PlayerSelectionState),
		*RaidStateLog,
		*SelectedCorePartID.ToString(),
		*SelectedLeftWeaponPartID.ToString(),
		*SelectedRightWeaponPartID.ToString());

	if (UWorld* World = GetWorld())
	{
		ARaidGameMode* RaidGameMode = World->GetAuthGameMode<ARaidGameMode>();
		if (!RaidGameMode)
		{
			for (TActorIterator<ARaidGameMode> It(World); It; ++It)
			{
				RaidGameMode = *It;
				break;
			}
		}

		if (RaidGameMode)
		{
			FName RejectReason;
			if (!RaidGameMode->CanAcceptRaidJoinForServer(RejectReason, false))
			{
				const FString FailureReason = RejectReason.IsNone() ? TEXT("Raid join rejected") : RejectReason.ToString();
				UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] RaidJoinRejected PC=%s Reason=%s Scope=Ready RaidState=%s"),
					*PlayerLog,
					*FailureReason,
					*RaidStateLog);

				if (bAutoReady)
				{
					UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] AutoReady PC=%s Result=Ignored Reason=%s SelectionState=%s"),
						*PlayerLog,
						*FailureReason,
						ToPlayerSelectionStateLogString(PlayerSelectionState));
				}
				else
				{
					LogReadySummary(false, FailureReason, PlayerSelectionState, SelectedCorePartID, SelectedLeftWeaponPartID, SelectedRightWeaponPartID, nullptr);
					Client_NotifyRaidReadyResult(false, FailureReason, SelectedCorePartID, SelectedLeftWeaponPartID, SelectedRightWeaponPartID);
				}
				return false;
			}
		}
	}

	if (ARaidGameState* RaidGameState = GetWorld() ? GetWorld()->GetGameState<ARaidGameState>() : nullptr)
	{
		if (RaidGameState->RaidState == ERaidState::End)
		{
			const FString FailureReason = TEXT("Raid ended");
			UE_LOG(LogTemp, Warning, TEXT("[Server] RequestReadyForRaid Failed: Player=%s Source=%s Reason=%s PlayerSelectionState=%s RaidState=%s"),
				*PlayerLog,
				bAutoReady ? TEXT("AutoReady") : TEXT("ManualReady"),
				*FailureReason,
				ToPlayerSelectionStateLogString(PlayerSelectionState),
				*RaidStateLog);

			if (bAutoReady)
			{
				UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] AutoReady PC=%s Result=Ignored Reason=RaidEnd SelectionState=%s"),
					*PlayerLog,
					ToPlayerSelectionStateLogString(PlayerSelectionState));
			}
			else
			{
				LogReadySummary(false, FailureReason, PlayerSelectionState, SelectedCorePartID, SelectedLeftWeaponPartID, SelectedRightWeaponPartID, nullptr);
				Client_NotifyRaidReadyResult(false, FailureReason, SelectedCorePartID, SelectedLeftWeaponPartID, SelectedRightWeaponPartID);
			}
			return false;
		}
	}

	if (PlayerSelectionState != EPlayerSelectionState::Selecting)
	{
		const FString FailureReason = TEXT("Selection locked");
		UE_LOG(LogTemp, Warning, TEXT("[Server] RequestReadyForRaid Failed: Player=%s Source=%s Reason=%s PlayerSelectionState=%s RaidState=%s"),
			*PlayerLog,
			bAutoReady ? TEXT("AutoReady") : TEXT("ManualReady"),
			*FailureReason,
			ToPlayerSelectionStateLogString(PlayerSelectionState),
			*RaidStateLog);

		if (bAutoReady)
		{
			const FString IgnoreReason = PlayerSelectionState == EPlayerSelectionState::InBattle
				? TEXT("AlreadyInBattle")
				: TEXT("SelectionLocked");
			UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] AutoReady PC=%s Result=Ignored Reason=%s SelectionState=%s"),
				*PlayerLog,
				*IgnoreReason,
				ToPlayerSelectionStateLogString(PlayerSelectionState));
		}
		else
		{
			LogReadySummary(false, FailureReason, PlayerSelectionState, SelectedCorePartID, SelectedLeftWeaponPartID, SelectedRightWeaponPartID, nullptr);
			Client_NotifyRaidReadyResult(false, FailureReason, SelectedCorePartID, SelectedLeftWeaponPartID, SelectedRightWeaponPartID);
		}
		return false;
	}

	FString FailureReason;
	if (!ValidateSelectedLoadoutForServer(FailureReason))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Server] RequestReadyForRaid Failed: Player=%s Source=%s Reason=%s Core=%s Left=%s Right=%s"),
			*PlayerLog,
			bAutoReady ? TEXT("AutoReady") : TEXT("ManualReady"),
			*FailureReason,
			*SelectedCorePartID.ToString(),
			*SelectedLeftWeaponPartID.ToString(),
			*SelectedRightWeaponPartID.ToString());
		if (bAutoReady)
		{
			LogAutoReadyFailure(FailureReason);
		}
		else
		{
			LogReadySummary(false, FailureReason, PlayerSelectionState, SelectedCorePartID, SelectedLeftWeaponPartID, SelectedRightWeaponPartID, nullptr);
		}
		Client_NotifyRaidReadyResult(false, FailureReason, SelectedCorePartID, SelectedLeftWeaponPartID, SelectedRightWeaponPartID);
		return false;
	}

	ADrone* ControlledDrone = Cast<ADrone>(GetPawn());
	if (!ControlledDrone)
	{
		FailureReason = TEXT("Controlled pawn is not ADrone");
		APawn* CurrentPawn = GetPawn();
		UE_LOG(LogTemp, Warning, TEXT("[Server] RequestReadyForRaid Failed: Player=%s Source=%s Reason=%s CurrentPawn=%s CurrentPawnClass=%s"),
			*PlayerLog,
			bAutoReady ? TEXT("AutoReady") : TEXT("ManualReady"),
			*FailureReason,
			CurrentPawn ? *CurrentPawn->GetName() : TEXT("None"),
			CurrentPawn ? *CurrentPawn->GetClass()->GetName() : TEXT("None"));
		if (bAutoReady)
		{
			LogAutoReadyFailure(FailureReason);
		}
		else
		{
			LogReadySummary(false, FailureReason, PlayerSelectionState, SelectedCorePartID, SelectedLeftWeaponPartID, SelectedRightWeaponPartID, nullptr);
		}
		Client_NotifyRaidReadyResult(false, FailureReason, SelectedCorePartID, SelectedLeftWeaponPartID, SelectedRightWeaponPartID);
		return false;
	}

	if (ControlledDrone->IsDead())
	{
		FailureReason = TEXT("DeadPawn");
		UE_LOG(LogTemp, Warning, TEXT("[Server] RequestReadyForRaid Ignored: Player=%s Source=%s Reason=%s PlayerSelectionState=%s RaidState=%s"),
			*PlayerLog,
			bAutoReady ? TEXT("AutoReady") : TEXT("ManualReady"),
			*FailureReason,
			ToPlayerSelectionStateLogString(PlayerSelectionState),
			*RaidStateLog);

		if (bAutoReady)
		{
			UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] AutoReady PC=%s Result=Ignored Reason=DeadPawn SelectionState=%s"),
				*PlayerLog,
				ToPlayerSelectionStateLogString(PlayerSelectionState));
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ReadyIgnored PC=%s Reason=DeadPawn SelectionState=%s"),
				*PlayerLog,
				ToPlayerSelectionStateLogString(PlayerSelectionState));
			Client_NotifyRaidReadyResult(false, FailureReason, SelectedCorePartID, SelectedLeftWeaponPartID, SelectedRightWeaponPartID);
		}
		return false;
	}

	const FName ReadyCorePartID = SelectedCorePartID;
	const FName ReadyLeftWeaponPartID = SelectedLeftWeaponPartID;
	const FName ReadyRightWeaponPartID = SelectedRightWeaponPartID;
	UWorld* World = GetWorld();
	ARaidGameState* RaidGameState = World ? World->GetGameState<ARaidGameState>() : nullptr;
	ARaidGameMode* RaidGameMode = World ? World->GetAuthGameMode<ARaidGameMode>() : nullptr;
	if (!RaidGameMode && World)
	{
		for (TActorIterator<ARaidGameMode> It(World); It; ++It)
		{
			RaidGameMode = *It;
			break;
		}
	}

	const bool bStartsBattle = RaidGameState
		&& (RaidGameState->RaidState == ERaidState::Waiting
			|| RaidGameState->RaidState == ERaidState::Drafting);
	if (bStartsBattle && RaidGameMode && !RaidGameMode->EnsureRaidBossForServer())
	{
		FailureReason = TEXT("RaidBoss spawn failed");
		UE_LOG(LogTemp, Warning, TEXT("[Server] RequestReadyForRaid Failed: Player=%s Source=%s Reason=%s"),
			*PlayerLog,
			bAutoReady ? TEXT("AutoReady") : TEXT("ManualReady"),
			*FailureReason);
		Client_NotifyRaidReadyResult(false, FailureReason, ReadyCorePartID, ReadyLeftWeaponPartID, ReadyRightWeaponPartID);
		return false;
	}

	if (!ControlledDrone->ApplyLoadout(ReadyCorePartID, ReadyLeftWeaponPartID, ReadyRightWeaponPartID))
	{
		FailureReason = TEXT("Drone ApplyLoadout failed");
		UE_LOG(LogTemp, Warning, TEXT("[Server] RequestReadyForRaid Failed: Player=%s Source=%s Reason=%s"),
			*PlayerLog,
			bAutoReady ? TEXT("AutoReady") : TEXT("ManualReady"),
			*FailureReason);
		if (bAutoReady)
		{
			LogAutoReadyFailure(FailureReason);
		}
		else
		{
			LogReadySummary(false, FailureReason, PlayerSelectionState, ReadyCorePartID, ReadyLeftWeaponPartID, ReadyRightWeaponPartID, ControlledDrone);
		}
		Client_NotifyRaidReadyResult(false, FailureReason, ReadyCorePartID, ReadyLeftWeaponPartID, ReadyRightWeaponPartID);
		return false;
	}

	MoveSelectedPartsToEquippedForServer();
	SetPlayerSelectionStateForServer(EPlayerSelectionState::InBattle);
	bDroneReportGenerated = false;
	LastDroneReportData = FDroneReportData();

	if (RaidGameState)
	{
		if (RaidGameState->RaidState == ERaidState::Waiting
			|| RaidGameState->RaidState == ERaidState::Drafting)
		{
			RaidGameState->SetRaidStateForServer(ERaidState::Battle);
		}
	}
	AssignBossTargetForServer();
	if (RaidGameMode)
	{
		RaidGameMode->StartRaidTimeLimitTimerForServer();
		RaidGameMode->StartBossPatternsForServer();
		RaidGameMode->ClearDroneReportKeyForServer(this, FName(TEXT("RaidReady")));
		if (UBalanceTelemetryComponent* Telemetry = RaidGameMode->GetBalanceTelemetryForServer())
		{
			Telemetry->EmitForServer(TEXT("LoadoutLocked"), {
				{TEXT("Player"), Telemetry->GetOrAssignPlayerAliasForServer(this)},
				{TEXT("Core"), ReadyCorePartID.ToString()},
				{TEXT("Left"), ReadyLeftWeaponPartID.ToString()},
				{TEXT("Right"), ReadyRightWeaponPartID.ToString()},
			});
		}
	}

	StopSelectionTimerForServer(bAutoReady ? TEXT("AutoReady") : TEXT("ManualReady"), !bAutoReady);

	UE_LOG(LogTemp, Log, TEXT("[Server] RequestReadyForRaid Success: Player=%s Source=%s PlayerSelectionState=%s Core=%s Left=%s Right=%s RaidState=%s"),
		*PlayerLog,
		bAutoReady ? TEXT("AutoReady") : TEXT("ManualReady"),
		ToPlayerSelectionStateLogString(PlayerSelectionState),
		*ReadyCorePartID.ToString(),
		*ReadyLeftWeaponPartID.ToString(),
		*ReadyRightWeaponPartID.ToString(),
		*GetRaidStateLogString(this));

	if (bAutoReady)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] AutoReady PC=%s Result=Success SelectionState=%s->%s Core=%s Left=%s Right=%s AttackPower=%d"),
			*PlayerLog,
			ToPlayerSelectionStateLogString(PreviousSelectionState),
			ToPlayerSelectionStateLogString(PlayerSelectionState),
			*ReadyCorePartID.ToString(),
			*ReadyLeftWeaponPartID.ToString(),
			*ReadyRightWeaponPartID.ToString(),
			ControlledDrone->GetAttackPower());
	}
	else
	{
		LogReadySummary(true, TEXT("Battle started"), PlayerSelectionState, ReadyCorePartID, ReadyLeftWeaponPartID, ReadyRightWeaponPartID, ControlledDrone);
	}

	Client_NotifyRaidReadyResult(true, bAutoReady ? TEXT("Auto ready") : TEXT("Battle started"), ReadyCorePartID, ReadyLeftWeaponPartID, ReadyRightWeaponPartID);
	return true;
}

void ARaidPlayerController::HandleDebugTriggerBossTelegraphAttackForServer(float RadiusCm, int32 DamageAmount, float TelegraphSeconds, float ForwardOffsetCm)
{
	const FString PlayerLog = BuildControllerLogString(this);
	const auto LogIgnored = [this, &PlayerLog, RadiusCm, DamageAmount, TelegraphSeconds, ForwardOffsetCm](const TCHAR* Reason)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossTelegraphDebugTrigger PC=%s Result=Ignored Reason=%s Scope=DebugOnly Radius=%.2f Damage=%d Delay=%.2f ForwardOffset=%.2f SelectionState=%s RaidState=%s"),
			*PlayerLog,
			Reason,
			RadiusCm,
			DamageAmount,
			TelegraphSeconds,
			ForwardOffsetCm,
			ToPlayerSelectionStateLogString(PlayerSelectionState),
			*GetRaidStateLogString(this));
	};

	if (!HasAuthority())
	{
		LogIgnored(TEXT("NotAuthority"));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		LogIgnored(TEXT("NoWorld"));
		return;
	}

	APawn* ControlledPawn = GetPawn();
	ADrone* ControlledDrone = Cast<ADrone>(ControlledPawn);
	if (!ControlledPawn || !ControlledDrone)
	{
		LogIgnored(TEXT("InvalidPawn"));
		return;
	}

	ARaidGameState* RaidGameState = World->GetGameState<ARaidGameState>();
	if (!RaidGameState)
	{
		LogIgnored(TEXT("NoGameState"));
		return;
	}

	ARaidBoss* Boss = RaidGameState->GetRaidBoss();
	if (!Boss)
	{
		LogIgnored(TEXT("NoBoss"));
		return;
	}

	const FVector AttackCenter = ControlledPawn->GetActorLocation() + (ControlledPawn->GetActorForwardVector() * ForwardOffsetCm);
	if (RaidGameState->RaidState == ERaidState::End)
	{
		LogIgnored(TEXT("RaidEnd"));
		Boss->StartDebugTelegraphedAreaAttackForServer(AttackCenter, RadiusCm, DamageAmount, TelegraphSeconds);
		return;
	}

	if (Boss->IsDefeated())
	{
		LogIgnored(TEXT("BossDead"));
		Boss->StartDebugTelegraphedAreaAttackForServer(AttackCenter, RadiusCm, DamageAmount, TelegraphSeconds);
		return;
	}

	if (PlayerSelectionState != EPlayerSelectionState::InBattle)
	{
		LogIgnored(TEXT("NotInBattle"));
		return;
	}

	if (ControlledDrone->IsDead())
	{
		LogIgnored(TEXT("DeadPawn"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BossTelegraphDebugTrigger PC=%s Result=Requested Scope=DebugOnly Boss=%s Pawn=%s Center=%s Radius=%.2f Damage=%d Delay=%.2f ForwardOffset=%.2f SelectionState=%s RaidState=%s"),
		*PlayerLog,
		*Boss->GetName(),
		*ControlledPawn->GetName(),
		*AttackCenter.ToString(),
		RadiusCm,
		DamageAmount,
		TelegraphSeconds,
		ForwardOffsetCm,
		ToPlayerSelectionStateLogString(PlayerSelectionState),
		ToRaidStateLogStringForPlayerController(RaidGameState->RaidState));

	Boss->StartDebugTelegraphedAreaAttackForServer(AttackCenter, RadiusCm, DamageAmount, TelegraphSeconds);
}

bool ARaidPlayerController::ReturnSelectedPartsForServer(EDronePartReturnReason Reason)
{
	if (!HasAuthority())
	{
		return false;
	}

	if (UDronePartReturnManager* ReturnManager = GetDronePartReturnManager())
	{
		return ReturnManager->ReturnSelectedParts(this, Reason);
	}

	return false;
}

bool ARaidPlayerController::ReturnEquippedPartsForServer(EDronePartReturnReason Reason)
{
	if (!HasAuthority())
	{
		return false;
	}

	if (UDronePartReturnManager* ReturnManager = GetDronePartReturnManager())
	{
		return ReturnManager->ReturnEquippedParts(this, Reason);
	}

	return false;
}

bool ARaidPlayerController::ReturnSingleSelectedPartForServer(EPartSlot Slot, EDronePartReturnReason Reason)
{
	if (!HasAuthority())
	{
		return false;
	}

	if (UDronePartReturnManager* ReturnManager = GetDronePartReturnManager())
	{
		return ReturnManager->ReturnSingleSelectedPart(this, Slot, Reason);
	}

	return false;
}

bool ARaidPlayerController::ReturnSingleEquippedPartForServer(EPartSlot Slot, EDronePartReturnReason Reason)
{
	if (!HasAuthority())
	{
		return false;
	}

	if (UDronePartReturnManager* ReturnManager = GetDronePartReturnManager())
	{
		return ReturnManager->ReturnSingleEquippedPart(this, Slot, Reason);
	}

	return false;
}

void ARaidPlayerController::FinalizeRaidEndForServer(FName Reason)
{
	if (!HasAuthority())
	{
		return;
	}

	StopSelectionTimerForServer(TEXT("RaidEnd"), false);
	ClearBossTargetForServer(FName(TEXT("Cleanup")));
	SetSelectedPartIDForSlotForServer(EPartSlot::Core, NAME_None);
	SetSelectedPartIDForSlotForServer(EPartSlot::LeftWeapon, NAME_None);
	SetSelectedPartIDForSlotForServer(EPartSlot::RightWeapon, NAME_None);
	SetEquippedPartIDForSlotForServer(EPartSlot::Core, NAME_None);
	SetEquippedPartIDForSlotForServer(EPartSlot::LeftWeapon, NAME_None);
	SetEquippedPartIDForSlotForServer(EPartSlot::RightWeapon, NAME_None);
	SetPlayerSelectionStateForServer(EPlayerSelectionState::Locked);
	Client_NotifyRaidEndedForUI(Reason);

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] RaidEndStateCleaned Player=%s Reason=%s State=%s Core=%s Left=%s Right=%s"),
		*BuildControllerLogString(this),
		Reason.IsNone() ? TEXT("RaidEnd") : *Reason.ToString(),
		ToPlayerSelectionStateLogString(PlayerSelectionState),
		*EquippedCorePartID.ToString(),
		*EquippedLeftWeaponPartID.ToString(),
		*EquippedRightWeaponPartID.ToString());
	RefreshSelectionUI();
}

bool ARaidPlayerController::TryCreateDroneReportForServer(EDroneReportTrigger Trigger, bool bBossDefeated)
{
	if (!HasAuthority())
	{
		return false;
	}

	const FString PlayerLog = BuildControllerLogString(this);
	if (bDroneReportGenerated)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ReportDuplicateIgnored Player=%s Reason=AlreadyGenerated Trigger=%s"),
			*PlayerLog,
			ReportTriggerToLogString(Trigger));
		return false;
	}

	ADrone* ControlledDrone = Cast<ADrone>(GetPawn());
	if (!ControlledDrone)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ReportDuplicateIgnored Player=%s Reason=NoDrone Trigger=%s"),
			*PlayerLog,
			ReportTriggerToLogString(Trigger));
		return false;
	}

	if (Trigger != EDroneReportTrigger::Death && ControlledDrone->IsDead())
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ReportDuplicateIgnored Player=%s Reason=DeadAtReport Trigger=%s"),
			*PlayerLog,
			ReportTriggerToLogString(Trigger));
		return false;
	}

	// PC 인스턴스 bool과 별개로, 재접속(새 PC)에도 유지되는 GameMode PlayerKey set으로 중복을 막는다.
	if (UWorld* World = GetWorld())
	{
		ARaidGameMode* RaidGameMode = World->GetAuthGameMode<ARaidGameMode>();
		if (!RaidGameMode)
		{
			for (TActorIterator<ARaidGameMode> It(World); It; ++It)
			{
				RaidGameMode = *It;
				break;
			}
		}
		if (RaidGameMode && !RaidGameMode->TryMarkDroneReportGeneratedForServer(this))
		{
			UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ReportDuplicateIgnored Player=%s Reason=AlreadyGeneratedForPlayerKey Trigger=%s"),
				*PlayerLog,
				ReportTriggerToLogString(Trigger));
			bDroneReportGenerated = true;
			return false;
		}
	}

	ClearBossTargetForServer(FName(TEXT("Report")));
	ControlledDrone->CancelDodgeForServer(FName(TEXT("Report")));

	const FDroneCombatRecord CombatRecord = ControlledDrone->GetCombatRecordForServer();
	LastDroneReportData = FDroneReportRules::BuildReportData(CombatRecord, bBossDefeated, ResolveDroneReportConfigForServer());
	bDroneReportGenerated = true;

	// REPORT-05: 생성 즉시 서버 목록에 보관한다. 이전에는 `LastDroneReportData` 1건만 남아
	// 레이드 종료 뒤 결과를 조회·집계할 수단이 없었다.
	if (UWorld* ReportWorld = GetWorld())
	{
		ARaidGameMode* ReportGameMode = ReportWorld->GetAuthGameMode<ARaidGameMode>();
		if (!ReportGameMode)
		{
			for (TActorIterator<ARaidGameMode> It(ReportWorld); It; ++It)
			{
				ReportGameMode = *It;
				break;
			}
		}
		if (ReportGameMode)
		{
			ReportGameMode->SaveDroneReportDataForServer(this, LastDroneReportData);
		}
	}

	const bool bBossSlayer = ReportHasBonus(LastDroneReportData, EDroneReportBonusType::BossSlayer);
	const bool bHighDPS = ReportHasBonus(LastDroneReportData, EDroneReportBonusType::HighDPS);
	const bool bNoDamage = ReportHasBonus(LastDroneReportData, EDroneReportBonusType::NoDamage);
	const bool bKeepMoving = ReportHasBonus(LastDroneReportData, EDroneReportBonusType::KeepMoving);
	const bool bHighRecovery = ReportHasBonus(LastDroneReportData, EDroneReportBonusType::HighRecovery);
	const float BasePerformanceScore = LastDroneReportData.ReportScore - static_cast<float>(LastDroneReportData.BonusScore);

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] BonusCalc Player=%s BossSlayer=%s HighDPS=%s NoDamage=%s KeepMoving=%s HighRecovery=%s BonusScore=%d"),
		*PlayerLog,
		bBossSlayer ? TEXT("true") : TEXT("false"),
		bHighDPS ? TEXT("true") : TEXT("false"),
		bNoDamage ? TEXT("true") : TEXT("false"),
		bKeepMoving ? TEXT("true") : TEXT("false"),
		bHighRecovery ? TEXT("true") : TEXT("false"),
		LastDroneReportData.BonusScore);

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] GradeCalc Player=%s BasePerformanceScore=%.2f ReportScore=%.2f Grade=%s"),
		*PlayerLog,
		BasePerformanceScore,
		LastDroneReportData.ReportScore,
		ReportGradeToLogString(LastDroneReportData.Grade));

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ReportCreated Player=%s SurvivalTime=%.2f BossDamage=%.2f BossDamageRatio=%.4f MoveDistance=%.2f HealAmount=%.2f BonusScore=%d ReportScore=%.2f Grade=%s Trigger=%s"),
		*PlayerLog,
		LastDroneReportData.SurvivalTime,
		LastDroneReportData.BossDamage,
		LastDroneReportData.BossDamageRatio,
		LastDroneReportData.MoveDistance,
		LastDroneReportData.HealAmount,
		LastDroneReportData.BonusScore,
		LastDroneReportData.ReportScore,
		ReportGradeToLogString(LastDroneReportData.Grade),
		ReportTriggerToLogString(Trigger));

	if (UBalanceTelemetryComponent* Telemetry = UBalanceTelemetryComponent::FindForServer(this))
	{
		Telemetry->EmitForServer(TEXT("DroneReportCreated"), {
			{TEXT("Player"), Telemetry->GetOrAssignPlayerAliasForServer(this)},
			{TEXT("SurvivalTime"), UBalanceTelemetryComponent::Number(LastDroneReportData.SurvivalTime)},
			{TEXT("BossDamage"), UBalanceTelemetryComponent::Number(LastDroneReportData.BossDamage)},
			{TEXT("MoveDistance"), UBalanceTelemetryComponent::Number(LastDroneReportData.MoveDistance)},
			{TEXT("HealAmount"), UBalanceTelemetryComponent::Number(LastDroneReportData.HealAmount)},
			{TEXT("DamageTakenCount"), FString::FromInt(LastDroneReportData.DamageTakenCount)},
			{TEXT("BonusScore"), FString::FromInt(LastDroneReportData.BonusScore)},
			{TEXT("Grade"), ReportGradeToLogString(LastDroneReportData.Grade)},
			{TEXT("ReportScore"), UBalanceTelemetryComponent::Number(LastDroneReportData.ReportScore)},
		});
	}

	Client_ReceiveDroneReport(LastDroneReportData);
	return true;
}

const FDroneReportResolvedConfig& ARaidPlayerController::ResolveDroneReportConfigForServer()
{
	if (bDroneReportConfigResolved)
	{
		return CachedDroneReportConfig;
	}

	EDroneReportDataFallbackReason FallbackReason = EDroneReportDataFallbackReason::None;
	bDroneReportConfigUsesDataTables = DroneReportData::TryResolve(
		{ DroneReportBonusDataTable, DroneReportSettingsDataTable, DroneReportGradeDataTable },
		CachedDroneReportConfig,
		FallbackReason);
	if (!bDroneReportConfigUsesDataTables)
	{
		CachedDroneReportConfig = FDroneReportRules::MakeCanonicalConfig();
		DroneReportConfigFallbackReason = DroneReportData::ToString(FallbackReason);
		UE_LOG(LogTemp, Warning, TEXT("[DR_SUMMARY] DroneReportData Source=Fallback Reason=%s"),
			*DroneReportConfigFallbackReason);
	}
	else
	{
		DroneReportConfigFallbackReason.Reset();
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] DroneReportData Source=DataTable"));
	}

	bDroneReportConfigResolved = true;
	return CachedDroneReportConfig;
}

bool ARaidPlayerController::IsPartTypeAllowedForSlot(EPartSlot Slot, EDronePartType PartType) const
{
	switch (Slot)
	{
	case EPartSlot::Core:
		return PartType == EDronePartType::Core;
	case EPartSlot::LeftWeapon:
	case EPartSlot::RightWeapon:
		return PartType == EDronePartType::Weapon;
	default:
		return false;
	}
}

bool ARaidPlayerController::TryParsePartSlot(const FString& SlotName, EPartSlot& OutSlot) const
{
	FString Normalized = SlotName;
	Normalized.TrimStartAndEndInline();
	Normalized.ToLowerInline();

	if (Normalized == TEXT("core"))
	{
		OutSlot = EPartSlot::Core;
		return true;
	}

	if (Normalized == TEXT("leftweapon") || Normalized == TEXT("left"))
	{
		OutSlot = EPartSlot::LeftWeapon;
		return true;
	}

	if (Normalized == TEXT("rightweapon") || Normalized == TEXT("right"))
	{
		OutSlot = EPartSlot::RightWeapon;
		return true;
	}

	return false;
}

bool ARaidPlayerController::ValidateSelectedLoadoutForServer(FString& OutReason) const
{
	if (!HasAuthority())
	{
		OutReason = TEXT("Server authority required");
		return false;
	}

	const bool bHasAnySelectedPart =
		!SelectedCorePartID.IsNone()
		|| !SelectedLeftWeaponPartID.IsNone()
		|| !SelectedRightWeaponPartID.IsNone();

	const ADronePartInventory* Inventory = GetDronePartInventory();
	if (bHasAnySelectedPart && !Inventory)
	{
		OutReason = TEXT("DronePartInventory is missing");
		return false;
	}

	if (!bHasAnySelectedPart)
	{
		OutReason.Reset();
		return true;
	}

	const auto ValidatePartForSlot = [Inventory, &OutReason](FName PartID, EPartSlot Slot) -> bool
	{
		if (PartID.IsNone())
		{
			return true;
		}

		EDronePartType PartType = EDronePartType::Core;
		if (!Inventory->GetPartType(PartID, PartType))
		{
			OutReason = FString::Printf(TEXT("Unknown PartID: %s"), *PartID.ToString());
			return false;
		}

		const bool bTypeMatches = Slot == EPartSlot::Core
			? PartType == EDronePartType::Core
			: PartType == EDronePartType::Weapon;
		if (!bTypeMatches)
		{
			OutReason = FString::Printf(TEXT("Part type mismatch: Slot=%s Part=%s"),
				ToSelectionSlotLogString(Slot),
				*PartID.ToString());
			return false;
		}

		return true;
	};

	if (!ValidatePartForSlot(SelectedCorePartID, EPartSlot::Core)
		|| !ValidatePartForSlot(SelectedLeftWeaponPartID, EPartSlot::LeftWeapon)
		|| !ValidatePartForSlot(SelectedRightWeaponPartID, EPartSlot::RightWeapon))
	{
		return false;
	}

	OutReason.Reset();
	return true;
}

void ARaidPlayerController::MoveSelectedPartsToEquippedForServer()
{
	if (!HasAuthority())
	{
		return;
	}

	SetEquippedPartIDForSlot(EPartSlot::Core, SelectedCorePartID);
	SetEquippedPartIDForSlot(EPartSlot::LeftWeapon, SelectedLeftWeaponPartID);
	SetEquippedPartIDForSlot(EPartSlot::RightWeapon, SelectedRightWeaponPartID);

	SetSelectedPartIDForSlot(EPartSlot::Core, NAME_None);
	SetSelectedPartIDForSlot(EPartSlot::LeftWeapon, NAME_None);
	SetSelectedPartIDForSlot(EPartSlot::RightWeapon, NAME_None);
}

void ARaidPlayerController::HandleDronePartStocksChanged()
{
	UE_LOG(LogTemp, VeryVerbose, TEXT("[Client] UI Refresh Requested: Player=%s Source=PartStocksChanged"),
		*BuildControllerLogString(this));
	RefreshSelectionUI();
}

void ARaidPlayerController::OnRep_PlayerSelectionState()
{
	UE_LOG(LogTemp, Log, TEXT("[Client] PlayerSelectionState replicated: Player=%s State=%s RaidState=%s"),
		*BuildControllerLogString(this),
		ToPlayerSelectionStateLogString(PlayerSelectionState),
		*GetRaidStateLogString(this));
	RefreshSelectionUI();

	if (PlayerSelectionState == EPlayerSelectionState::InBattle)
	{
		ShowBossHUDForLocalPlayer();
	}
	else
	{
		HideBossHUDForLocalPlayer();
	}
}

void ARaidPlayerController::OnRep_SelectionEndServerTime()
{
	RefreshSelectionUI();
}

void ARaidPlayerController::OnRep_CurrentTargetBoss()
{
	RefreshTargetMarkerUI();
	RefreshBossHUDForLocalPlayer();
}
