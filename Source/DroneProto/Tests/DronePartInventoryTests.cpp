#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "Drone.h"
#include "DronePart.h"
#include "Raid/DronePartInventory.h"
#include "Raid/DronePartReturnManager.h"
#include "Raid/RaidBoss.h"
#include "Raid/RaidGameMode.h"
#include "Raid/RaidGameState.h"
#include "Raid/RaidPlayerController.h"

#include "Engine/World.h"
#include "GameFramework/DefaultPawn.h"
#include "TimerManager.h"

namespace
{
struct FDroneSelectionTestContext
{
	UWorld* World = nullptr;
	ARaidGameState* GameState = nullptr;
	ADronePartInventory* Inventory = nullptr;
	ARaidPlayerController* PC = nullptr;
	ADrone* Drone = nullptr;
};

FDroneSelectionTestContext CreateDroneSelectionTestContext(const TCHAR* WorldName)
{
	FDroneSelectionTestContext Context;
	Context.World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
	if (!Context.World)
	{
		return Context;
	}

	Context.GameState = Context.World->SpawnActor<ARaidGameState>();
	Context.Inventory = Context.World->SpawnActor<ADronePartInventory>();
	Context.PC = Context.World->SpawnActor<ARaidPlayerController>();
	Context.Drone = Context.World->SpawnActor<ADrone>();

	if (Context.GameState)
	{
		Context.World->SetGameState(Context.GameState);
		if (Context.Inventory)
		{
			Context.GameState->SetDronePartInventory(Context.Inventory);
		}
	}

	if (Context.PC && Context.Drone)
	{
		Context.PC->Possess(Context.Drone);
	}

	return Context;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDronePartInventoryStockTest,
	"DroneProto.D4.DronePartInventory.StockConsumeReturn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDronePartInventoryStockTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("DronePartInventoryTestWorld")));
	TestNotNull(TEXT("test world is created"), World);
	if (!World)
	{
		return false;
	}

	ADronePartInventory* Inventory = World->SpawnActor<ADronePartInventory>();
	TestNotNull(TEXT("inventory actor is spawned"), Inventory);
	if (!Inventory)
	{
		World->DestroyWorld(false);
		return false;
	}

	TestTrue(TEXT("inventory actor replicates for GameState pointer delivery"), Inventory->GetIsReplicated());
	TestTrue(TEXT("inventory actor is always relevant so clients receive the referenced actor"), Inventory->bAlwaysRelevant);
	TestEqual(TEXT("inventory actor does not start dormant"), static_cast<uint8>(Inventory->NetDormancy), static_cast<uint8>(DORM_Never));

	const FName CoreZenith = ADronePartInventory::GetCoreZenithPartID();
	const FName PulseLaser = ADronePartInventory::GetPulseLaserPartID();
	const FName FractureBurst = ADronePartInventory::GetFractureBurstPartID();

	TestEqual(TEXT("Zenith Core uses the design PartID"), CoreZenith, FName(TEXT("CORE_001")));
	TestEqual(TEXT("Pulse Laser uses the design PartID"), PulseLaser, FName(TEXT("WEAPON_001")));

	EDronePartType PartType = EDronePartType::Weapon;
	TestTrue(TEXT("Zenith Core has a registered part type"), Inventory->GetPartType(CoreZenith, PartType));
	TestEqual(TEXT("Zenith Core is a Core part"), static_cast<uint8>(PartType), static_cast<uint8>(EDronePartType::Core));
	TestEqual(TEXT("Zenith Core starts with design max/current stock"), Inventory->GetCurrentCount(CoreZenith), 5);
	TestEqual(TEXT("Pulse Laser starts with design max/current stock"), Inventory->GetCurrentCount(PulseLaser), 11);
	TestEqual(TEXT("Fracture Burst starts with design max/current stock"), Inventory->GetCurrentCount(FractureBurst), 10);

	TestTrue(TEXT("Zenith Core can be consumed while available"), Inventory->TryConsumePart(CoreZenith));
	TestEqual(TEXT("Zenith Core count decreases after consume"), Inventory->GetCurrentCount(CoreZenith), 4);

	Inventory->ReturnPart(CoreZenith);
	TestEqual(TEXT("Zenith Core count returns after cancellation"), Inventory->GetCurrentCount(CoreZenith), 5);

	Inventory->ReturnPart(CoreZenith);
	TestEqual(TEXT("Zenith Core count does not exceed max"), Inventory->GetCurrentCount(CoreZenith), 5);

	TestFalse(TEXT("unknown part cannot be consumed"), Inventory->TryConsumePart(TEXT("UNKNOWN_PART")));
	TestEqual(TEXT("unknown part count is zero"), Inventory->GetCurrentCount(TEXT("UNKNOWN_PART")), 0);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDronePartReturnManagerTest,
	"DroneProto.D5.DronePartReturnManager.ReturnAndReplace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDronePartReturnManagerTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("DronePartReturnManagerTestWorld")));
	TestNotNull(TEXT("test world is created"), World);
	if (!World)
	{
		return false;
	}

	ADronePartInventory* Inventory = World->SpawnActor<ADronePartInventory>();
	ARaidPlayerController* PC = World->SpawnActor<ARaidPlayerController>();
	UDronePartReturnManager* ReturnManager = NewObject<UDronePartReturnManager>();

	TestNotNull(TEXT("inventory actor is spawned"), Inventory);
	TestNotNull(TEXT("player controller is spawned"), PC);
	TestNotNull(TEXT("return manager is created"), ReturnManager);
	if (!Inventory || !PC || !ReturnManager)
	{
		World->DestroyWorld(false);
		return false;
	}

	ReturnManager->Initialize(Inventory);

	const FName CoreZenith = ADronePartInventory::GetCoreZenithPartID();
	const FName CoreBooster = ADronePartInventory::GetCoreBoosterPartID();

	TestTrue(TEXT("initial selection consumes Zenith Core"), Inventory->TryConsumePart(CoreZenith));
	PC->SetSelectedPartIDForSlotForServer(EPartSlot::Core, CoreZenith);
	TestEqual(TEXT("Zenith Core count decreases after selection"), Inventory->GetCurrentCount(CoreZenith), 4);

	TestTrue(TEXT("cancel return succeeds"), ReturnManager->ReturnSingleSelectedPart(PC, EPartSlot::Core, EDronePartReturnReason::Cancel));
	TestEqual(TEXT("Zenith Core returns to stock after cancel"), Inventory->GetCurrentCount(CoreZenith), 5);
	TestEqual(TEXT("selected slot is cleared after cancel"), PC->GetSelectedPartIDBySlot(EPartSlot::Core), NAME_None);

	TestFalse(TEXT("second cancel is skipped because slot is already empty"),
		ReturnManager->ReturnSingleSelectedPart(PC, EPartSlot::Core, EDronePartReturnReason::Cancel));
	TestEqual(TEXT("Zenith Core does not exceed max after duplicate cancel"), Inventory->GetCurrentCount(CoreZenith), 5);

	TestTrue(TEXT("Zenith Core can be selected again"), Inventory->TryConsumePart(CoreZenith));
	PC->SetSelectedPartIDForSlotForServer(EPartSlot::Core, CoreZenith);
	TestEqual(TEXT("Zenith Core count decreases before replace"), Inventory->GetCurrentCount(CoreZenith), 4);

	TestTrue(TEXT("new part is available for replace"), Inventory->IsPartAvailable(CoreBooster));
	TestTrue(TEXT("successful replace first consumes new part"), Inventory->TryConsumePart(CoreBooster));
	TestTrue(TEXT("successful replace returns old part"), ReturnManager->ReturnSingleSelectedPart(PC, EPartSlot::Core, EDronePartReturnReason::Replace));
	PC->SetSelectedPartIDForSlotForServer(EPartSlot::Core, CoreBooster);
	TestEqual(TEXT("old part stock is restored after replace"), Inventory->GetCurrentCount(CoreZenith), 5);
	TestEqual(TEXT("new part stock is consumed after replace"), Inventory->GetCurrentCount(CoreBooster), 5);
	TestEqual(TEXT("slot now points at new part"), PC->GetSelectedPartIDBySlot(EPartSlot::Core), CoreBooster);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDronePartSelectUIGlueTest,
	"DroneProto.D5.DronePartSelectUI.BlueprintGlue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDronePartSelectUIGlueTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("DronePartSelectUIGlueTestWorld")));
	TestNotNull(TEXT("test world is created"), World);
	if (!World)
	{
		return false;
	}

	ARaidPlayerController* PC = World->SpawnActor<ARaidPlayerController>();
	TestNotNull(TEXT("player controller is spawned"), PC);
	if (!PC)
	{
		World->DestroyWorld(false);
		return false;
	}

	const TArray<FName> CorePartIDs = PC->GetCorePartIDs();
	const TArray<FName> WeaponPartIDs = PC->GetWeaponPartIDs();

	TestEqual(TEXT("core candidates are exposed for the UMG arrows"), CorePartIDs.Num(), 3);
	TestEqual(TEXT("weapon candidates are exposed for both weapon slots"), WeaponPartIDs.Num(), 3);
	TestTrue(TEXT("core slot returns core candidates"), PC->GetAvailablePartIDsForSlot(EDronePartSlot::Core).Contains(ADronePartInventory::GetCoreZenithPartID()));
	TestTrue(TEXT("left weapon slot returns weapon candidates"), PC->GetAvailablePartIDsForSlot(EDronePartSlot::LeftWeapon).Contains(ADronePartInventory::GetPulseLaserPartID()));
	TestTrue(TEXT("right weapon slot returns weapon candidates"), PC->GetAvailablePartIDsForSlot(EDronePartSlot::RightWeapon).Contains(ADronePartInventory::GetVectorCannonPartID()));

	TestFalse(TEXT("empty slot reports no selected part"), PC->HasSelectedPartForSlot(EDronePartSlot::Core));
	PC->SetSelectedPartIDForSlotForServer(EPartSlot::Core, ADronePartInventory::GetCoreZenithPartID());
	TestTrue(TEXT("selected slot reports a selected part"), PC->HasSelectedPartForSlot(EDronePartSlot::Core));
	TestEqual(TEXT("UMG getter returns selected part"), PC->GetSelectedPartForSlot(EDronePartSlot::Core), ADronePartInventory::GetCoreZenithPartID());

	TestFalse(TEXT("known part display name is not empty"), PC->GetPartDisplayName(ADronePartInventory::GetCoreZenithPartID()).IsEmpty());
	TestFalse(TEXT("known part description is not empty"), PC->GetPartDescription(ADronePartInventory::GetPulseLaserPartID()).IsEmpty());
	TestEqual(TEXT("unknown part current count is zero without inventory"), PC->GetPartCurrentCount(TEXT("UNKNOWN_PART")), 0);
	TestEqual(TEXT("unknown part max count is zero without inventory"), PC->GetPartMaxCount(TEXT("UNKNOWN_PART")), 0);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRaidGameModeDroneSpawnCollisionTest,
	"DroneProto.D5.RaidGameMode.SpawnsDroneWhenPlayerStartIsOccupied",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRaidGameModeDroneSpawnCollisionTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("RaidGameModeDroneSpawnCollisionTestWorld")));
	TestNotNull(TEXT("test world is created"), World);
	if (!World)
	{
		return false;
	}

	ARaidGameMode* GameMode = World->SpawnActor<ARaidGameMode>();
	ARaidPlayerController* FirstPC = World->SpawnActor<ARaidPlayerController>();
	ARaidPlayerController* SecondPC = World->SpawnActor<ARaidPlayerController>();
	UClass* DronePawnClass = LoadClass<APawn>(nullptr, TEXT("/Game/BP_Drone.BP_Drone_C"));
	ADefaultPawn* DefaultPawnCDO = GetMutableDefault<ADefaultPawn>();

	TestNotNull(TEXT("raid game mode is spawned"), GameMode);
	TestNotNull(TEXT("first player controller is spawned"), FirstPC);
	TestNotNull(TEXT("second player controller is spawned"), SecondPC);
	TestNotNull(TEXT("BP_Drone pawn class is loaded"), DronePawnClass);
	TestNotNull(TEXT("default pawn CDO is available"), DefaultPawnCDO);
	if (!GameMode || !FirstPC || !SecondPC || !DronePawnClass || !DefaultPawnCDO)
	{
		World->DestroyWorld(false);
		return false;
	}

	TestTrue(TEXT("BP_Drone remains an ADrone class for TestMap PIE"), DronePawnClass->IsChildOf(ADrone::StaticClass()));

	const ESpawnActorCollisionHandlingMethod OriginalSpawnCollisionHandling = DefaultPawnCDO->SpawnCollisionHandlingMethod;
	DefaultPawnCDO->SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
	GameMode->DefaultPawnClass = ADefaultPawn::StaticClass();

	const FTransform SharedSpawnTransform(
		FRotator::ZeroRotator,
		FVector(-720.0f, 170.0f, 192.0f),
		FVector::OneVector);

	APawn* FirstPawn = GameMode->SpawnDefaultPawnAtTransform(FirstPC, SharedSpawnTransform);
	TestNotNull(TEXT("first pawn spawn succeeds"), FirstPawn);

	APawn* SecondPawn = GameMode->SpawnDefaultPawnAtTransform(SecondPC, SharedSpawnTransform);
	TestNotNull(TEXT("second pawn spawn succeeds even when the spawn transform is occupied"), SecondPawn);

	DefaultPawnCDO->SpawnCollisionHandlingMethod = OriginalSpawnCollisionHandling;

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDronePlayerSelectionStateTest,
	"DroneProto.D5.RaidPlayerController.PlayerSelectionStateSeparatesRaidState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDronePlayerSelectionStateTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("DronePlayerSelectionStateTestWorld")));
	TestNotNull(TEXT("test world is created"), World);
	if (!World)
	{
		return false;
	}

	ARaidGameState* GameState = World->SpawnActor<ARaidGameState>();
	ADronePartInventory* Inventory = World->SpawnActor<ADronePartInventory>();
	ARaidPlayerController* ReadyPC = World->SpawnActor<ARaidPlayerController>();
	ADrone* ReadyDrone = World->SpawnActor<ADrone>();
	ARaidPlayerController* LateJoinPC = World->SpawnActor<ARaidPlayerController>();
	ADrone* LateJoinDrone = World->SpawnActor<ADrone>();

	TestNotNull(TEXT("game state is spawned"), GameState);
	TestNotNull(TEXT("inventory actor is spawned"), Inventory);
	TestNotNull(TEXT("ready player controller is spawned"), ReadyPC);
	TestNotNull(TEXT("ready drone is spawned"), ReadyDrone);
	TestNotNull(TEXT("late join player controller is spawned"), LateJoinPC);
	TestNotNull(TEXT("late join drone is spawned"), LateJoinDrone);
	if (!GameState || !Inventory || !ReadyPC || !ReadyDrone || !LateJoinPC || !LateJoinDrone)
	{
		World->DestroyWorld(false);
		return false;
	}

	World->SetGameState(GameState);
	GameState->SetDronePartInventory(Inventory);
	GameState->SetRaidStateForServer(ERaidState::Battle);

	ReadyPC->Possess(ReadyDrone);
	LateJoinPC->Possess(LateJoinDrone);

	const FName PulseLaser = ADronePartInventory::GetPulseLaserPartID();
	const FName VectorCannon = ADronePartInventory::GetVectorCannonPartID();

	TestEqual(TEXT("ready player starts in Selecting"), ReadyPC->GetPlayerSelectionState(), EPlayerSelectionState::Selecting);
	TestTrue(TEXT("ready player's Pulse selection consumes stock"), Inventory->TryConsumePart(PulseLaser));
	ReadyPC->SetSelectedPartIDForSlotForServer(EPartSlot::LeftWeapon, PulseLaser);
	ReadyPC->Server_RequestReadyForRaid_Implementation();

	TestEqual(TEXT("ready player locks after Ready"), ReadyPC->GetPlayerSelectionState(), EPlayerSelectionState::InBattle);
	TestEqual(TEXT("ready selected weapon moved out of selected slot"), ReadyPC->GetSelectedPartIDBySlot(EPartSlot::LeftWeapon), NAME_None);
	TestEqual(TEXT("ready selected weapon copied to equipped slot"), ReadyPC->GetEquippedPartIDBySlot(EPartSlot::LeftWeapon), PulseLaser);
	TestEqual(TEXT("Ready keeps the global raid in Battle"), GameState->RaidState, ERaidState::Battle);

	const int32 PulseCountAfterReady = Inventory->GetCurrentCount(PulseLaser);
	ReadyPC->Server_RequestCancelPart_Implementation(EPartSlot::LeftWeapon);
	TestEqual(TEXT("locked player cancel does not return equipped stock"), Inventory->GetCurrentCount(PulseLaser), PulseCountAfterReady);
	TestEqual(TEXT("locked player cancel keeps equipped weapon"), ReadyPC->GetEquippedPartIDBySlot(EPartSlot::LeftWeapon), PulseLaser);

	ReadyPC->Server_RequestSelectPart_Implementation(EPartSlot::RightWeapon, VectorCannon);
	TestEqual(TEXT("locked player select does not change selected slot"), ReadyPC->GetSelectedPartIDBySlot(EPartSlot::RightWeapon), NAME_None);
	TestEqual(TEXT("locked player select does not consume new stock"), Inventory->GetCurrentCount(VectorCannon), 11);

	TestEqual(TEXT("late join player remains Selecting while RaidState is Battle"), LateJoinPC->GetPlayerSelectionState(), EPlayerSelectionState::Selecting);
	TestTrue(TEXT("late join player's Vector selection consumes stock"), Inventory->TryConsumePart(VectorCannon));
	LateJoinPC->SetSelectedPartIDForSlotForServer(EPartSlot::RightWeapon, VectorCannon);
	LateJoinPC->Server_RequestReadyForRaid_Implementation();

	TestEqual(TEXT("late join player can Ready while RaidState is Battle"), LateJoinPC->GetPlayerSelectionState(), EPlayerSelectionState::InBattle);
	TestEqual(TEXT("late join selected weapon copied to equipped slot"), LateJoinPC->GetEquippedPartIDBySlot(EPartSlot::RightWeapon), VectorCannon);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneSelectionTimerAutoReadyTest,
	"DroneProto.D5.RaidPlayerController.SelectionTimerAutoReady",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneSelectionTimerAutoReadyTest::RunTest(const FString& Parameters)
{
	FDroneSelectionTestContext EmptyLoadout = CreateDroneSelectionTestContext(TEXT("DroneSelectionTimerEmptyWorld"));
	TestNotNull(TEXT("empty loadout world is created"), EmptyLoadout.World);
	TestNotNull(TEXT("empty loadout player controller is spawned"), EmptyLoadout.PC);
	TestNotNull(TEXT("empty loadout drone is spawned"), EmptyLoadout.Drone);
	if (!EmptyLoadout.World || !EmptyLoadout.PC || !EmptyLoadout.Drone)
	{
		if (EmptyLoadout.World)
		{
			EmptyLoadout.World->DestroyWorld(false);
		}
		return false;
	}

	TestEqual(TEXT("empty loadout starts Selecting"),
		EmptyLoadout.PC->GetCurrentSelectionState(),
		EPlayerSelectionState::Selecting);
	EmptyLoadout.PC->Server_RequestStartSelectionTimer_Implementation();
	TestTrue(TEXT("server selection timer exposes remaining time"),
		EmptyLoadout.PC->GetSelectionRemainingTime() > 0.0f);
	EmptyLoadout.PC->HandleSelectionTimerExpiredForServer();

	TestEqual(TEXT("empty loadout auto ready moves player to InBattle"),
		EmptyLoadout.PC->GetCurrentSelectionState(),
		EPlayerSelectionState::InBattle);
	TestTrue(TEXT("empty loadout reports selection locked after auto ready"),
		EmptyLoadout.PC->IsSelectionLocked());
	TestEqual(TEXT("empty auto ready equips no core"),
		EmptyLoadout.PC->GetEquippedPartIDBySlot(EPartSlot::Core),
		NAME_None);
	TestEqual(TEXT("empty auto ready keeps default attack power"),
		EmptyLoadout.Drone->GetAttackPower(),
		0);
	EmptyLoadout.World->DestroyWorld(false);

	FDroneSelectionTestContext PartialLoadout = CreateDroneSelectionTestContext(TEXT("DroneSelectionTimerPartialWorld"));
	TestNotNull(TEXT("partial loadout world is created"), PartialLoadout.World);
	TestNotNull(TEXT("partial loadout inventory is spawned"), PartialLoadout.Inventory);
	TestNotNull(TEXT("partial loadout player controller is spawned"), PartialLoadout.PC);
	TestNotNull(TEXT("partial loadout drone is spawned"), PartialLoadout.Drone);
	if (!PartialLoadout.World || !PartialLoadout.Inventory || !PartialLoadout.PC || !PartialLoadout.Drone)
	{
		if (PartialLoadout.World)
		{
			PartialLoadout.World->DestroyWorld(false);
		}
		return false;
	}

	const FName PulseLaser = ADronePartInventory::GetPulseLaserPartID();
	TestTrue(TEXT("partial loadout consumes selected weapon before timer"),
		PartialLoadout.Inventory->TryConsumePart(PulseLaser));
	PartialLoadout.PC->SetSelectedPartIDForSlotForServer(EPartSlot::LeftWeapon, PulseLaser);
	PartialLoadout.PC->Server_RequestStartSelectionTimer_Implementation();
	PartialLoadout.PC->HandleSelectionTimerExpiredForServer();

	TestEqual(TEXT("partial loadout auto ready moves player to InBattle"),
		PartialLoadout.PC->GetCurrentSelectionState(),
		EPlayerSelectionState::InBattle);
	TestEqual(TEXT("partial auto ready copies selected weapon to equipped slot"),
		PartialLoadout.PC->GetEquippedPartIDBySlot(EPartSlot::LeftWeapon),
		PulseLaser);
	TestEqual(TEXT("partial auto ready clears selected weapon slot"),
		PartialLoadout.PC->GetSelectedPartIDBySlot(EPartSlot::LeftWeapon),
		NAME_None);
	TestEqual(TEXT("partial auto ready applies weapon attack power"),
		PartialLoadout.Drone->GetAttackPower(),
		8);

	PartialLoadout.World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneSelectionTimerDuplicateReadyTest,
	"DroneProto.D5.RaidPlayerController.SelectionTimerManualReadyPreventsDuplicate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneSelectionTimerDuplicateReadyTest::RunTest(const FString& Parameters)
{
	FDroneSelectionTestContext Context = CreateDroneSelectionTestContext(TEXT("DroneSelectionTimerDuplicateWorld"));
	TestNotNull(TEXT("test world is created"), Context.World);
	TestNotNull(TEXT("inventory actor is spawned"), Context.Inventory);
	TestNotNull(TEXT("player controller is spawned"), Context.PC);
	TestNotNull(TEXT("drone is spawned"), Context.Drone);
	if (!Context.World || !Context.Inventory || !Context.PC || !Context.Drone)
	{
		if (Context.World)
		{
			Context.World->DestroyWorld(false);
		}
		return false;
	}

	const FName PulseLaser = ADronePartInventory::GetPulseLaserPartID();
	TestTrue(TEXT("manual ready test consumes selected weapon"),
		Context.Inventory->TryConsumePart(PulseLaser));
	Context.PC->SetSelectedPartIDForSlotForServer(EPartSlot::LeftWeapon, PulseLaser);
	Context.PC->Server_RequestStartSelectionTimer_Implementation();
	Context.PC->Server_RequestReadyForRaid_Implementation();

	const int32 PulseCountAfterManualReady = Context.Inventory->GetCurrentCount(PulseLaser);
	TestEqual(TEXT("manual ready locks selection"),
		Context.PC->GetCurrentSelectionState(),
		EPlayerSelectionState::InBattle);
	TestEqual(TEXT("manual ready equips selected weapon"),
		Context.PC->GetEquippedPartIDBySlot(EPartSlot::LeftWeapon),
		PulseLaser);
	TestEqual(TEXT("manual ready stops remaining timer display"),
		Context.PC->GetSelectionRemainingTime(),
		0.0f);

	Context.PC->HandleSelectionTimerExpiredForServer();
	TestEqual(TEXT("stale timer expiry does not change consumed stock"),
		Context.Inventory->GetCurrentCount(PulseLaser),
		PulseCountAfterManualReady);
	TestEqual(TEXT("stale timer expiry does not clear equipped slot"),
		Context.PC->GetEquippedPartIDBySlot(EPartSlot::LeftWeapon),
		PulseLaser);

	Context.PC->Server_RequestReadyForRaid_Implementation();
	TestEqual(TEXT("locked manual ready retry does not change stock"),
		Context.Inventory->GetCurrentCount(PulseLaser),
		PulseCountAfterManualReady);
	TestEqual(TEXT("locked manual ready retry keeps equipped slot"),
		Context.PC->GetEquippedPartIDBySlot(EPartSlot::LeftWeapon),
		PulseLaser);

	Context.World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneSelectionLockedRequestRegressionTest,
	"DroneProto.D5.RaidPlayerController.LockedRequestsDoNotChangeStock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneSelectionLockedRequestRegressionTest::RunTest(const FString& Parameters)
{
	FDroneSelectionTestContext Context = CreateDroneSelectionTestContext(TEXT("DroneSelectionLockedRegressionWorld"));
	TestNotNull(TEXT("test world is created"), Context.World);
	TestNotNull(TEXT("inventory actor is spawned"), Context.Inventory);
	TestNotNull(TEXT("player controller is spawned"), Context.PC);
	TestNotNull(TEXT("drone is spawned"), Context.Drone);
	if (!Context.World || !Context.Inventory || !Context.PC || !Context.Drone)
	{
		if (Context.World)
		{
			Context.World->DestroyWorld(false);
		}
		return false;
	}

	const FName PulseLaser = ADronePartInventory::GetPulseLaserPartID();
	const FName VectorCannon = ADronePartInventory::GetVectorCannonPartID();
	TestTrue(TEXT("locked request test consumes selected weapon"),
		Context.Inventory->TryConsumePart(PulseLaser));
	Context.PC->SetSelectedPartIDForSlotForServer(EPartSlot::LeftWeapon, PulseLaser);
	Context.PC->Server_RequestReadyForRaid_Implementation();

	const int32 PulseCountAfterReady = Context.Inventory->GetCurrentCount(PulseLaser);
	const int32 VectorCountBeforeLockedSelect = Context.Inventory->GetCurrentCount(VectorCannon);

	Context.PC->Server_RequestSelectPart_Implementation(EPartSlot::RightWeapon, VectorCannon);
	Context.PC->Server_RequestCancelPart_Implementation(EPartSlot::LeftWeapon);
	Context.PC->Server_RequestReadyForRaid_Implementation();
	Context.PC->Server_RequestStartSelectionTimer_Implementation();

	TestEqual(TEXT("locked select does not consume another weapon"),
		Context.Inventory->GetCurrentCount(VectorCannon),
		VectorCountBeforeLockedSelect);
	TestEqual(TEXT("locked cancel does not return equipped weapon"),
		Context.Inventory->GetCurrentCount(PulseLaser),
		PulseCountAfterReady);
	TestEqual(TEXT("locked requests keep equipped weapon"),
		Context.PC->GetEquippedPartIDBySlot(EPartSlot::LeftWeapon),
		PulseLaser);
	TestEqual(TEXT("locked requests leave selected right weapon empty"),
		Context.PC->GetSelectedPartIDBySlot(EPartSlot::RightWeapon),
		NAME_None);

	Context.World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneSelectionReturnPathRegressionTest,
	"DroneProto.D5.DronePartReturnManager.SelectionAndBattleReturnPaths",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneSelectionReturnPathRegressionTest::RunTest(const FString& Parameters)
{
	FDroneSelectionTestContext Context = CreateDroneSelectionTestContext(TEXT("DroneSelectionReturnRegressionWorld"));
	UDronePartReturnManager* ReturnManager = NewObject<UDronePartReturnManager>();
	TestNotNull(TEXT("test world is created"), Context.World);
	TestNotNull(TEXT("inventory actor is spawned"), Context.Inventory);
	TestNotNull(TEXT("player controller is spawned"), Context.PC);
	TestNotNull(TEXT("return manager is created"), ReturnManager);
	if (!Context.World || !Context.Inventory || !Context.PC || !ReturnManager)
	{
		if (Context.World)
		{
			Context.World->DestroyWorld(false);
		}
		return false;
	}

	ReturnManager->Initialize(Context.Inventory);

	const FName PulseLaser = ADronePartInventory::GetPulseLaserPartID();
	TestTrue(TEXT("selecting logout path consumes selected part"),
		Context.Inventory->TryConsumePart(PulseLaser));
	Context.PC->SetSelectedPartIDForSlotForServer(EPartSlot::LeftWeapon, PulseLaser);
	TestTrue(TEXT("selecting logout path returns selected parts"),
		ReturnManager->ReturnSelectedParts(Context.PC, EDronePartReturnReason::Disconnect));
	TestEqual(TEXT("selecting logout path restores selected part count"),
		Context.Inventory->GetCurrentCount(PulseLaser),
		Context.Inventory->GetMaxCount(PulseLaser));
	TestEqual(TEXT("selecting logout path clears selected slot"),
		Context.PC->GetSelectedPartIDBySlot(EPartSlot::LeftWeapon),
		NAME_None);

	TestTrue(TEXT("battle logout path consumes equipped part"),
		Context.Inventory->TryConsumePart(PulseLaser));
	Context.PC->SetEquippedPartIDForSlotForServer(EPartSlot::LeftWeapon, PulseLaser);
	TestTrue(TEXT("battle logout path returns equipped parts"),
		ReturnManager->ReturnEquippedParts(Context.PC, EDronePartReturnReason::Disconnect));
	TestEqual(TEXT("battle logout path restores equipped part count"),
		Context.Inventory->GetCurrentCount(PulseLaser),
		Context.Inventory->GetMaxCount(PulseLaser));
	TestEqual(TEXT("battle logout path clears equipped slot"),
		Context.PC->GetEquippedPartIDBySlot(EPartSlot::LeftWeapon),
		NAME_None);
	TestFalse(TEXT("second battle logout return is skipped after slot clear"),
		ReturnManager->ReturnEquippedParts(Context.PC, EDronePartReturnReason::Disconnect));
	TestEqual(TEXT("duplicate equipped return does not exceed max count"),
		Context.Inventory->GetCurrentCount(PulseLaser),
		Context.Inventory->GetMaxCount(PulseLaser));

	Context.World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneApplyLoadoutTest,
	"DroneProto.D5.Drone.ApplySelectedLoadout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneApplyLoadoutTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("DroneApplyLoadoutTestWorld")));
	TestNotNull(TEXT("test world is created"), World);
	if (!World)
	{
		return false;
	}

	ADrone* Drone = World->SpawnActor<ADrone>();
	TestNotNull(TEXT("drone is spawned"), Drone);
	if (!Drone)
	{
		World->DestroyWorld(false);
		return false;
	}

	TestTrue(TEXT("loadout accepts the default drone with no parts"),
		Drone->ApplyLoadout(NAME_None, NAME_None, NAME_None));
	TestEqual(TEXT("default drone keeps base max health"), Drone->GetMaxHealth(), 100);
	TestEqual(TEXT("default drone has zero displayed attack power"), Drone->GetAttackPower(), 0);

	TestTrue(TEXT("loadout accepts core-only selection"),
		Drone->ApplyLoadout(ADronePartInventory::GetCoreZenithPartID(), NAME_None, NAME_None));

	TestTrue(TEXT("loadout accepts one weapon without a core"),
		Drone->ApplyLoadout(NAME_None, ADronePartInventory::GetPulseLaserPartID(), NAME_None));

	TestTrue(TEXT("loadout accepts selected core and weapons"),
		Drone->ApplyLoadout(
			ADronePartInventory::GetCoreZenithPartID(),
			ADronePartInventory::GetPulseLaserPartID(),
			ADronePartInventory::GetVectorCannonPartID()));
	TestEqual(TEXT("loadout keeps base max health when no health part stat exists"), Drone->GetMaxHealth(), 100);
	TestEqual(TEXT("loadout displays total base weapon damage"), Drone->GetAttackPower(), 15);
	TestEqual(TEXT("loadout fills health after drafting"), Drone->GetHealth(), 100);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneAttackBossTest,
	"DroneProto.D5.Drone.ServerAttackBossVerticalSlice",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneAttackBossTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("DroneAttackBossTestWorld")));
	TestNotNull(TEXT("test world is created"), World);
	if (!World)
	{
		return false;
	}

	ADrone* Drone = World->SpawnActor<ADrone>();
	ARaidBoss* Boss = World->SpawnActor<ARaidBoss>();
	ARaidPlayerController* PC = World->SpawnActor<ARaidPlayerController>();
	TestNotNull(TEXT("drone is spawned"), Drone);
	TestNotNull(TEXT("boss is spawned"), Boss);
	TestNotNull(TEXT("player controller is spawned"), PC);
	if (!Drone || !Boss || !PC)
	{
		World->DestroyWorld(false);
		return false;
	}

	PC->Possess(Drone);
	TestEqual(TEXT("attack test starts in Selecting"),
		PC->GetCurrentSelectionState(),
		EPlayerSelectionState::Selecting);

	TestTrue(TEXT("selecting drone can have a loadout but still cannot attack"),
		Drone->ApplyLoadout(NAME_None, ADronePartInventory::GetPulseLaserPartID(), NAME_None));
	const float HPBeforeSelectingAttack = Boss->GetCurrentHP();
	Drone->RequestAttackBoss();
	TestTrue(TEXT("Selecting player attack is ignored before boss damage"),
		FMath::IsNearlyEqual(Boss->GetCurrentHP(), HPBeforeSelectingAttack, 0.01f));

	PC->Server_RequestReadyForRaid_Implementation();
	TestEqual(TEXT("ready player moves to InBattle before attacks"),
		PC->GetCurrentSelectionState(),
		EPlayerSelectionState::InBattle);

	TestTrue(TEXT("empty loadout can attack for zero damage"), Drone->ApplyLoadout(NAME_None, NAME_None, NAME_None));
	const float InitialHP = Boss->GetCurrentHP();
	Drone->RequestAttackBoss();
	TestTrue(TEXT("empty weapon slots leave boss HP unchanged"),
		FMath::IsNearlyEqual(Boss->GetCurrentHP(), InitialHP, 0.01f));

	TestTrue(TEXT("Pulse plus Fracture loadout applies both weapon damages"),
		Drone->ApplyLoadout(NAME_None, ADronePartInventory::GetPulseLaserPartID(), ADronePartInventory::GetFractureBurstPartID()));
	Drone->RequestAttackBoss();
	TestTrue(TEXT("first attack deals Pulse 8 plus Fracture 11"),
		FMath::IsNearlyEqual(Boss->GetCurrentHP(), InitialHP - 19.0f, 0.01f));
	Drone->RequestAttackBoss();
	TestTrue(TEXT("second attack deals another 19 damage"),
		FMath::IsNearlyEqual(Boss->GetCurrentHP(), InitialHP - 38.0f, 0.01f));
	Drone->RequestAttackBoss();
	TestTrue(TEXT("third Pulse attack uses strong 18 plus Fracture 11"),
		FMath::IsNearlyEqual(Boss->GetCurrentHP(), InitialHP - 67.0f, 0.01f));

	TestTrue(TEXT("Zenith core applies HP-ratio attack bonus"),
		Drone->ApplyLoadout(ADronePartInventory::GetCoreZenithPartID(), ADronePartInventory::GetPulseLaserPartID(), NAME_None));
	const float HPBeforeZenithAttack = Boss->GetCurrentHP();
	Drone->RequestAttackBoss();
	TestTrue(TEXT("full HP Zenith Pulse attack deals 8 * 1.2 damage"),
		FMath::IsNearlyEqual(Boss->GetCurrentHP(), HPBeforeZenithAttack - 9.6f, 0.01f));

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneRaidSummaryLogSourceTest,
	"DroneProto.D5.ManualSummaryLogs.SourceMarkers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneRaidSummaryLogSourceTest::RunTest(const FString& Parameters)
{
	const FString SourceRoot = FPaths::ProjectDir() / TEXT("Source/DroneProto");

	auto LoadSourceFile = [this, SourceRoot](const TCHAR* RelativePath, FString& OutContents) -> bool
	{
		const FString FullPath = SourceRoot / RelativePath;
		const bool bLoaded = FFileHelper::LoadFileToString(OutContents, *FullPath);
		TestTrue(FString::Printf(TEXT("%s loads"), RelativePath), bLoaded);
		return bLoaded;
	};

	FString RaidGameModeSource;
	FString RaidPlayerControllerSource;
	FString DronePartReturnManagerSource;
	FString DronePartSelectWidgetSource;
	FString DroneSource;

	if (!LoadSourceFile(TEXT("Raid/RaidGameMode.cpp"), RaidGameModeSource)
		|| !LoadSourceFile(TEXT("Raid/RaidPlayerController.cpp"), RaidPlayerControllerSource)
		|| !LoadSourceFile(TEXT("Raid/DronePartReturnManager.cpp"), DronePartReturnManagerSource)
		|| !LoadSourceFile(TEXT("Raid/DronePartSelectWidget.cpp"), DronePartSelectWidgetSource)
		|| !LoadSourceFile(TEXT("Drone.cpp"), DroneSource))
	{
		return false;
	}

	TestTrue(TEXT("spawn summary log marker exists"),
		RaidGameModeSource.Contains(TEXT("[DR_SUMMARY] Spawn PC=")));
	TestTrue(TEXT("select summary log marker exists"),
		RaidPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] Select PC=")));
	TestTrue(TEXT("cancel summary log marker exists"),
		RaidPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] Cancel PC=")));
	TestTrue(TEXT("ready summary log marker exists"),
		RaidPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] Ready PC=")));
	TestTrue(TEXT("return summary log marker exists"),
		DronePartReturnManagerSource.Contains(TEXT("[DR_SUMMARY] Return PC=")));
	TestTrue(TEXT("attack summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] Attack PC=")));
	TestTrue(TEXT("attack ignored summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] AttackIgnored PC=")));
	TestTrue(TEXT("attack path checks PlayerSelectionState"),
		DroneSource.Contains(TEXT("GetPlayerSelectionState()")));
	TestTrue(TEXT("selection timer start summary log marker exists"),
		RaidPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] SelectTimerStart PC=")));
	TestTrue(TEXT("selection timer stop summary log marker exists"),
		RaidPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] SelectTimerStop PC=")));
	TestTrue(TEXT("auto ready summary log marker exists"),
		RaidPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] AutoReady PC=")));
	TestTrue(TEXT("ui refresh summary log marker exists"),
		RaidPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] UIRefresh PC=")));
	TestTrue(TEXT("death return summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] DeathReturn PC=")));
	TestTrue(TEXT("timer text refresh interval marker exists"),
		DronePartSelectWidgetSource.Contains(TEXT("TimerTextRefreshIntervalSeconds")));
	TestTrue(TEXT("timer text refresh starts from the widget"),
		DronePartSelectWidgetSource.Contains(TEXT("StartTimerTextRefresh")));
	TestTrue(TEXT("timer text refresh stops from the widget"),
		DronePartSelectWidgetSource.Contains(TEXT("StopTimerTextRefresh")));
	TestTrue(TEXT("timer text refresh reads the computed remaining time"),
		DronePartSelectWidgetSource.Contains(TEXT("GetSelectionRemainingTime()")));
	TestTrue(TEXT("remaining timer clamps to the selection duration"),
		RaidPlayerControllerSource.Contains(TEXT("SelectionDurationSeconds")));
	TestTrue(TEXT("remaining timer uses an upper clamp"),
		RaidPlayerControllerSource.Contains(TEXT("FMath::Clamp(")));
	TestTrue(TEXT("summary logs use stable controller naming"),
		RaidPlayerControllerSource.Contains(TEXT("BuildStableControllerLogString")));
	TestTrue(TEXT("drone logs use stable controller naming"),
		DroneSource.Contains(TEXT("BuildStableControllerLogString")));
	TestTrue(TEXT("return logs use stable controller naming"),
		DronePartReturnManagerSource.Contains(TEXT("BuildStableControllerLogString")));
	TestTrue(TEXT("in battle UI summary reports equipped parts"),
		RaidPlayerControllerSource.Contains(TEXT("SummaryCorePartID")));

	return true;
}

#endif
