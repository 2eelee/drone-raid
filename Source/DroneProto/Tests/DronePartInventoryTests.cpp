#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "Drone.h"
#include "DronePart.h"
#include "Raid/DronePartInventory.h"
#include "Raid/DroneCombatTypes.h"
#include "Raid/DronePartReturnManager.h"
#include "Raid/DroneReportWidget.h"
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

void DestroyDroneSelectionTestContext(FDroneSelectionTestContext& Context)
{
	if (Context.World)
	{
		Context.World->DestroyWorld(false);
	}
	Context = FDroneSelectionTestContext();
}

FDroneSelectionTestContext CreateDroneReturnTestContext(
	const TCHAR* WorldName,
	UDronePartReturnManager*& OutReturnManager)
{
	FDroneSelectionTestContext Context = CreateDroneSelectionTestContext(WorldName);
	OutReturnManager = NewObject<UDronePartReturnManager>();

	if (OutReturnManager && Context.Inventory)
	{
		OutReturnManager->Initialize(Context.Inventory);
	}

	if (Context.PC)
	{
		Context.PC->SetDronePartReturnManagerForTest(OutReturnManager);
	}

	return Context;
}

bool PrepareBattleAttackTest(
	FAutomationTestBase& Test,
	FDroneSelectionTestContext& Context,
	ARaidBoss*& OutBoss,
	const TCHAR* ContextLabel)
{
	Test.TestNotNull(FString::Printf(TEXT("%s world is created"), ContextLabel), Context.World);
	Test.TestNotNull(FString::Printf(TEXT("%s game state is spawned"), ContextLabel), Context.GameState);
	Test.TestNotNull(FString::Printf(TEXT("%s player controller is spawned"), ContextLabel), Context.PC);
	Test.TestNotNull(FString::Printf(TEXT("%s drone is spawned"), ContextLabel), Context.Drone);
	if (!Context.World || !Context.GameState || !Context.PC || !Context.Drone)
	{
		return false;
	}

	OutBoss = Context.World->SpawnActor<ARaidBoss>();
	Test.TestNotNull(FString::Printf(TEXT("%s boss is spawned"), ContextLabel), OutBoss);
	if (!OutBoss)
	{
		return false;
	}

	Context.GameState->SetRaidBossForServer(OutBoss);
	Context.PC->Server_RequestReadyForRaid_Implementation();
	Test.TestEqual(FString::Printf(TEXT("%s player is InBattle"), ContextLabel),
		Context.PC->GetCurrentSelectionState(),
		EPlayerSelectionState::InBattle);
	return true;
}

