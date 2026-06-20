#include "RaidPlayerController.h"
#include "Drone.h"
#include "DronePartInventory.h"
#include "RaidGameMode.h"
#include "RaidGameState.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"

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
	if (!PC)
	{
		return TEXT("None");
	}

	if (const APlayerState* PS = PC->PlayerState)
	{
		return FString::Printf(TEXT("%s:%d"), *PS->GetPlayerName(), PS->GetPlayerId());
	}

	return PC->GetName();
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

	if (bAutoShowDronePartSelectUI)
	{
		ShowDronePartSelectUI();
	}
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

void ARaidPlayerController::Server_RequestSelectPart_Implementation(EPartSlot Slot, FName NewPartID)
{
	if (!HasAuthority())
	{
		return;
	}

	const FString PlayerLog = BuildControllerLogString(this);

	if (NewPartID.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Server] SelectPart Failed: Player=%s Slot=%s PreviousPart=None NewPart=None Reason=PartID is None"),
			*PlayerLog,
			ToSelectionSlotLogString(Slot));
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

	Client_NotifyPartSelectionResult(Slot, NewPartID, true, PreviousPartID.IsNone() ? TEXT("Selected") : TEXT("Replaced"));
}

void ARaidPlayerController::Server_RequestCancelPart_Implementation(EPartSlot Slot)
{
	if (!HasAuthority())
	{
		return;
	}

	const FString PlayerLog = BuildControllerLogString(this);

	FName* SelectedPartID = GetSelectedPartIDForSlot(Slot);
	if (!SelectedPartID)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Server] CancelPart Failed: Player=%s Slot=%s PreviousPart=None Reason=Invalid slot"),
			*PlayerLog,
			ToSelectionSlotLogString(Slot));
		Client_NotifyPartSelectionResult(Slot, NAME_None, false, TEXT("Invalid slot"));
		return;
	}

	if (SelectedPartID->IsNone())
	{
		UE_LOG(LogTemp, Log, TEXT("[Server] CancelPart NoOp: Player=%s Slot=%s PreviousPart=None Reason=Already empty"),
			*PlayerLog,
			ToSelectionSlotLogString(Slot));
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
		Client_NotifyPartSelectionResult(Slot, ReturnedPartID, false, TEXT("Return failed"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Server] CancelPart Success: Player=%s Slot=%s PreviousPart=%s NewPart=None Count=%d/%d"),
		*PlayerLog,
		ToSelectionSlotLogString(Slot),
		*ReturnedPartID.ToString(),
		GetPartCurrentCount(ReturnedPartID),
		GetPartMaxCount(ReturnedPartID));

	Client_NotifyPartSelectionResult(Slot, ReturnedPartID, true, TEXT("Cancelled"));
}

