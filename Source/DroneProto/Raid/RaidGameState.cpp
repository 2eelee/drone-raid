#include "RaidGameState.h"
#include "DronePartInventory.h"
#include "RaidBoss.h"
#include "RaidPlayerController.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

namespace
{
const TCHAR* ToNetRoleLogString(ENetRole Role)
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

FString BuildInventoryReplicationDebugString(const ADronePartInventory* Inventory)
{
	if (!Inventory)
	{
		return TEXT("Inventory=None");
	}

	return FString::Printf(
		TEXT("Inventory=%s HasAuthority=%s LocalRole=%s RemoteRole=%s Replicates=%s AlwaysRelevant=%s Dormancy=%d"),
		*Inventory->GetName(),
		Inventory->HasAuthority() ? TEXT("true") : TEXT("false"),
		ToNetRoleLogString(Inventory->GetLocalRole()),
		ToNetRoleLogString(Inventory->GetRemoteRole()),
		Inventory->GetIsReplicated() ? TEXT("true") : TEXT("false"),
		Inventory->bAlwaysRelevant ? TEXT("true") : TEXT("false"),
		static_cast<int32>(Inventory->NetDormancy));
}
}

ARaidGameState::ARaidGameState()
{
	RaidState = ERaidState::Waiting;
	CurrentPlayers = 0;
	DronePartInventory = nullptr;
	RaidBoss = nullptr;
}

void ARaidGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARaidGameState, RaidState);
	DOREPLIFETIME(ARaidGameState, CurrentPlayers);
	DOREPLIFETIME(ARaidGameState, DronePartInventory);
	DOREPLIFETIME(ARaidGameState, RaidBoss);
}

ADronePartInventory* ARaidGameState::GetDronePartInventory() const
{
	return DronePartInventory.Get();
}

ARaidBoss* ARaidGameState::GetRaidBoss() const
{
	return RaidBoss.Get();
}

void ARaidGameState::SetRaidStateForServer(ERaidState NewRaidState)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Client] SetRaidStateForServer rejected: GameState has no authority"));
		return;
	}

	if (RaidState == NewRaidState)
	{
		return;
	}

	const ERaidState PreviousRaidState = RaidState;
	RaidState = NewRaidState;
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[DR_SUMMARY] RaidState Previous=%s New=%s"),
		ToRaidStateLogString(PreviousRaidState),
		ToRaidStateLogString(RaidState));
}

void ARaidGameState::SetDronePartInventory(ADronePartInventory* InDronePartInventory)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Client] SetDronePartInventory rejected: GameState has no authority"));
		return;
	}

	DronePartInventory = InDronePartInventory;
	BindDronePartInventoryEvents();

	if (DronePartInventory)
	{
		DronePartInventory->ForceNetUpdate();
	}

	ForceNetUpdate();

	UE_LOG(LogTemp, Log, TEXT("[Server] DronePartInventory assigned to GameState: %s"),
		*BuildInventoryReplicationDebugString(DronePartInventory.Get()));
}

void ARaidGameState::SetRaidBossForServer(ARaidBoss* InRaidBoss)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Client] SetRaidBossForServer rejected: GameState has no authority"));
		return;
	}

	RaidBoss = InRaidBoss;
	if (RaidBoss)
	{
		RaidBoss->ForceNetUpdate();
	}

	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[Server] RaidBoss assigned to GameState: %s"),
		RaidBoss ? *RaidBoss->GetName() : TEXT("None"));
}

void ARaidGameState::OnRep_RaidState()
{
	UE_LOG(LogTemp, Log, TEXT("[Client] RaidState replicated -> %s"), ToRaidStateLogString(RaidState));
}

void ARaidGameState::OnRep_DronePartInventory()
{
	UE_LOG(LogTemp, Log, TEXT("[Client] OnRep_DronePartInventory: GameState=%s LocalRole=%s %s"),
		*GetName(),
		ToNetRoleLogString(GetLocalRole()),
		*BuildInventoryReplicationDebugString(DronePartInventory.Get()));

	BindDronePartInventoryEvents();
	NotifyLocalPartSelectUIRefresh(TEXT("GameState.OnRep_DronePartInventory"));
}

void ARaidGameState::OnRep_RaidBoss()
{
	UE_LOG(LogTemp, Log, TEXT("[Client] OnRep_RaidBoss: %s"),
		RaidBoss ? *RaidBoss->GetName() : TEXT("None"));
}

void ARaidGameState::HandleDronePartStocksChanged()
{
	UE_LOG(LogTemp, Verbose, TEXT("[Client] GameState observed PartStocks change: %s"),
		*BuildInventoryReplicationDebugString(DronePartInventory.Get()));
	NotifyLocalPartSelectUIRefresh(TEXT("GameState.PartStocksChanged"));
}

void ARaidGameState::BindDronePartInventoryEvents()
{
	if (!DronePartInventory)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[%s] BindDronePartInventoryEvents skipped: DronePartInventory=None GameState=%s LocalRole=%s"),
			HasAuthority() ? TEXT("Server") : TEXT("Client"),
			*GetName(),
			ToNetRoleLogString(GetLocalRole()));
		return;
	}

	DronePartInventory->OnPartStocksChanged.RemoveDynamic(this, &ARaidGameState::HandleDronePartStocksChanged);
	DronePartInventory->OnPartStocksChanged.AddDynamic(this, &ARaidGameState::HandleDronePartStocksChanged);
	UE_LOG(LogTemp, Verbose, TEXT("[%s] DronePartInventory events bound: %s"),
		HasAuthority() ? TEXT("Server") : TEXT("Client"),
		*BuildInventoryReplicationDebugString(DronePartInventory.Get()));
}

void ARaidGameState::NotifyLocalPartSelectUIRefresh(const TCHAR* Source)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (ARaidPlayerController* RaidPC = Cast<ARaidPlayerController>(It->Get()))
		{
			if (RaidPC->IsLocalController())
			{
				RaidPC->RefreshDronePartInventoryBinding();
				UE_LOG(LogTemp, VeryVerbose, TEXT("[Client] UI Refresh Requested: Player=%s Source=%s"),
					*RaidPC->GetName(),
					Source);
				RaidPC->OnPartSelectUIRefreshRequested.Broadcast();
			}
		}
	}
}