float AttackBossAndMeasureDamage(ADrone* Drone, const ARaidBoss* Boss)
{
	if (!Drone || !Boss)
	{
		return 0.0f;
	}

	const float HPBeforeAttack = Boss->GetCurrentHP();
	Drone->RequestAttackBoss();
	return HPBeforeAttack - Boss->GetCurrentHP();
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneCombatFormulaTest,
	"DroneProto.D9.DroneCombat.Formulas",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneCombatFormulaTest::RunTest(const FString& Parameters)
{
	FDroneWeaponCalculationInput PulseInput;
	PulseInput.WeaponType = EDroneCombatWeaponType::PulseLaser;
	PulseInput.PulseAttackCount = 2;
	const FDroneWeaponCalculationResult PulseResult = FDroneCombatRules::CalculateWeaponDamage(PulseInput);
	TestTrue(TEXT("third Pulse hit uses strong damage"),
		FMath::IsNearlyEqual(PulseResult.WeaponDamage, 18.0f, 0.01f));
	TestEqual(TEXT("third Pulse hit resets the slot-local counter"), PulseResult.PulseAttackCount, 0);

	FDroneWeaponCalculationInput FractureInput;
	FractureInput.WeaponType = EDroneCombatWeaponType::FractureBurst;
	const FDroneWeaponCalculationResult FractureResult = FDroneCombatRules::CalculateWeaponDamage(FractureInput);
	TestTrue(TEXT("Fracture Burst calculates 5 + 3 * 2 damage"),
		FMath::IsNearlyEqual(FractureResult.WeaponDamage, 11.0f, 0.01f));
	TestEqual(TEXT("Fracture Burst exposes four total hit events for reporting"), FractureResult.HitCount, 4);

	FDroneWeaponCalculationInput VectorInput;
	VectorInput.WeaponType = EDroneCombatWeaponType::VectorCannon;
	VectorInput.VectorAccumulatedMoveDistanceMeters = 43.0f;
	const FDroneWeaponCalculationResult VectorResult = FDroneCombatRules::CalculateWeaponDamage(VectorInput);
	TestTrue(TEXT("Vector Cannon caps movement bonus at 8"),
		FMath::IsNearlyEqual(VectorResult.WeaponDamage, 15.0f, 0.01f));
	TestTrue(TEXT("Vector Cannon requests attack-time vector distance reset"), VectorResult.bResetVectorDistance);

	FDroneCoreCalculationInput ZenithInput;
	ZenithInput.CoreType = EDroneCombatCoreType::Zenith;
	ZenithInput.CurrentHP = 50.0f;
	ZenithInput.MaxHP = 100.0f;
	const FDroneCoreCalculationResult ZenithResult = FDroneCombatRules::CalculateCoreBonus(ZenithInput);
	TestTrue(TEXT("Zenith at 50 percent HP gives 1.10 bonus modifier"),
		FMath::IsNearlyEqual(ZenithResult.CoreBonusAttackModifier, 1.10f, 0.01f));

	FDroneCoreCalculationInput BoosterInput;
	BoosterInput.CoreType = EDroneCombatCoreType::Booster;
	BoosterInput.AccumulatedMoveDistanceMeters = 210.0f;
	const FDroneCoreCalculationResult BoosterResult = FDroneCombatRules::CalculateCoreBonus(BoosterInput);
	TestTrue(TEXT("Booster speed bonus caps at 0.30"),
		FMath::IsNearlyEqual(BoosterResult.MoveSpeedBonus, 0.30f, 0.01f));
	TestTrue(TEXT("Booster attack bonus is half of speed bonus"),
		FMath::IsNearlyEqual(BoosterResult.CoreBonusAttackModifier, 1.15f, 0.01f));

	TestTrue(TEXT("Drain heal is 12 percent of dealt damage"),
		FMath::IsNearlyEqual(FDroneCombatRules::CalculateDrainHeal(11.0f), 1.32f, 0.01f));
	TestTrue(TEXT("Drain heal caps once per attack input"),
		FMath::IsNearlyEqual(FDroneCombatRules::CalculateDrainHeal(100.0f), 3.0f, 0.01f));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneReportFormulaTest,
	"DroneProto.D10.DroneReport.Formulas",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneReportFormulaTest::RunTest(const FString& Parameters)
{
	FDroneCombatRecord CappedBaseRecord;
	CappedBaseRecord.SurvivalTime = 240.0f;
	CappedBaseRecord.BossDamage = 80.0f;
	CappedBaseRecord.BossMaxHP = 1000.0f;
	CappedBaseRecord.BossHPOnJoin = 1000.0f;
	CappedBaseRecord.MoveDistance = 600.0f;
	CappedBaseRecord.HealAmount = 60.0f;
	CappedBaseRecord.DamageTakenCount = 1;
	CappedBaseRecord.CombatStartTime = 0.0f;
	CappedBaseRecord.CombatEndTime = 20.0f;
	CappedBaseRecord.bIsAliveAtReport = true;
	const FDroneReportData CappedBaseReport = FDroneReportRules::BuildReportData(CappedBaseRecord, false);
	TestTrue(TEXT("Survival, boss damage, move, and heal base scores cap at 750 total"),
		FMath::IsNearlyEqual(CappedBaseReport.ReportScore, 750.0f, 0.01f));
	TestTrue(TEXT("BossDamageRatio is stored for UI"),
		FMath::IsNearlyEqual(CappedBaseReport.BossDamageRatio, 0.08f, 0.001f));
	TestEqual(TEXT("750 score maps to grade A"), CappedBaseReport.Grade, EDroneReportGrade::A);

	FDroneCombatRecord LowDamageRecord = CappedBaseRecord;
	LowDamageRecord.BossDamage = 5.0f;
	LowDamageRecord.MoveDistance = 600.0f;
	LowDamageRecord.HealAmount = 60.0f;
	const FDroneReportData LowDamageReport = FDroneReportRules::BuildReportData(LowDamageRecord, false);
	TestTrue(TEXT("BossDamageRatio under 1 percent zeros move and heal score"),
		FMath::IsNearlyEqual(LowDamageReport.ReportScore, 221.875f, 0.01f));
	TestEqual(TEXT("Low contribution report has no bonuses"), LowDamageReport.BonusScore, 0);

	FDroneCombatRecord AllBonusRecord = CappedBaseRecord;
	AllBonusRecord.BossDamage = 120.0f;
	AllBonusRecord.MoveDistance = 900.0f;
	AllBonusRecord.HealAmount = 70.0f;
	AllBonusRecord.DamageTakenCount = 0;
	AllBonusRecord.CombatEndTime = 180.0f;
	const FDroneReportData AllBonusReport = FDroneReportRules::BuildReportData(AllBonusRecord, true);
	TestEqual(TEXT("BonusScore caps at 250"), AllBonusReport.BonusScore, 250);
	TestEqual(TEXT("All five bonus types are listed before cap"), AllBonusReport.AchievedBonusList.Num(), 5);

	FDroneCombatRecord BossSlayerRecord = CappedBaseRecord;
	BossSlayerRecord.BossDamage = 30.0f;
	BossSlayerRecord.MoveDistance = 0.0f;
	BossSlayerRecord.HealAmount = 0.0f;
	BossSlayerRecord.DamageTakenCount = 1;
	BossSlayerRecord.CombatEndTime = 180.0f;
	const FDroneReportData BossSlayerReport = FDroneReportRules::BuildReportData(BossSlayerRecord, true);
	TestEqual(TEXT("BossSlayer basic bonus grants 80"), BossSlayerReport.BonusScore, 80);
	TestTrue(TEXT("BossSlayer bonus type is listed"),
		BossSlayerReport.AchievedBonusList.Contains(EDroneReportBonusType::BossSlayer));

	FDroneCombatRecord LateJoinRecord = BossSlayerRecord;
	LateJoinRecord.BossDamage = 10.0f;
	LateJoinRecord.BossHPOnJoin = 250.0f;
	LateJoinRecord.CombatEndTime = 30.0f;
	const FDroneReportData LateJoinReport = FDroneReportRules::BuildReportData(LateJoinRecord, true);
	TestEqual(TEXT("Late join BossSlayer bonus is limited to 40"), LateJoinReport.BonusScore, 40);

	FDroneCombatRecord TooShortRecord = AllBonusRecord;
	TooShortRecord.CombatEndTime = 29.0f;
	const FDroneReportData TooShortReport = FDroneReportRules::BuildReportData(TooShortRecord, true);
	TestEqual(TEXT("CombatDuration under 30 seconds blocks bonuses"), TooShortReport.BonusScore, 0);

	TestEqual(TEXT("850 score maps to S"), FDroneReportRules::CalculateGrade(850.0f), EDroneReportGrade::S);
	TestEqual(TEXT("650 score maps to A"), FDroneReportRules::CalculateGrade(650.0f), EDroneReportGrade::A);
	TestEqual(TEXT("400 score maps to B"), FDroneReportRules::CalculateGrade(400.0f), EDroneReportGrade::B);
	TestEqual(TEXT("399 score maps to C"), FDroneReportRules::CalculateGrade(399.0f), EDroneReportGrade::C);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneReportDuplicateGenerationTest,
	"DroneProto.D10.DroneReport.DuplicateGeneration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneReportDuplicateGenerationTest::RunTest(const FString& Parameters)
{
	FDroneSelectionTestContext Context = CreateDroneSelectionTestContext(TEXT("DroneReportDuplicateWorld"));
	TestNotNull(TEXT("test player controller is spawned"), Context.PC);
	TestNotNull(TEXT("test drone is spawned"), Context.Drone);
	if (!Context.PC || !Context.Drone)
	{
		DestroyDroneSelectionTestContext(Context);
		return false;
	}

	Context.Drone->ApplyDamageForServer(Context.Drone->GetMaxHealth() + 10, FName(TEXT("Automation")));
	TestTrue(TEXT("death creates exactly one server-held report"), Context.PC->HasDroneReportGeneratedForTest());
	const FDroneReportData FirstReport = Context.PC->GetLastDroneReportDataForTest();
	TestTrue(TEXT("created report is marked generated"), FirstReport.bIsReportGenerated);

	TestFalse(TEXT("second report request is ignored"),
		Context.PC->TryCreateDroneReportForServer(EDroneReportTrigger::RaidTimeLimit, false));
	const FDroneReportData DuplicateAttemptReport = Context.PC->GetLastDroneReportDataForTest();
	TestTrue(TEXT("duplicate attempt does not clear the existing report"),
		DuplicateAttemptReport.bIsReportGenerated);
	TestTrue(TEXT("duplicate attempt preserves the first report score"),
		FMath::IsNearlyEqual(DuplicateAttemptReport.ReportScore, FirstReport.ReportScore, 0.01f));

	DestroyDroneSelectionTestContext(Context);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneReportWidgetTextTest,
	"DroneProto.D11.DroneReport.WidgetTextHelpers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneReportWidgetTextTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("BossSlayer bonus display name"),
		UDroneReportWidget::GetBonusTypeDisplayText(EDroneReportBonusType::BossSlayer).ToString(),
		FString(TEXT("Boss Slayer")));
	TestEqual(TEXT("HighDPS bonus display name"),
		UDroneReportWidget::GetBonusTypeDisplayText(EDroneReportBonusType::HighDPS).ToString(),
		FString(TEXT("High DPS")));
	TestEqual(TEXT("NoDamage bonus display name"),
		UDroneReportWidget::GetBonusTypeDisplayText(EDroneReportBonusType::NoDamage).ToString(),
		FString(TEXT("NO DAMAGE")));
	TestEqual(TEXT("KeepMoving bonus display name"),
		UDroneReportWidget::GetBonusTypeDisplayText(EDroneReportBonusType::KeepMoving).ToString(),
		FString(TEXT("Keep Moving")));
	TestEqual(TEXT("HighRecovery bonus display name"),
		UDroneReportWidget::GetBonusTypeDisplayText(EDroneReportBonusType::HighRecovery).ToString(),
		FString(TEXT("High Recovery")));

	TestEqual(TEXT("S grade display text"), UDroneReportWidget::GetGradeDisplayText(EDroneReportGrade::S).ToString(), FString(TEXT("S")));
	TestEqual(TEXT("A grade display text"), UDroneReportWidget::GetGradeDisplayText(EDroneReportGrade::A).ToString(), FString(TEXT("A")));
	TestEqual(TEXT("B grade display text"), UDroneReportWidget::GetGradeDisplayText(EDroneReportGrade::B).ToString(), FString(TEXT("B")));
	TestEqual(TEXT("C grade display text"), UDroneReportWidget::GetGradeDisplayText(EDroneReportGrade::C).ToString(), FString(TEXT("C")));

	return true;
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
	FDroneDeadReadyGuardTest,
	"DroneProto.D6.RaidPlayerController.DeadPawnReadyAndAutoReadyIgnored",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneDeadReadyGuardTest::RunTest(const FString& Parameters)
{
	const FName PulseLaser = ADronePartInventory::GetPulseLaserPartID();
	const FName VectorCannon = ADronePartInventory::GetVectorCannonPartID();

	FDroneSelectionTestContext ManualReady = CreateDroneSelectionTestContext(TEXT("DroneDeadManualReadyGuardWorld"));
	TestNotNull(TEXT("manual ready world is created"), ManualReady.World);
	TestNotNull(TEXT("manual ready inventory actor is spawned"), ManualReady.Inventory);
	TestNotNull(TEXT("manual ready player controller is spawned"), ManualReady.PC);
	TestNotNull(TEXT("manual ready drone is spawned"), ManualReady.Drone);
	if (!ManualReady.World || !ManualReady.Inventory || !ManualReady.PC || !ManualReady.Drone)
	{
		DestroyDroneSelectionTestContext(ManualReady);
		return false;
	}

	TestTrue(TEXT("manual ready dead guard consumes selected weapon"),
		ManualReady.Inventory->TryConsumePart(PulseLaser));
	ManualReady.PC->SetSelectedPartIDForSlotForServer(EPartSlot::LeftWeapon, PulseLaser);
	const int32 ManualPulseCountBeforeReady = ManualReady.Inventory->GetCurrentCount(PulseLaser);

	ManualReady.Drone->ApplyDamageForServer(ManualReady.Drone->GetMaxHealth() + 1, FName(TEXT("Automation")));
	TestTrue(TEXT("manual ready guard starts from a dead drone"), ManualReady.Drone->IsDead());
	ManualReady.PC->Server_RequestReadyForRaid_Implementation();

	TestEqual(TEXT("dead manual ready keeps player Selecting"),
		ManualReady.PC->GetCurrentSelectionState(),
		EPlayerSelectionState::Selecting);
	TestEqual(TEXT("dead manual ready keeps selected weapon"),
		ManualReady.PC->GetSelectedPartIDBySlot(EPartSlot::LeftWeapon),
		PulseLaser);
	TestEqual(TEXT("dead manual ready does not equip selected weapon"),
		ManualReady.PC->GetEquippedPartIDBySlot(EPartSlot::LeftWeapon),
		NAME_None);
	TestEqual(TEXT("dead manual ready does not change selected weapon stock"),
		ManualReady.Inventory->GetCurrentCount(PulseLaser),
		ManualPulseCountBeforeReady);
	DestroyDroneSelectionTestContext(ManualReady);

	FDroneSelectionTestContext AutoReady = CreateDroneSelectionTestContext(TEXT("DroneDeadAutoReadyGuardWorld"));
	TestNotNull(TEXT("auto ready world is created"), AutoReady.World);
	TestNotNull(TEXT("auto ready inventory actor is spawned"), AutoReady.Inventory);
	TestNotNull(TEXT("auto ready player controller is spawned"), AutoReady.PC);
	TestNotNull(TEXT("auto ready drone is spawned"), AutoReady.Drone);
	if (!AutoReady.World || !AutoReady.Inventory || !AutoReady.PC || !AutoReady.Drone)
	{
		DestroyDroneSelectionTestContext(AutoReady);
		return false;
	}

	TestTrue(TEXT("auto ready dead guard consumes selected weapon"),
		AutoReady.Inventory->TryConsumePart(VectorCannon));
	AutoReady.PC->SetSelectedPartIDForSlotForServer(EPartSlot::RightWeapon, VectorCannon);
	AutoReady.PC->Server_RequestStartSelectionTimer_Implementation();
	const int32 AutoVectorCountBeforeReady = AutoReady.Inventory->GetCurrentCount(VectorCannon);

	AutoReady.Drone->ApplyDamageForServer(AutoReady.Drone->GetMaxHealth() + 1, FName(TEXT("Automation")));
	TestTrue(TEXT("auto ready guard starts from a dead drone"), AutoReady.Drone->IsDead());
	AutoReady.PC->HandleSelectionTimerExpiredForServer();

	TestEqual(TEXT("dead auto ready keeps player Selecting"),
		AutoReady.PC->GetCurrentSelectionState(),
		EPlayerSelectionState::Selecting);
	TestEqual(TEXT("dead auto ready keeps selected weapon"),
		AutoReady.PC->GetSelectedPartIDBySlot(EPartSlot::RightWeapon),
		VectorCannon);
	TestEqual(TEXT("dead auto ready does not equip selected weapon"),
		AutoReady.PC->GetEquippedPartIDBySlot(EPartSlot::RightWeapon),
		NAME_None);
	TestEqual(TEXT("dead auto ready does not change selected weapon stock"),
		AutoReady.Inventory->GetCurrentCount(VectorCannon),
		AutoVectorCountBeforeReady);
	DestroyDroneSelectionTestContext(AutoReady);

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
	FDronePulseLaserCombatTest,
	"DroneProto.D7.Drone.PulseLaserCombat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDronePulseLaserCombatTest::RunTest(const FString& Parameters)
{
	const FName PulseLaser = ADronePartInventory::GetPulseLaserPartID();

	FDroneSelectionTestContext SinglePulse = CreateDroneSelectionTestContext(TEXT("DronePulseSingleWorld"));
	ARaidBoss* SingleBoss = nullptr;
	if (!PrepareBattleAttackTest(*this, SinglePulse, SingleBoss, TEXT("single pulse")))
	{
		DestroyDroneSelectionTestContext(SinglePulse);
		return false;
	}

	TestTrue(TEXT("single Pulse loadout applies"), SinglePulse.Drone->ApplyLoadout(NAME_None, PulseLaser, NAME_None));
	TestTrue(TEXT("first Pulse attack deals base damage"),
		FMath::IsNearlyEqual(AttackBossAndMeasureDamage(SinglePulse.Drone, SingleBoss), 8.0f, 0.01f));
	TestEqual(TEXT("left Pulse count advances to 1"), SinglePulse.Drone->GetPulseAttackCountForTest(true), 1);
	TestTrue(TEXT("second Pulse attack deals base damage"),
		FMath::IsNearlyEqual(AttackBossAndMeasureDamage(SinglePulse.Drone, SingleBoss), 8.0f, 0.01f));
	TestEqual(TEXT("left Pulse count advances to 2"), SinglePulse.Drone->GetPulseAttackCountForTest(true), 2);
	TestTrue(TEXT("third Pulse attack deals strong damage"),
		FMath::IsNearlyEqual(AttackBossAndMeasureDamage(SinglePulse.Drone, SingleBoss), 18.0f, 0.01f));
	TestEqual(TEXT("third Pulse attack resets left count"), SinglePulse.Drone->GetPulseAttackCountForTest(true), 0);

	TestTrue(TEXT("new loadout resets Pulse count after partial chain"),
		SinglePulse.Drone->ApplyLoadout(NAME_None, PulseLaser, NAME_None));
	AttackBossAndMeasureDamage(SinglePulse.Drone, SingleBoss);
	AttackBossAndMeasureDamage(SinglePulse.Drone, SingleBoss);
	TestEqual(TEXT("left Pulse count reaches 2 before reset"), SinglePulse.Drone->GetPulseAttackCountForTest(true), 2);
	TestTrue(TEXT("reapplying loadout succeeds"), SinglePulse.Drone->ApplyLoadout(NAME_None, PulseLaser, NAME_None));
	TestEqual(TEXT("new loadout clears left Pulse count"), SinglePulse.Drone->GetPulseAttackCountForTest(true), 0);

	AttackBossAndMeasureDamage(SinglePulse.Drone, SingleBoss);
	AttackBossAndMeasureDamage(SinglePulse.Drone, SingleBoss);
	TestEqual(TEXT("left Pulse count reaches 2 before death"), SinglePulse.Drone->GetPulseAttackCountForTest(true), 2);
	SinglePulse.Drone->ApplyDamageForServer(SinglePulse.Drone->GetMaxHealth() + 10, FName(TEXT("Automation")));
	TestTrue(TEXT("lethal damage marks Pulse test drone dead"), SinglePulse.Drone->IsDead());
	TestEqual(TEXT("death clears left Pulse count"), SinglePulse.Drone->GetPulseAttackCountForTest(true), 0);
	DestroyDroneSelectionTestContext(SinglePulse);

	FDroneSelectionTestContext DoublePulse = CreateDroneSelectionTestContext(TEXT("DronePulseDoubleWorld"));
	ARaidBoss* DoubleBoss = nullptr;
	if (!PrepareBattleAttackTest(*this, DoublePulse, DoubleBoss, TEXT("double pulse")))
	{
		DestroyDroneSelectionTestContext(DoublePulse);
		return false;
	}

	TestTrue(TEXT("double Pulse loadout applies"),
		DoublePulse.Drone->ApplyLoadout(NAME_None, PulseLaser, PulseLaser));
	TestTrue(TEXT("double Pulse first attack deals two base hits"),
		FMath::IsNearlyEqual(AttackBossAndMeasureDamage(DoublePulse.Drone, DoubleBoss), 16.0f, 0.01f));
	TestEqual(TEXT("left Pulse count advances independently to 1"),
		DoublePulse.Drone->GetPulseAttackCountForTest(true),
		1);
	TestEqual(TEXT("right Pulse count advances independently to 1"),
		DoublePulse.Drone->GetPulseAttackCountForTest(false),
		1);
	TestTrue(TEXT("double Pulse second attack deals two base hits"),
		FMath::IsNearlyEqual(AttackBossAndMeasureDamage(DoublePulse.Drone, DoubleBoss), 16.0f, 0.01f));
	TestEqual(TEXT("left Pulse count advances independently to 2"),
		DoublePulse.Drone->GetPulseAttackCountForTest(true),
		2);
	TestEqual(TEXT("right Pulse count advances independently to 2"),
		DoublePulse.Drone->GetPulseAttackCountForTest(false),
		2);
	TestTrue(TEXT("double Pulse third attack deals two strong hits"),
		FMath::IsNearlyEqual(AttackBossAndMeasureDamage(DoublePulse.Drone, DoubleBoss), 36.0f, 0.01f));
	TestEqual(TEXT("left Pulse strong hit resets only left count"),
		DoublePulse.Drone->GetPulseAttackCountForTest(true),
		0);
	TestEqual(TEXT("right Pulse strong hit resets only right count"),
		DoublePulse.Drone->GetPulseAttackCountForTest(false),
		0);

	AttackBossAndMeasureDamage(DoublePulse.Drone, DoubleBoss);
	AttackBossAndMeasureDamage(DoublePulse.Drone, DoubleBoss);
	TestEqual(TEXT("left Pulse count reaches 2 before RaidEnd"), DoublePulse.Drone->GetPulseAttackCountForTest(true), 2);
	ARaidGameMode* GameMode = DoublePulse.World->SpawnActor<ARaidGameMode>();
	TestNotNull(TEXT("raid end game mode is spawned"), GameMode);
	if (GameMode)
	{
		DoublePulse.World->AddController(DoublePulse.PC);
		GameMode->ReturnAllEquippedPartsForRaidEnd(FName(TEXT("Automation")));
		TestEqual(TEXT("RaidEnd clears left Pulse count"), DoublePulse.Drone->GetPulseAttackCountForTest(true), 0);
		TestEqual(TEXT("RaidEnd clears right Pulse count"), DoublePulse.Drone->GetPulseAttackCountForTest(false), 0);
	}

	DestroyDroneSelectionTestContext(DoublePulse);

	FDroneSelectionTestContext DeadBossPulse = CreateDroneSelectionTestContext(TEXT("DronePulseDeadBossWorld"));
	ARaidBoss* DeadBoss = nullptr;
	if (!PrepareBattleAttackTest(*this, DeadBossPulse, DeadBoss, TEXT("dead boss pulse")))
	{
		DestroyDroneSelectionTestContext(DeadBossPulse);
		return false;
	}

	TestTrue(TEXT("dead boss Pulse loadout applies"),
		DeadBossPulse.Drone->ApplyLoadout(NAME_None, PulseLaser, NAME_None));
	DeadBoss->ApplyDamageForServer(DeadBoss->GetMaxHP() + 100.0f, DeadBossPulse.PC, DeadBossPulse.Drone);
	TestTrue(TEXT("dead boss setup reaches zero HP"),
		FMath::IsNearlyZero(DeadBoss->GetCurrentHP(), 0.01f));
	DeadBossPulse.Drone->RequestAttackBoss();
	TestEqual(TEXT("BossDead attack does not advance Pulse counter"),
		DeadBossPulse.Drone->GetPulseAttackCountForTest(true),
		0);
	TestTrue(TEXT("BossDead attack keeps boss HP at zero"),
		FMath::IsNearlyZero(DeadBoss->GetCurrentHP(), 0.01f));
	DestroyDroneSelectionTestContext(DeadBossPulse);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneFractureBurstCombatTest,
	"DroneProto.D7.Drone.FractureBurstCombat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneFractureBurstCombatTest::RunTest(const FString& Parameters)
{
	FDroneSelectionTestContext Context = CreateDroneSelectionTestContext(TEXT("DroneFractureBurstWorld"));
	ARaidBoss* Boss = nullptr;
	if (!PrepareBattleAttackTest(*this, Context, Boss, TEXT("fracture burst")))
	{
		DestroyDroneSelectionTestContext(Context);
		return false;
	}

	const FName FractureBurst = ADronePartInventory::GetFractureBurstPartID();
	TestTrue(TEXT("single Fracture loadout applies"),
		Context.Drone->ApplyLoadout(NAME_None, FractureBurst, NAME_None));
	TestTrue(TEXT("single Fracture attack deals base plus shards"),
		FMath::IsNearlyEqual(AttackBossAndMeasureDamage(Context.Drone, Boss), 11.0f, 0.01f));

	TestTrue(TEXT("Fracture plus Fracture with CORE_002 loadout applies"),
		Context.Drone->ApplyLoadout(ADronePartInventory::GetCoreBoosterPartID(), FractureBurst, FractureBurst));
	TestTrue(TEXT("Fracture plus Fracture with no movement keeps base 22 damage"),
		FMath::IsNearlyEqual(AttackBossAndMeasureDamage(Context.Drone, Boss), 22.0f, 0.01f));

	DestroyDroneSelectionTestContext(Context);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneZenithCoreCombatTest,
	"DroneProto.D7.Drone.ZenithCoreCombat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneZenithCoreCombatTest::RunTest(const FString& Parameters)
{
	FDroneSelectionTestContext Context = CreateDroneSelectionTestContext(TEXT("DroneZenithCoreWorld"));
	ARaidBoss* Boss = nullptr;
	if (!PrepareBattleAttackTest(*this, Context, Boss, TEXT("zenith core")))
	{
		DestroyDroneSelectionTestContext(Context);
		return false;
	}

	const FName ZenithCore = ADronePartInventory::GetCoreZenithPartID();
	const FName PulseLaser = ADronePartInventory::GetPulseLaserPartID();
	TestTrue(TEXT("Zenith Pulse loadout applies"),
		Context.Drone->ApplyLoadout(ZenithCore, PulseLaser, NAME_None));
	TestTrue(TEXT("full HP Zenith applies 1.20 modifier"),
		FMath::IsNearlyEqual(AttackBossAndMeasureDamage(Context.Drone, Boss), 9.60f, 0.01f));

	TestTrue(TEXT("Zenith Pulse loadout reapplies for half HP case"),
		Context.Drone->ApplyLoadout(ZenithCore, PulseLaser, NAME_None));
	Context.Drone->ApplyDamageForServer(50, FName(TEXT("Automation")));
	TestEqual(TEXT("Zenith half HP setup reaches 50 HP"), Context.Drone->GetHealth(), 50);
	TestTrue(TEXT("50 percent HP Zenith applies 1.10 modifier"),
		FMath::IsNearlyEqual(AttackBossAndMeasureDamage(Context.Drone, Boss), 8.80f, 0.01f));

	DestroyDroneSelectionTestContext(Context);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneDrainCoreCombatTest,
	"DroneProto.D7.Drone.DrainCoreCombat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneDrainCoreCombatTest::RunTest(const FString& Parameters)
{
	FDroneSelectionTestContext Context = CreateDroneSelectionTestContext(TEXT("DroneDrainCoreWorld"));
	ARaidBoss* Boss = nullptr;
	if (!PrepareBattleAttackTest(*this, Context, Boss, TEXT("drain core")))
	{
		DestroyDroneSelectionTestContext(Context);
		return false;
	}

	const FName DrainCore = ADronePartInventory::GetCoreDrainPartID();
	const FName FractureBurst = ADronePartInventory::GetFractureBurstPartID();
	const FName PulseLaser = ADronePartInventory::GetPulseLaserPartID();

	TestTrue(TEXT("Drain Fracture plus Fracture loadout applies"),
		Context.Drone->ApplyLoadout(DrainCore, FractureBurst, FractureBurst));
	Context.Drone->ApplyDamageForServer(20, FName(TEXT("Automation")));
	const float HPBeforeFractureDrain = Context.Drone->GetHealthValueForTest();
	TestTrue(TEXT("Drain Fracture plus Fracture deals unmodified 22 damage"),
		FMath::IsNearlyEqual(AttackBossAndMeasureDamage(Context.Drone, Boss), 22.0f, 0.01f));
	TestTrue(TEXT("Drain heals from total Fracture damage once per input"),
		FMath::IsNearlyEqual(Context.Drone->GetHealthValueForTest(), HPBeforeFractureDrain + 2.64f, 0.01f));

	TestTrue(TEXT("Drain with empty weapons loadout applies"),
		Context.Drone->ApplyLoadout(DrainCore, NAME_None, NAME_None));
	Context.Drone->ApplyDamageForServer(10, FName(TEXT("Automation")));
	const float HPBeforeZeroDamageDrain = Context.Drone->GetHealthValueForTest();
	TestTrue(TEXT("Drain empty weapon attack deals zero damage"),
		FMath::IsNearlyEqual(AttackBossAndMeasureDamage(Context.Drone, Boss), 0.0f, 0.01f));
	TestTrue(TEXT("zero damage Drain attack heals zero"),
		FMath::IsNearlyEqual(Context.Drone->GetHealthValueForTest(), HPBeforeZeroDamageDrain, 0.01f));

	TestTrue(TEXT("Drain Pulse plus Pulse loadout applies"),
		Context.Drone->ApplyLoadout(DrainCore, PulseLaser, PulseLaser));
	Context.Drone->ApplyDamageForServer(50, FName(TEXT("Automation")));
	AttackBossAndMeasureDamage(Context.Drone, Boss);
	AttackBossAndMeasureDamage(Context.Drone, Boss);
	const float HPBeforeStrongDrain = Context.Drone->GetHealthValueForTest();
	TestTrue(TEXT("third Drain double Pulse attack deals capped-heal damage"),
		FMath::IsNearlyEqual(AttackBossAndMeasureDamage(Context.Drone, Boss), 36.0f, 0.01f));
	TestTrue(TEXT("Drain heal is capped at 3 per attack input"),
		FMath::IsNearlyEqual(Context.Drone->GetHealthValueForTest(), HPBeforeStrongDrain + 3.0f, 0.01f));

	TestTrue(TEXT("Drain max health cap loadout applies"),
		Context.Drone->ApplyLoadout(DrainCore, FractureBurst, FractureBurst));
	Context.Drone->ApplyDamageForServer(1, FName(TEXT("Automation")));
	AttackBossAndMeasureDamage(Context.Drone, Boss);
	TestTrue(TEXT("Drain never heals beyond MaxHealth"),
		Context.Drone->GetHealthValueForTest() <= static_cast<float>(Context.Drone->GetMaxHealth()) + 0.01f);

	DestroyDroneSelectionTestContext(Context);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneVectorBoosterCombatRecordTest,
	"DroneProto.D9.Drone.VectorBoosterCombatRecord",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneVectorBoosterCombatRecordTest::RunTest(const FString& Parameters)
{
	FDroneSelectionTestContext Context = CreateDroneSelectionTestContext(TEXT("DroneVectorBoosterCombatRecordWorld"));
	ARaidBoss* Boss = nullptr;
	if (!PrepareBattleAttackTest(*this, Context, Boss, TEXT("vector booster record")))
	{
		DestroyDroneSelectionTestContext(Context);
		return false;
	}

	const FName BoosterCore = ADronePartInventory::GetCoreBoosterPartID();
	const FName VectorCannon = ADronePartInventory::GetVectorCannonPartID();
	TestTrue(TEXT("Booster double Vector loadout applies"),
		Context.Drone->ApplyLoadout(BoosterCore, VectorCannon, VectorCannon));

	Context.Drone->SetActorLocation(FVector::ZeroVector);
	Context.Drone->ResetMoveDistanceForServerForTest(FName(TEXT("Automation")));
	Context.Drone->UpdateMoveDistanceForServerForTest(0.016f);
	for (int32 Step = 1; Step <= 8; ++Step)
	{
		Context.Drone->SetActorLocation(FVector(static_cast<float>(Step) * 500.0f, 0.0f, 0.0f));
		Context.Drone->UpdateMoveDistanceForServerForTest(0.25f);
	}

	TestTrue(TEXT("movement setup accumulates 40 meters for Vector"),
		FMath::IsNearlyEqual(Context.Drone->GetVectorAccumulatedMoveDistanceForTest(), 40.0f, 0.01f));
	TestTrue(TEXT("movement setup accumulates 40 meters for Booster"),
		FMath::IsNearlyEqual(Context.Drone->GetBoosterAccumulatedMoveDistanceForTest(), 40.0f, 0.01f));
	TestTrue(TEXT("double Vector uses capped damage and Booster attack bonus"),
		FMath::IsNearlyEqual(AttackBossAndMeasureDamage(Context.Drone, Boss), 30.9f, 0.01f));
	TestTrue(TEXT("Vector attack resets Vector distance only"),
		FMath::IsNearlyZero(Context.Drone->GetVectorAccumulatedMoveDistanceForTest(), 0.001f));
	TestTrue(TEXT("Vector attack keeps Booster distance"),
		FMath::IsNearlyEqual(Context.Drone->GetBoosterAccumulatedMoveDistanceForTest(), 40.0f, 0.01f));

	const FDroneCombatRecord Record = Context.Drone->GetCombatRecordForTest();
	TestTrue(TEXT("CombatRecord accumulates boss damage"),
		FMath::IsNearlyEqual(Record.BossDamage, 30.9f, 0.01f));
	TestTrue(TEXT("CombatRecord accumulates move distance"),
		FMath::IsNearlyEqual(Record.MoveDistance, 40.0f, 0.01f));
	TestTrue(TEXT("CombatRecord stores boss max HP for ratio calculation"),
		FMath::IsNearlyEqual(Record.BossMaxHP, Boss->GetMaxHP(), 0.01f));
	TestTrue(TEXT("CombatRecord stores boss HP on join"),
		Record.BossHPOnJoin > 0.0f);

	Context.Drone->ApplyDamageForServer(10, FName(TEXT("Automation")));
	const FDroneCombatRecord DamagedRecord = Context.Drone->GetCombatRecordForTest();
	TestEqual(TEXT("CombatRecord increments damage taken count"), DamagedRecord.DamageTakenCount, 1);

	DestroyDroneSelectionTestContext(Context);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneServerMoveInputAuthorityTest,
	"DroneProto.D8.Drone.ServerMoveInputAuthority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneServerMoveInputAuthorityTest::RunTest(const FString& Parameters)
{
	FDroneSelectionTestContext Context = CreateDroneSelectionTestContext(TEXT("DroneServerMoveInputAuthorityWorld"));
	TestNotNull(TEXT("move input world is created"), Context.World);
	TestNotNull(TEXT("move input player controller is spawned"), Context.PC);
	TestNotNull(TEXT("move input drone is spawned"), Context.Drone);
	if (!Context.World || !Context.PC || !Context.Drone)
	{
		DestroyDroneSelectionTestContext(Context);
		return false;
	}

	TestEqual(TEXT("move input test starts Selecting"),
		Context.PC->GetCurrentSelectionState(),
		EPlayerSelectionState::Selecting);
	TestFalse(TEXT("Selecting pawn move input is ignored by server"),
		Context.Drone->ApplyMoveInputForServerForTest(FVector2D(1.0f, 0.0f)));
	TestTrue(TEXT("ignored move input keeps last accepted axis zero"),
		Context.Drone->GetLastServerMoveInputForTest().IsNearlyZero());

	Context.PC->Server_RequestReadyForRaid_Implementation();
	TestEqual(TEXT("ready player is InBattle before server move input"),
		Context.PC->GetCurrentSelectionState(),
		EPlayerSelectionState::InBattle);

	TestTrue(TEXT("InBattle alive pawn move input is accepted"),
		Context.Drone->ApplyMoveInputForServerForTest(FVector2D(3.0f, 4.0f)));
	TestTrue(TEXT("server clamps move input vector length to one"),
		FMath::IsNearlyEqual(Context.Drone->GetLastServerMoveInputForTest().Size(), 1.0f, 0.001f));
	const FVector LocationBeforeServerMove = Context.Drone->GetActorLocation();
	TestTrue(TEXT("pending accepted server move input is applied"),
		Context.Drone->ApplyPendingServerMoveInputForTest(0.10f));
	const FVector LocationAfterServerMove = Context.Drone->GetActorLocation();
	TestTrue(TEXT("accepted server move input changes drone location on server tick"),
		!LocationAfterServerMove.Equals(LocationBeforeServerMove, 0.1f));
	TestTrue(TEXT("accepted server move input is consumed after server tick"),
		Context.Drone->GetLastServerMoveInputForTest().IsNearlyZero());

	Context.PC->UnPossess();
	TestFalse(TEXT("unpossessed drone move input is ignored"),
		Context.Drone->ApplyMoveInputForServerForTest(FVector2D(1.0f, 0.0f)));
	TestTrue(TEXT("unpossessed ignored input clears last server axis"),
		Context.Drone->GetLastServerMoveInputForTest().IsNearlyZero());

	Context.PC->Possess(Context.Drone);
	Context.Drone->ApplyDamageForServer(Context.Drone->GetMaxHealth() + 10, FName(TEXT("Automation")));
	TestTrue(TEXT("move input test drone is dead"), Context.Drone->IsDead());
	TestFalse(TEXT("Dead pawn move input is ignored by server"),
		Context.Drone->ApplyMoveInputForServerForTest(FVector2D(1.0f, 0.0f)));

	DestroyDroneSelectionTestContext(Context);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneMoveDistanceAccumulationTest,
	"DroneProto.D8.Drone.MoveDistanceAccumulation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneMoveDistanceAccumulationTest::RunTest(const FString& Parameters)
{
	FDroneSelectionTestContext Context = CreateDroneSelectionTestContext(TEXT("DroneMoveDistanceAccumulationWorld"));
	TestNotNull(TEXT("move distance world is created"), Context.World);
	TestNotNull(TEXT("move distance player controller is spawned"), Context.PC);
	TestNotNull(TEXT("move distance drone is spawned"), Context.Drone);
	if (!Context.World || !Context.PC || !Context.Drone)
	{
		DestroyDroneSelectionTestContext(Context);
		return false;
	}

	Context.Drone->SetActorLocation(FVector::ZeroVector);
	Context.Drone->ResetMoveDistanceForServerForTest(FName(TEXT("Spawn")));
	Context.Drone->SetActorLocation(FVector(100.0f, 0.0f, 0.0f));
	Context.Drone->UpdateMoveDistanceForServerForTest(0.016f);
	TestTrue(TEXT("first server location sample is ignored"),
		FMath::IsNearlyZero(Context.Drone->GetVectorAccumulatedMoveDistanceForTest(), 0.001f));
	TestTrue(TEXT("Selecting movement is not accumulated"),
		FMath::IsNearlyZero(Context.Drone->GetBoosterAccumulatedMoveDistanceForTest(), 0.001f));

	Context.PC->Server_RequestReadyForRaid_Implementation();
	TestEqual(TEXT("move distance player is InBattle"),
		Context.PC->GetCurrentSelectionState(),
		EPlayerSelectionState::InBattle);
	Context.Drone->ResetMoveDistanceForServerForTest(FName(TEXT("Possess")));
	Context.Drone->SetActorLocation(FVector::ZeroVector);
	Context.Drone->UpdateMoveDistanceForServerForTest(0.016f);
	Context.Drone->SetActorLocation(FVector(100.0f, 0.0f, 0.0f));
	Context.Drone->UpdateMoveDistanceForServerForTest(0.10f);
	TestTrue(TEXT("InBattle alive movement accumulates Vector meters"),
		FMath::IsNearlyEqual(Context.Drone->GetVectorAccumulatedMoveDistanceForTest(), 1.0f, 0.001f));
	TestTrue(TEXT("InBattle alive movement accumulates Booster meters separately"),
		FMath::IsNearlyEqual(Context.Drone->GetBoosterAccumulatedMoveDistanceForTest(), 1.0f, 0.001f));

	Context.Drone->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
	Context.Drone->UpdateMoveDistanceForServerForTest(0.10f);
	TestTrue(TEXT("second normal move adds to Vector total"),
		FMath::IsNearlyEqual(Context.Drone->GetVectorAccumulatedMoveDistanceForTest(), 1.5f, 0.001f));
	TestTrue(TEXT("second normal move adds to Booster total"),
		FMath::IsNearlyEqual(Context.Drone->GetBoosterAccumulatedMoveDistanceForTest(), 1.5f, 0.001f));

	Context.Drone->SetActorLocation(FVector(100000.0f, 0.0f, 0.0f));
	Context.Drone->UpdateMoveDistanceForServerForTest(0.016f);
	TestTrue(TEXT("TooLargeDelta is ignored for Vector total"),
		FMath::IsNearlyEqual(Context.Drone->GetVectorAccumulatedMoveDistanceForTest(), 1.5f, 0.001f));
	TestTrue(TEXT("TooLargeDelta is ignored for Booster total"),
		FMath::IsNearlyEqual(Context.Drone->GetBoosterAccumulatedMoveDistanceForTest(), 1.5f, 0.001f));

	Context.Drone->SetActorLocation(FVector(100100.0f, 0.0f, 0.0f));
	Context.Drone->UpdateMoveDistanceForServerForTest(0.50f);
	TestTrue(TEXT("hitch delta time is ignored for Vector total"),
		FMath::IsNearlyEqual(Context.Drone->GetVectorAccumulatedMoveDistanceForTest(), 1.5f, 0.001f));
	TestTrue(TEXT("hitch delta time is ignored for Booster total"),
		FMath::IsNearlyEqual(Context.Drone->GetBoosterAccumulatedMoveDistanceForTest(), 1.5f, 0.001f));

	Context.Drone->ResetVectorMoveDistanceForServerForTest(FName(TEXT("VectorAttack")));
	TestTrue(TEXT("Vector reset clears only Vector total"),
		FMath::IsNearlyZero(Context.Drone->GetVectorAccumulatedMoveDistanceForTest(), 0.001f));
	TestTrue(TEXT("Vector reset does not clear Booster total"),
		FMath::IsNearlyEqual(Context.Drone->GetBoosterAccumulatedMoveDistanceForTest(), 1.5f, 0.001f));

	Context.Drone->ResetMoveDistanceForServerForTest(FName(TEXT("Loadout")));
	TestTrue(TEXT("full move distance reset clears Vector total"),
		FMath::IsNearlyZero(Context.Drone->GetVectorAccumulatedMoveDistanceForTest(), 0.001f));
	TestTrue(TEXT("full move distance reset clears Booster total"),
		FMath::IsNearlyZero(Context.Drone->GetBoosterAccumulatedMoveDistanceForTest(), 0.001f));

	DestroyDroneSelectionTestContext(Context);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneMoveDistanceResetTest,
	"DroneProto.D8.Drone.MoveDistanceReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneMoveDistanceResetTest::RunTest(const FString& Parameters)
{
	UDronePartReturnManager* DeathReturnManager = nullptr;
	FDroneSelectionTestContext DeathContext = CreateDroneReturnTestContext(
		TEXT("DroneMoveDistanceDeathResetWorld"),
		DeathReturnManager);
	TestNotNull(TEXT("death reset world is created"), DeathContext.World);
	TestNotNull(TEXT("death reset player controller is spawned"), DeathContext.PC);
	TestNotNull(TEXT("death reset drone is spawned"), DeathContext.Drone);
	if (!DeathContext.World || !DeathContext.PC || !DeathContext.Drone)
	{
		DestroyDroneSelectionTestContext(DeathContext);
		return false;
	}

	DeathContext.PC->Server_RequestReadyForRaid_Implementation();
	DeathContext.Drone->ResetMoveDistanceForServerForTest(FName(TEXT("Possess")));
	DeathContext.Drone->SetActorLocation(FVector::ZeroVector);
	DeathContext.Drone->UpdateMoveDistanceForServerForTest(0.016f);
	DeathContext.Drone->SetActorLocation(FVector(200.0f, 0.0f, 0.0f));
	DeathContext.Drone->UpdateMoveDistanceForServerForTest(0.10f);
	TestTrue(TEXT("death reset setup accumulates movement"),
		DeathContext.Drone->GetVectorAccumulatedMoveDistanceForTest() > 0.0f);
	DeathContext.Drone->ApplyDamageForServer(DeathContext.Drone->GetMaxHealth() + 10, FName(TEXT("Automation")));
	TestTrue(TEXT("death reset drone dies"), DeathContext.Drone->IsDead());
	TestTrue(TEXT("death resets Vector movement distance"),
		FMath::IsNearlyZero(DeathContext.Drone->GetVectorAccumulatedMoveDistanceForTest(), 0.001f));
	TestTrue(TEXT("death resets Booster movement distance"),
		FMath::IsNearlyZero(DeathContext.Drone->GetBoosterAccumulatedMoveDistanceForTest(), 0.001f));
	DestroyDroneSelectionTestContext(DeathContext);

	UDronePartReturnManager* RaidEndReturnManager = nullptr;
	FDroneSelectionTestContext RaidEndContext = CreateDroneReturnTestContext(
		TEXT("DroneMoveDistanceRaidEndResetWorld"),
		RaidEndReturnManager);
	TestNotNull(TEXT("raid end reset world is created"), RaidEndContext.World);
	TestNotNull(TEXT("raid end reset player controller is spawned"), RaidEndContext.PC);
	TestNotNull(TEXT("raid end reset drone is spawned"), RaidEndContext.Drone);
	if (!RaidEndContext.World || !RaidEndContext.PC || !RaidEndContext.Drone)
	{
		DestroyDroneSelectionTestContext(RaidEndContext);
		return false;
	}

	RaidEndContext.PC->Server_RequestReadyForRaid_Implementation();
	RaidEndContext.Drone->ResetMoveDistanceForServerForTest(FName(TEXT("Possess")));
	RaidEndContext.Drone->SetActorLocation(FVector::ZeroVector);
	RaidEndContext.Drone->UpdateMoveDistanceForServerForTest(0.016f);
	RaidEndContext.Drone->SetActorLocation(FVector(300.0f, 0.0f, 0.0f));
	RaidEndContext.Drone->UpdateMoveDistanceForServerForTest(0.10f);
	TestTrue(TEXT("raid end reset setup accumulates movement"),
		RaidEndContext.Drone->GetBoosterAccumulatedMoveDistanceForTest() > 0.0f);
	if (ARaidGameMode* GameMode = RaidEndContext.World->SpawnActor<ARaidGameMode>())
	{
		RaidEndContext.World->AddController(RaidEndContext.PC);
		GameMode->ReturnAllEquippedPartsForRaidEnd(FName(TEXT("Automation")));
	}
	TestTrue(TEXT("RaidEnd resets Vector movement distance"),
		FMath::IsNearlyZero(RaidEndContext.Drone->GetVectorAccumulatedMoveDistanceForTest(), 0.001f));
	TestTrue(TEXT("RaidEnd resets Booster movement distance"),
		FMath::IsNearlyZero(RaidEndContext.Drone->GetBoosterAccumulatedMoveDistanceForTest(), 0.001f));

	RaidEndContext.Drone->SetActorLocation(FVector::ZeroVector);
	RaidEndContext.Drone->UpdateMoveDistanceForServerForTest(0.016f);
	RaidEndContext.Drone->SetActorLocation(FVector(100.0f, 0.0f, 0.0f));
	RaidEndContext.Drone->UpdateMoveDistanceForServerForTest(0.10f);
	TestTrue(TEXT("RaidEnd state keeps later movement from accumulating"),
		FMath::IsNearlyZero(RaidEndContext.Drone->GetVectorAccumulatedMoveDistanceForTest(), 0.001f));
	DestroyDroneSelectionTestContext(RaidEndContext);

	FDroneSelectionTestContext LoadoutContext = CreateDroneSelectionTestContext(TEXT("DroneMoveDistanceLoadoutResetWorld"));
	TestNotNull(TEXT("loadout reset world is created"), LoadoutContext.World);
	TestNotNull(TEXT("loadout reset player controller is spawned"), LoadoutContext.PC);
	TestNotNull(TEXT("loadout reset drone is spawned"), LoadoutContext.Drone);
	if (!LoadoutContext.World || !LoadoutContext.PC || !LoadoutContext.Drone)
	{
		DestroyDroneSelectionTestContext(LoadoutContext);
		return false;
	}

	LoadoutContext.PC->Server_RequestReadyForRaid_Implementation();
	LoadoutContext.Drone->ResetMoveDistanceForServerForTest(FName(TEXT("Possess")));
	LoadoutContext.Drone->SetActorLocation(FVector::ZeroVector);
	LoadoutContext.Drone->UpdateMoveDistanceForServerForTest(0.016f);
	LoadoutContext.Drone->SetActorLocation(FVector(100.0f, 0.0f, 0.0f));
	LoadoutContext.Drone->UpdateMoveDistanceForServerForTest(0.10f);
	TestTrue(TEXT("new loadout reset setup accumulates movement"),
		LoadoutContext.Drone->GetVectorAccumulatedMoveDistanceForTest() > 0.0f);
	TestTrue(TEXT("reapplying loadout resets Vector movement distance"),
		LoadoutContext.Drone->ApplyLoadout(NAME_None, ADronePartInventory::GetFractureBurstPartID(), NAME_None));
	TestTrue(TEXT("new loadout resets Vector movement distance"),
		FMath::IsNearlyZero(LoadoutContext.Drone->GetVectorAccumulatedMoveDistanceForTest(), 0.001f));
	TestTrue(TEXT("new loadout resets Booster movement distance"),
		FMath::IsNearlyZero(LoadoutContext.Drone->GetBoosterAccumulatedMoveDistanceForTest(), 0.001f));

	DestroyDroneSelectionTestContext(LoadoutContext);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneDeathReturnTest,
	"DroneProto.D6.Drone.DeathReturnClearsEquippedSlots",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneDeathReturnTest::RunTest(const FString& Parameters)
{
	UDronePartReturnManager* FullReturnManager = nullptr;
	FDroneSelectionTestContext FullLoadout = CreateDroneReturnTestContext(TEXT("DroneDeathReturnFullLoadoutWorld"), FullReturnManager);
	TestNotNull(TEXT("full loadout world is created"), FullLoadout.World);
	TestNotNull(TEXT("full loadout inventory is spawned"), FullLoadout.Inventory);
	TestNotNull(TEXT("full loadout player controller is spawned"), FullLoadout.PC);
	TestNotNull(TEXT("full loadout drone is spawned"), FullLoadout.Drone);
	TestNotNull(TEXT("full loadout return manager is created"), FullReturnManager);
	if (!FullLoadout.World || !FullLoadout.Inventory || !FullLoadout.PC || !FullLoadout.Drone || !FullReturnManager)
	{
		DestroyDroneSelectionTestContext(FullLoadout);
		return false;
	}

	const FName CoreZenith = ADronePartInventory::GetCoreZenithPartID();
	const FName PulseLaser = ADronePartInventory::GetPulseLaserPartID();
	const FName VectorCannon = ADronePartInventory::GetVectorCannonPartID();

	TestTrue(TEXT("full death test consumes selected core"), FullLoadout.Inventory->TryConsumePart(CoreZenith));
	TestTrue(TEXT("full death test consumes selected left weapon"), FullLoadout.Inventory->TryConsumePart(PulseLaser));
	TestTrue(TEXT("full death test consumes selected right weapon"), FullLoadout.Inventory->TryConsumePart(VectorCannon));
	FullLoadout.PC->SetSelectedPartIDForSlotForServer(EPartSlot::Core, CoreZenith);
	FullLoadout.PC->SetSelectedPartIDForSlotForServer(EPartSlot::LeftWeapon, PulseLaser);
	FullLoadout.PC->SetSelectedPartIDForSlotForServer(EPartSlot::RightWeapon, VectorCannon);
	FullLoadout.PC->Server_RequestReadyForRaid_Implementation();

	TestEqual(TEXT("full loadout ready moves player to battle"),
		FullLoadout.PC->GetCurrentSelectionState(),
		EPlayerSelectionState::InBattle);
	TestEqual(TEXT("full loadout equips core before death"),
		FullLoadout.PC->GetEquippedPartIDBySlot(EPartSlot::Core),
		CoreZenith);
	TestEqual(TEXT("full loadout equips left weapon before death"),
		FullLoadout.PC->GetEquippedPartIDBySlot(EPartSlot::LeftWeapon),
		PulseLaser);
	TestEqual(TEXT("full loadout equips right weapon before death"),
		FullLoadout.PC->GetEquippedPartIDBySlot(EPartSlot::RightWeapon),
		VectorCannon);

	ARaidBoss* Boss = FullLoadout.World->SpawnActor<ARaidBoss>();
	TestNotNull(TEXT("boss is spawned for dead attack guard"), Boss);
	if (Boss && FullLoadout.GameState)
	{
		FullLoadout.GameState->SetRaidBossForServer(Boss);
	}

	FullLoadout.Drone->ApplyDamageForServer(FullLoadout.Drone->GetMaxHealth() + 50, FName(TEXT("Automation")));

	TestTrue(TEXT("lethal damage marks drone dead"), FullLoadout.Drone->IsDead());
	TestEqual(TEXT("lethal damage clamps health to zero"), FullLoadout.Drone->GetHealth(), 0);
	TestEqual(TEXT("death return clears equipped core slot"),
		FullLoadout.PC->GetEquippedPartIDBySlot(EPartSlot::Core),
		NAME_None);
	TestEqual(TEXT("death return clears equipped left weapon slot"),
		FullLoadout.PC->GetEquippedPartIDBySlot(EPartSlot::LeftWeapon),
		NAME_None);
	TestEqual(TEXT("death return clears equipped right weapon slot"),
		FullLoadout.PC->GetEquippedPartIDBySlot(EPartSlot::RightWeapon),
		NAME_None);
	TestEqual(TEXT("death return restores core stock"),
		FullLoadout.Inventory->GetCurrentCount(CoreZenith),
		FullLoadout.Inventory->GetMaxCount(CoreZenith));
	TestEqual(TEXT("death return restores left weapon stock"),
		FullLoadout.Inventory->GetCurrentCount(PulseLaser),
		FullLoadout.Inventory->GetMaxCount(PulseLaser));
	TestEqual(TEXT("death return restores right weapon stock"),
		FullLoadout.Inventory->GetCurrentCount(VectorCannon),
		FullLoadout.Inventory->GetMaxCount(VectorCannon));

	const float BossHPAfterDeath = Boss ? Boss->GetCurrentHP() : 0.0f;
	FullLoadout.Drone->RequestAttackBoss();
	if (Boss)
	{
		TestTrue(TEXT("dead attack is ignored before boss damage"),
			FMath::IsNearlyEqual(Boss->GetCurrentHP(), BossHPAfterDeath, 0.01f));
	}
	TestFalse(TEXT("dead dodge is ignored"), FullLoadout.Drone->RequestDodgeForServer());
	TestFalse(TEXT("dead heal is ignored"), FullLoadout.Drone->HealForServer(50));
	TestEqual(TEXT("dead heal does not revive the drone"), FullLoadout.Drone->GetHealth(), 0);

	TestFalse(TEXT("logout after death skips because equipped slots were cleared"),
		FullLoadout.PC->ReturnEquippedPartsForServer(EDronePartReturnReason::Disconnect));
	TestEqual(TEXT("logout after death does not over-return core stock"),
		FullLoadout.Inventory->GetCurrentCount(CoreZenith),
		FullLoadout.Inventory->GetMaxCount(CoreZenith));
	TestEqual(TEXT("logout after death does not over-return left stock"),
		FullLoadout.Inventory->GetCurrentCount(PulseLaser),
		FullLoadout.Inventory->GetMaxCount(PulseLaser));
	TestEqual(TEXT("logout after death does not over-return right stock"),
		FullLoadout.Inventory->GetCurrentCount(VectorCannon),
		FullLoadout.Inventory->GetMaxCount(VectorCannon));
	DestroyDroneSelectionTestContext(FullLoadout);

	UDronePartReturnManager* EmptyReturnManager = nullptr;
	FDroneSelectionTestContext EmptyLoadout = CreateDroneReturnTestContext(TEXT("DroneDeathReturnEmptyLoadoutWorld"), EmptyReturnManager);
	TestNotNull(TEXT("empty loadout world is created"), EmptyLoadout.World);
	TestNotNull(TEXT("empty loadout inventory is spawned"), EmptyLoadout.Inventory);
	TestNotNull(TEXT("empty loadout player controller is spawned"), EmptyLoadout.PC);
	TestNotNull(TEXT("empty loadout drone is spawned"), EmptyLoadout.Drone);
	if (!EmptyLoadout.World || !EmptyLoadout.Inventory || !EmptyLoadout.PC || !EmptyLoadout.Drone)
	{
		DestroyDroneSelectionTestContext(EmptyLoadout);
		return false;
	}

	const int32 EmptyPulseCountBeforeDeath = EmptyLoadout.Inventory->GetCurrentCount(PulseLaser);
	EmptyLoadout.PC->Server_RequestReadyForRaid_Implementation();
	EmptyLoadout.Drone->ApplyDamageForServer(999, FName(TEXT("Automation")));
	TestTrue(TEXT("default drone can die without equipped parts"), EmptyLoadout.Drone->IsDead());
	TestEqual(TEXT("default drone death does not change weapon stock"),
		EmptyLoadout.Inventory->GetCurrentCount(PulseLaser),
		EmptyPulseCountBeforeDeath);
	TestFalse(TEXT("default drone death leaves equipped return as no-op"),
		EmptyLoadout.PC->ReturnEquippedPartsForServer(EDronePartReturnReason::Disconnect));
	DestroyDroneSelectionTestContext(EmptyLoadout);

	UDronePartReturnManager* PartialReturnManager = nullptr;
	FDroneSelectionTestContext PartialLoadout = CreateDroneReturnTestContext(TEXT("DroneDeathReturnPartialLoadoutWorld"), PartialReturnManager);
	TestNotNull(TEXT("partial loadout world is created"), PartialLoadout.World);
	TestNotNull(TEXT("partial loadout inventory is spawned"), PartialLoadout.Inventory);
	TestNotNull(TEXT("partial loadout player controller is spawned"), PartialLoadout.PC);
	TestNotNull(TEXT("partial loadout drone is spawned"), PartialLoadout.Drone);
	if (!PartialLoadout.World || !PartialLoadout.Inventory || !PartialLoadout.PC || !PartialLoadout.Drone)
	{
		DestroyDroneSelectionTestContext(PartialLoadout);
		return false;
	}

	TestTrue(TEXT("partial death test consumes one weapon"), PartialLoadout.Inventory->TryConsumePart(PulseLaser));
	PartialLoadout.PC->SetSelectedPartIDForSlotForServer(EPartSlot::LeftWeapon, PulseLaser);
	PartialLoadout.PC->Server_RequestReadyForRaid_Implementation();
	PartialLoadout.Drone->ApplyDamageForServer(999, FName(TEXT("Automation")));
	TestTrue(TEXT("partial loadout drone dies"), PartialLoadout.Drone->IsDead());
	TestEqual(TEXT("partial death clears left weapon only"),
		PartialLoadout.PC->GetEquippedPartIDBySlot(EPartSlot::LeftWeapon),
		NAME_None);
	TestEqual(TEXT("partial death leaves empty core slot empty"),
		PartialLoadout.PC->GetEquippedPartIDBySlot(EPartSlot::Core),
		NAME_None);
	TestEqual(TEXT("partial death restores the one equipped weapon"),
		PartialLoadout.Inventory->GetCurrentCount(PulseLaser),
		PartialLoadout.Inventory->GetMaxCount(PulseLaser));
	DestroyDroneSelectionTestContext(PartialLoadout);

	UDronePartReturnManager* DuplicateReturnManager = nullptr;
	FDroneSelectionTestContext DuplicateLoadout = CreateDroneReturnTestContext(TEXT("DroneDeathReturnDuplicateWeaponWorld"), DuplicateReturnManager);
	TestNotNull(TEXT("duplicate loadout world is created"), DuplicateLoadout.World);
	TestNotNull(TEXT("duplicate loadout inventory is spawned"), DuplicateLoadout.Inventory);
	TestNotNull(TEXT("duplicate loadout player controller is spawned"), DuplicateLoadout.PC);
	TestNotNull(TEXT("duplicate loadout drone is spawned"), DuplicateLoadout.Drone);
	if (!DuplicateLoadout.World || !DuplicateLoadout.Inventory || !DuplicateLoadout.PC || !DuplicateLoadout.Drone)
	{
		DestroyDroneSelectionTestContext(DuplicateLoadout);
		return false;
	}

	TestTrue(TEXT("duplicate death test consumes first left/right weapon"), DuplicateLoadout.Inventory->TryConsumePart(PulseLaser));
	TestTrue(TEXT("duplicate death test consumes second left/right weapon"), DuplicateLoadout.Inventory->TryConsumePart(PulseLaser));
	DuplicateLoadout.PC->SetSelectedPartIDForSlotForServer(EPartSlot::LeftWeapon, PulseLaser);
	DuplicateLoadout.PC->SetSelectedPartIDForSlotForServer(EPartSlot::RightWeapon, PulseLaser);
	DuplicateLoadout.PC->Server_RequestReadyForRaid_Implementation();
	TestEqual(TEXT("duplicate weapons consume two stock entries before death"),
		DuplicateLoadout.Inventory->GetCurrentCount(PulseLaser),
		DuplicateLoadout.Inventory->GetMaxCount(PulseLaser) - 2);

	DuplicateLoadout.Drone->ApplyDamageForServer(999, FName(TEXT("Automation")));
	TestTrue(TEXT("duplicate weapon drone dies"), DuplicateLoadout.Drone->IsDead());
	TestEqual(TEXT("duplicate death clears left duplicate weapon"),
		DuplicateLoadout.PC->GetEquippedPartIDBySlot(EPartSlot::LeftWeapon),
		NAME_None);
	TestEqual(TEXT("duplicate death clears right duplicate weapon"),
		DuplicateLoadout.PC->GetEquippedPartIDBySlot(EPartSlot::RightWeapon),
		NAME_None);
	TestEqual(TEXT("duplicate left/right weapons return exactly two stock entries without exceeding max"),
		DuplicateLoadout.Inventory->GetCurrentCount(PulseLaser),
		DuplicateLoadout.Inventory->GetMaxCount(PulseLaser));
	DestroyDroneSelectionTestContext(DuplicateLoadout);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRaidEndReturnTest,
	"DroneProto.D6.RaidGameMode.RaidEndReturnClearsInBattlePlayers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRaidEndReturnTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("RaidEndReturnTestWorld")));
	TestNotNull(TEXT("raid end test world is created"), World);
	if (!World)
	{
		return false;
	}

	ARaidGameMode* GameMode = World->SpawnActor<ARaidGameMode>();
	ARaidGameState* GameState = World->SpawnActor<ARaidGameState>();
	ADronePartInventory* Inventory = World->SpawnActor<ADronePartInventory>();
	ARaidPlayerController* BattlePC = World->SpawnActor<ARaidPlayerController>();
	ADrone* BattleDrone = World->SpawnActor<ADrone>();
	ARaidPlayerController* SelectingPC = World->SpawnActor<ARaidPlayerController>();
	ADrone* SelectingDrone = World->SpawnActor<ADrone>();

	TestNotNull(TEXT("raid end game mode is spawned"), GameMode);
	TestNotNull(TEXT("raid end game state is spawned"), GameState);
	TestNotNull(TEXT("raid end inventory is spawned"), Inventory);
	TestNotNull(TEXT("battle player controller is spawned"), BattlePC);
	TestNotNull(TEXT("battle drone is spawned"), BattleDrone);
	TestNotNull(TEXT("selecting player controller is spawned"), SelectingPC);
	TestNotNull(TEXT("selecting drone is spawned"), SelectingDrone);
	if (!GameMode || !GameState || !Inventory || !BattlePC || !BattleDrone || !SelectingPC || !SelectingDrone)
	{
		World->DestroyWorld(false);
		return false;
	}

	World->SetGameState(GameState);
	GameState->SetDronePartInventory(Inventory);
	World->AddController(BattlePC);
	World->AddController(SelectingPC);
	BattlePC->Possess(BattleDrone);
	SelectingPC->Possess(SelectingDrone);

	const FName CoreZenith = ADronePartInventory::GetCoreZenithPartID();
	const FName PulseLaser = ADronePartInventory::GetPulseLaserPartID();
	const FName VectorCannon = ADronePartInventory::GetVectorCannonPartID();

	TestTrue(TEXT("battle raid end test consumes left weapon"), Inventory->TryConsumePart(PulseLaser));
	TestTrue(TEXT("battle raid end test consumes right weapon"), Inventory->TryConsumePart(VectorCannon));
	BattlePC->SetSelectedPartIDForSlotForServer(EPartSlot::LeftWeapon, PulseLaser);
	BattlePC->SetSelectedPartIDForSlotForServer(EPartSlot::RightWeapon, VectorCannon);
	BattlePC->Server_RequestReadyForRaid_Implementation();
	ARaidBoss* Boss = World->SpawnActor<ARaidBoss>();
	TestNotNull(TEXT("raid end boss is spawned"), Boss);
	if (Boss)
	{
		GameState->SetRaidBossForServer(Boss);
	}
	TestEqual(TEXT("battle player is InBattle before RaidEnd"),
		BattlePC->GetCurrentSelectionState(),
		EPlayerSelectionState::InBattle);

	TestTrue(TEXT("selecting raid end test consumes selected core"), Inventory->TryConsumePart(CoreZenith));
	SelectingPC->SetSelectedPartIDForSlotForServer(EPartSlot::Core, CoreZenith);
	TestEqual(TEXT("selecting player stays Selecting before RaidEnd"),
		SelectingPC->GetCurrentSelectionState(),
		EPlayerSelectionState::Selecting);

	GameMode->ReturnAllEquippedPartsForRaidEnd(FName(TEXT("Automation")));

	TestEqual(TEXT("RaidEnd clears battle left weapon"),
		BattlePC->GetEquippedPartIDBySlot(EPartSlot::LeftWeapon),
		NAME_None);
	TestEqual(TEXT("RaidEnd clears battle right weapon"),
		BattlePC->GetEquippedPartIDBySlot(EPartSlot::RightWeapon),
		NAME_None);
	TestTrue(TEXT("RaidEnd moves battle player out of InBattle"),
		BattlePC->GetCurrentSelectionState() != EPlayerSelectionState::InBattle);
	TestEqual(TEXT("RaidEnd returns battle left weapon stock"),
		Inventory->GetCurrentCount(PulseLaser),
		Inventory->GetMaxCount(PulseLaser));
	TestEqual(TEXT("RaidEnd returns battle right weapon stock"),
		Inventory->GetCurrentCount(VectorCannon),
		Inventory->GetMaxCount(VectorCannon));
	TestEqual(TEXT("RaidEnd does not return selecting player's selected core"),
		Inventory->GetCurrentCount(CoreZenith),
		Inventory->GetMaxCount(CoreZenith) - 1);
	TestEqual(TEXT("RaidEnd leaves selecting player's selected core intact"),
		SelectingPC->GetSelectedPartIDBySlot(EPartSlot::Core),
		CoreZenith);

	if (Boss)
	{
		const float BossHPAfterRaidEnd = Boss->GetCurrentHP();
		BattleDrone->RequestAttackBoss();
		TestTrue(TEXT("RaidEnd attack input does not damage boss"),
			FMath::IsNearlyEqual(Boss->GetCurrentHP(), BossHPAfterRaidEnd, 0.01f));
	}

	TestFalse(TEXT("RaidEnd move input is ignored before server movement"),
		BattleDrone->ApplyMoveInputForServerForTest(FVector2D(1.0f, 0.0f)));
	const FVector BattleLocationAfterRaidEnd = BattleDrone->GetActorLocation();
	TestFalse(TEXT("RaidEnd pending move is not applied"),
		BattleDrone->ApplyPendingServerMoveInputForTest(0.10f));
	TestTrue(TEXT("RaidEnd pending move leaves location unchanged"),
		BattleDrone->GetActorLocation().Equals(BattleLocationAfterRaidEnd, 0.1f));

	BattlePC->SetDronePartReturnManagerForTest(GameMode->GetDronePartReturnManager());
	TestFalse(TEXT("logout after RaidEnd skips because equipped slots were cleared"),
		BattlePC->ReturnEquippedPartsForServer(EDronePartReturnReason::Disconnect));
	TestEqual(TEXT("logout after RaidEnd does not over-return left weapon"),
		Inventory->GetCurrentCount(PulseLaser),
		Inventory->GetMaxCount(PulseLaser));
	TestEqual(TEXT("logout after RaidEnd does not over-return right weapon"),
		Inventory->GetCurrentCount(VectorCannon),
		Inventory->GetMaxCount(VectorCannon));

	GameMode->ReturnAllEquippedPartsForRaidEnd(FName(TEXT("AutomationRetry")));
	TestEqual(TEXT("second RaidEnd does not over-return left weapon"),
		Inventory->GetCurrentCount(PulseLaser),
		Inventory->GetMaxCount(PulseLaser));
	TestEqual(TEXT("second RaidEnd does not over-return right weapon"),
		Inventory->GetCurrentCount(VectorCannon),
		Inventory->GetMaxCount(VectorCannon));

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
	FString DroneReportWidgetSource;
	FString DroneSource;
	FString RaidBossSource;

	if (!LoadSourceFile(TEXT("Raid/RaidGameMode.cpp"), RaidGameModeSource)
		|| !LoadSourceFile(TEXT("Raid/RaidPlayerController.cpp"), RaidPlayerControllerSource)
		|| !LoadSourceFile(TEXT("Raid/DronePartReturnManager.cpp"), DronePartReturnManagerSource)
		|| !LoadSourceFile(TEXT("Raid/DronePartSelectWidget.cpp"), DronePartSelectWidgetSource)
		|| !LoadSourceFile(TEXT("Raid/DroneReportWidget.cpp"), DroneReportWidgetSource)
		|| !LoadSourceFile(TEXT("Drone.cpp"), DroneSource)
		|| !LoadSourceFile(TEXT("Raid/RaidBoss.cpp"), RaidBossSource))
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
	TestTrue(TEXT("ready ignored dead pawn summary log marker exists"),
		RaidPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] ReadyIgnored PC=")));
	TestTrue(TEXT("return summary log marker exists"),
		DronePartReturnManagerSource.Contains(TEXT("ToReturnSummaryLogName")));
	TestTrue(TEXT("attack calculation summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] AttackCalc Player=")));
	TestTrue(TEXT("attack accepted summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] Attack Accepted: Player=")));
	TestTrue(TEXT("attack ignored summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] AttackIgnored PC=")));
	TestTrue(TEXT("not in battle attack ignored summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] Attack Ignored: Reason=NotInBattle")));
	TestTrue(TEXT("raid end attack ignored summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] Attack Ignored: Reason=RaidEnd")));
	TestTrue(TEXT("boss dead attack ignored summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] Attack Ignored: Reason=BossDead")));
	TestTrue(TEXT("no boss attack failed summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] Attack Failed: Reason=NoBoss")));
	TestTrue(TEXT("invalid pawn attack failed summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] Attack Failed: Reason=InvalidPawn")));
	TestTrue(TEXT("attack path checks PlayerSelectionState"),
		DroneSource.Contains(TEXT("GetPlayerSelectionState()")));
	TestTrue(TEXT("boss damage summary log marker exists"),
		RaidBossSource.Contains(TEXT("[DR_SUMMARY] BossDamage: OldHP=")));
	TestTrue(TEXT("boss death summary log marker exists"),
		RaidBossSource.Contains(TEXT("[DR_SUMMARY] BossDeath:")));
	TestTrue(TEXT("dead boss damage ignored summary log marker exists"),
		RaidBossSource.Contains(TEXT("[DR_SUMMARY] BossDamageIgnored: Reason=BossDead")));
	TestTrue(TEXT("boss max HP is replicated with current HP"),
		RaidBossSource.Contains(TEXT("DOREPLIFETIME(ARaidBoss, MaxHP)")));
	TestTrue(TEXT("selection timer start summary log marker exists"),
		RaidPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] SelectTimerStart PC=")));
	TestTrue(TEXT("selection timer stop summary log marker exists"),
		RaidPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] SelectTimerStop PC=")));
	TestTrue(TEXT("auto ready summary log marker exists"),
		RaidPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] AutoReady PC=")));
	TestTrue(TEXT("auto ready dead pawn ignored reason marker exists"),
		RaidPlayerControllerSource.Contains(TEXT("Result=Ignored Reason=DeadPawn")));
	TestTrue(TEXT("D6 kill drone target summary marker exists"),
		RaidPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] D6KillDrone RequestPC=")));
	TestTrue(TEXT("ui refresh summary log marker exists"),
		RaidPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] UIRefresh PC=")));
	TestTrue(TEXT("death return summary log marker exists"),
		DronePartReturnManagerSource.Contains(TEXT("DeathReturn")));
	TestTrue(TEXT("drone damage summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] DroneDamage PC=")));
	TestTrue(TEXT("drone death summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] DroneDeath PC=")));
	TestTrue(TEXT("raid end summary log marker exists"),
		RaidGameModeSource.Contains(TEXT("[DR_SUMMARY] RaidEnd Reason=")));
	TestTrue(TEXT("raid end return summary log marker exists"),
		DronePartReturnManagerSource.Contains(TEXT("RaidEndReturn")));
	TestTrue(TEXT("return skipped summary log marker exists"),
		DronePartReturnManagerSource.Contains(TEXT("[DR_SUMMARY] ReturnSkipped PC=")));
	TestTrue(TEXT("dead input ignored summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] DeadInputIgnored PC=")));
	TestTrue(TEXT("dead attack ignored marker exists"),
		DroneSource.Contains(TEXT("TEXT(\"Attack\")")));
	TestTrue(TEXT("dead move ignored marker exists"),
		DroneSource.Contains(TEXT("TEXT(\"Move\")")));
	TestTrue(TEXT("dead dodge ignored marker exists"),
		DroneSource.Contains(TEXT("TEXT(\"Dodge\")")));
	TestTrue(TEXT("dead heal ignored marker exists"),
		DroneSource.Contains(TEXT("TEXT(\"Heal\")")));
	TestTrue(TEXT("weapon calculation summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] WeaponCalc Player=")));
	TestTrue(TEXT("core calculation summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] CoreCalc Player=")));
	TestTrue(TEXT("Drain heal summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] DrainHeal PC=")));
	TestTrue(TEXT("combat record summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] CombatRecord Player=")));
	TestTrue(TEXT("report created summary log marker exists"),
		RaidPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] ReportCreated Player=")));
	TestTrue(TEXT("bonus calculation summary log marker exists"),
		RaidPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] BonusCalc Player=")));
	TestTrue(TEXT("grade calculation summary log marker exists"),
		RaidPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] GradeCalc Player=")));
	TestTrue(TEXT("report duplicate summary log marker exists"),
		RaidPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] ReportDuplicateIgnored Player=")));
	TestTrue(TEXT("report client received summary log marker exists"),
		RaidPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] ReportClientReceived Player=")));
	TestTrue(TEXT("report widget shown summary log marker exists"),
		RaidPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] ReportWidgetShown Player=")));
	TestTrue(TEXT("report widget refreshed summary log marker exists"),
		RaidPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] ReportWidgetRefreshed Player=")));
	TestTrue(TEXT("report widget missing class summary log marker exists"),
		RaidPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] ReportWidgetMissingClass Player=")));
	TestTrue(TEXT("report widget hidden summary log marker exists"),
		RaidPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] ReportWidgetHidden Player=")));
	TestFalse(TEXT("report widget does not expose ReportScore getter"),
		DroneReportWidgetSource.Contains(TEXT("GetReportScoreText")));
	TestTrue(TEXT("return after report summary log marker exists"),
		RaidGameModeSource.Contains(TEXT("[DR_SUMMARY] ReturnAfterReport Player="))
		&& DroneSource.Contains(TEXT("[DR_SUMMARY] ReturnAfterReport Player=")));
	TestTrue(TEXT("move input summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] MoveInput PC=")));
	TestTrue(TEXT("server move applied summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] ServerMoveApplied PC=")));
	TestTrue(TEXT("server move ignored summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] ServerMoveIgnored PC=")));
	TestTrue(TEXT("owner-only move sync is registered"),
		DroneSource.Contains(TEXT("DOREPLIFETIME_CONDITION(ADrone, OwnerMoveSync, COND_OwnerOnly)")));
	TestTrue(TEXT("owner move sync sent summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] OwnerMoveSyncSent PC=")));
	TestTrue(TEXT("owner move corrected summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] OwnerMoveCorrected PC=")));
	TestTrue(TEXT("replicated location summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] ReplicatedLocation PC=")));
	TestTrue(TEXT("view target result summary marker exists"),
		DroneSource.Contains(TEXT("ViewTargetResult=")));
	TestTrue(TEXT("view target matched pawn result marker exists"),
		DroneSource.Contains(TEXT("ViewTargetMatchesPawn")));
	TestTrue(TEXT("simulated proxy view target result marker exists"),
		DroneSource.Contains(TEXT("SimProxyObserved")));
	TestTrue(TEXT("move audit summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] MoveAudit PC=")));
	TestTrue(TEXT("move distance summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] MoveDistance PC=")));
	TestTrue(TEXT("move distance ignored summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] MoveDistanceIgnored PC=")));
	TestTrue(TEXT("move distance reset summary log marker exists"),
		DroneSource.Contains(TEXT("[DR_SUMMARY] MoveDistanceReset PC=")));
	TestTrue(TEXT("drone movement replication is explicit"),
		DroneSource.Contains(TEXT("SetReplicateMovement(true)")));
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
	TestTrue(TEXT("raid end duplicate skip summary log marker exists"),
		RaidGameModeSource.Contains(TEXT("[DR_SUMMARY] RaidEndSkipped Reason=AlreadyEnded")));
	TestTrue(TEXT("raid end server state cleanup summary log marker exists"),
		RaidPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] RaidEndStateCleaned Player=")));
	TestTrue(TEXT("raid end client refresh summary log marker exists"),
		RaidPlayerControllerSource.Contains(TEXT("[DR_SUMMARY] RaidEndClientRefresh Player=")));
	TestTrue(TEXT("in battle UI summary reports equipped parts"),
		RaidPlayerControllerSource.Contains(TEXT("SummaryCorePartID")));

	return true;
}

#endif