void ARaidPlayerController::Server_RequestReadyForRaid_Implementation()
{
	if (!HasAuthority())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Server] RequestReadyForRaid received: Player=%s Core=%s Left=%s Right=%s"),
		*BuildControllerLogString(this),
		*SelectedCorePartID.ToString(),
		*SelectedLeftWeaponPartID.ToString(),
		*SelectedRightWeaponPartID.ToString());

	FString FailureReason;
	if (!ValidateSelectedLoadoutForServer(FailureReason))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Server] RequestReadyForRaid Failed: Player=%s Reason=%s Core=%s Left=%s Right=%s"),
			*BuildControllerLogString(this),
			*FailureReason,
			*SelectedCorePartID.ToString(),
			*SelectedLeftWeaponPartID.ToString(),
			*SelectedRightWeaponPartID.ToString());
		Client_NotifyRaidReadyResult(false, FailureReason, SelectedCorePartID, SelectedLeftWeaponPartID, SelectedRightWeaponPartID);
		return;
	}

	ADrone* ControlledDrone = Cast<ADrone>(GetPawn());
	if (!ControlledDrone)
	{
		FailureReason = TEXT("Controlled pawn is not ADrone");
		UE_LOG(LogTemp, Warning, TEXT("[Server] RequestReadyForRaid Failed: Player=%s Reason=%s"),
			*BuildControllerLogString(this),
			*FailureReason);
		Client_NotifyRaidReadyResult(false, FailureReason, SelectedCorePartID, SelectedLeftWeaponPartID, SelectedRightWeaponPartID);
		return;
	}

	const FName ReadyCorePartID = SelectedCorePartID;
	const FName ReadyLeftWeaponPartID = SelectedLeftWeaponPartID;
	const FName ReadyRightWeaponPartID = SelectedRightWeaponPartID;

	if (!ControlledDrone->ApplyLoadout(ReadyCorePartID, ReadyLeftWeaponPartID, ReadyRightWeaponPartID))
	{
		FailureReason = TEXT("Drone ApplyLoadout failed");
		UE_LOG(LogTemp, Warning, TEXT("[Server] RequestReadyForRaid Failed: Player=%s Reason=%s"),
			*BuildControllerLogString(this),
			*FailureReason);
		Client_NotifyRaidReadyResult(false, FailureReason, ReadyCorePartID, ReadyLeftWeaponPartID, ReadyRightWeaponPartID);
		return;
	}

	MoveSelectedPartsToEquippedForServer();

	if (ARaidGameState* RaidGameState = GetWorld() ? GetWorld()->GetGameState<ARaidGameState>() : nullptr)
	{
		RaidGameState->SetRaidStateForServer(ERaidState::Battle);
	}

	UE_LOG(LogTemp, Log, TEXT("[Server] RequestReadyForRaid Success: Player=%s Core=%s Left=%s Right=%s RaidState=Battle"),
		*BuildControllerLogString(this),
		*ReadyCorePartID.ToString(),
		*ReadyLeftWeaponPartID.ToString(),
		*ReadyRightWeaponPartID.ToString());

	Client_NotifyRaidReadyResult(true, TEXT("Battle started"), ReadyCorePartID, ReadyLeftWeaponPartID, ReadyRightWeaponPartID);
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
		OnPartSelectUIRefreshRequested.Broadcast();
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

	SetEquippedPartIDForSlot(EPartSlot::Core, CorePartID);
	SetEquippedPartIDForSlot(EPartSlot::LeftWeapon, LeftWeaponPartID);
	SetEquippedPartIDForSlot(EPartSlot::RightWeapon, RightWeaponPartID);
	SetSelectedPartIDForSlot(EPartSlot::Core, NAME_None);
	SetSelectedPartIDForSlot(EPartSlot::LeftWeapon, NAME_None);
	SetSelectedPartIDForSlot(EPartSlot::RightWeapon, NAME_None);
	OnSelectedPartsChanged.Broadcast();

	HideDronePartSelectUI();
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
	if (const UWorld* World = GetWorld())
	{
		if (ARaidGameMode* GM = World->GetAuthGameMode<ARaidGameMode>())
		{
			return GM->GetDronePartReturnManager();
		}
	}

	return nullptr;
}

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
	OnPartSelectUIRefreshRequested.Broadcast();
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

	if (SelectedCorePartID.IsNone())
	{
		OutReason = TEXT("Core part is not selected");
		return false;
	}
	if (SelectedLeftWeaponPartID.IsNone())
	{
		OutReason = TEXT("Left weapon part is not selected");
		return false;
	}
	if (SelectedRightWeaponPartID.IsNone())
	{
		OutReason = TEXT("Right weapon part is not selected");
		return false;
	}

	const ADronePartInventory* Inventory = GetDronePartInventory();
	if (!Inventory)
	{
		OutReason = TEXT("DronePartInventory is missing");
		return false;
	}

	const auto ValidatePartForSlot = [Inventory, &OutReason](FName PartID, EPartSlot Slot) -> bool
	{
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
	OnPartSelectUIRefreshRequested.Broadcast();
}
