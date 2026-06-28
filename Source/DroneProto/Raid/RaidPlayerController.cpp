#include "RaidPlayerController.h"
#include "Drone.h"
#include "DronePartInventory.h"
#include "DroneReportWidget.h"
#include "RaidGameMode.h"
#include "RaidGameState.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

namespace
{
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

const TCHAR* ToRaidStateLogString(ERaidState State)
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
	return RaidGameState ? ToRaidStateLogString(RaidGameState->RaidState) : TEXT("None");
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
		ShowDronePartSelectUI();
	}
}

void ARaidPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		StopSelectionTimerForServer(TEXT("EndPlay"), false);
	}

	Super::EndPlay(EndPlayReason);
}

void ARaidPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARaidPlayerController, PlayerSelectionState);
	DOREPLIFETIME_CONDITION(ARaidPlayerController, SelectionEndServerTime, COND_OwnerOnly);
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

	const FString PlayerLog = BuildControllerLogString(this);
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
		LogSelectSummary(NewPartID, false, TEXT("Inventory not ready"));
		Client_NotifyPartSelectionResult(Slot, NewPartID, false, TEXT("Inventory not ready"));
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
		LogSelectSummary(NewPartID, false, TEXT("Return manager not ready"));
		Client_NotifyPartSelectionResult(Slot, NewPartID, false, TEXT("Return manager not ready"));
		return;
	}

	if (!Inventory->IsPartAvailable(NewPartID))
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

	if (!PreviousPartID.IsNone())
	{
		if (!ReturnManager->ReturnSingleSelectedPart(this, Slot, EDronePartReturnReason::Replace))
		{
			UE_LOG(LogTemp, Warning, TEXT("[Server] SelectPart Failed: Player=%s Slot=%s PreviousPart=%s NewPart=%s Reason=Failed to return previous part"),
				*PlayerLog,
				ToSelectionSlotLogString(Slot),
				*PreviousPartID.ToString(),
				*NewPartID.ToString());
			LogSelectSummary(NewPartID, false, TEXT("Failed to return previous part"));
			Client_NotifyPartSelectionResult(Slot, NewPartID, false, TEXT("Failed to return previous part"));
			return;
		}
	}

	if (!Inventory->TryConsumePart(NewPartID))
	{
		UE_LOG(LogTemp, Warning, TEXT("Replace Failed: Player=%s Slot=%s PreviousPart=%s NewPart=%s Reason=New part consume failed after return Count=%d/%d"),
			*PlayerLog,
			ToSelectionSlotLogString(Slot),
			*PreviousPartID.ToString(),
			*NewPartID.ToString(),
			Inventory->GetCurrentCount(NewPartID),
			Inventory->GetMaxCount(NewPartID));

		if (!PreviousPartID.IsNone() && Inventory->TryConsumePart(PreviousPartID))
		{
			SetSelectedPartIDForSlotForServer(Slot, PreviousPartID);
		}

		LogSelectSummary(NewPartID, false, TEXT("Out of stock"));
		Client_NotifyPartSelectionResult(Slot, NewPartID, false, TEXT("Out of stock"));
		return;
	}

	SetSelectedPartIDForSlotForServer(Slot, NewPartID);

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
}

void ARaidPlayerController::Client_ReceiveDroneReport_Implementation(const FDroneReportData& ReportData)
{
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ReportClientReceived Player=%s Grade=%s BonusScore=%d"),
		*BuildControllerLogString(this),
		ReportGradeToLogString(ReportData.Grade),
		ReportData.BonusScore);

	if (IsLocalController())
	{
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

void ARaidPlayerController::ResetDroneReportForTest()
{
	bDroneReportGenerated = false;
	LastDroneReportData = FDroneReportData();
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
	if (!IsLocalController())
	{
		return;
	}

	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (!DroneReportWidgetClass)
	{
		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ReportWidgetMissingClass Player=%s Grade=%s BonusScore=%d"),
			*BuildControllerLogString(this),
			ReportGradeToLogString(ReportData.Grade),
			ReportData.BonusScore);
		return;
	}

	if (!CurrentDroneReportWidget)
	{
		CurrentDroneReportWidget = CreateWidget<UDroneReportWidget>(this, DroneReportWidgetClass);
	}

	if (!CurrentDroneReportWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Client] ShowDroneReportWidget failed: widget could not be created Player=%s"),
			*BuildControllerLogString(this));
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

		UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ReportWidgetShown Player=%s Grade=%s BonusScore=%d"),
			*BuildControllerLogString(this),
			ReportGradeToLogString(ReportData.Grade),
			ReportData.BonusScore);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] ReportWidgetRefreshed Player=%s Grade=%s BonusScore=%d"),
		*BuildControllerLogString(this),
		ReportGradeToLogString(ReportData.Grade),
		ReportData.BonusScore);
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

void ARaidPlayerController::StartSelectionTimerForServer()
{
	if (!HasAuthority())
	{
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

	if (ARaidGameState* RaidGameState = GetWorld() ? GetWorld()->GetGameState<ARaidGameState>() : nullptr)
	{
		RaidGameState->SetRaidStateForServer(ERaidState::Battle);
	}
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
			RaidGameMode->StartRaidTimeLimitTimerForServer();
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

	const FDroneCombatRecord CombatRecord = ControlledDrone->GetCombatRecordForServer();
	LastDroneReportData = FDroneReportRules::BuildReportData(CombatRecord, bBossDefeated);
	bDroneReportGenerated = true;

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

	Client_ReceiveDroneReport(LastDroneReportData);
	return true;
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
}

void ARaidPlayerController::OnRep_SelectionEndServerTime()
{
	RefreshSelectionUI();
}
