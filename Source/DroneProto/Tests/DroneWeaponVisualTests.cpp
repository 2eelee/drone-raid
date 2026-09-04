#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Drone.h"
#include "Raid/DroneDataTableRows.h"
#include "Raid/DronePartInventory.h"

#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneWeaponVisualRowContractTest,
	"DroneProto.Visual.Weapon.DataTableRowContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneWeaponVisualRowContractTest::RunTest(const FString& Parameters)
{
	UScriptStruct* RowStruct = FDroneWeaponVisualRow::StaticStruct();
	TestNotNull(TEXT("weapon visual row struct exists"), RowStruct);
	TestTrue(TEXT("weapon visual row derives from FTableRowBase"),
		RowStruct && RowStruct->IsChildOf(FTableRowBase::StaticStruct()));

	FObjectProperty* WeaponMeshProperty = FindFProperty<FObjectProperty>(RowStruct, TEXT("WeaponMesh"));
	TestNotNull(TEXT("WeaponMesh is a direct object property"), WeaponMeshProperty);
	TestTrue(TEXT("WeaponMesh accepts UStaticMesh assets"),
		WeaponMeshProperty
		&& WeaponMeshProperty->PropertyClass.Get() == UStaticMesh::StaticClass());
	TestNotNull(TEXT("LeftRelativeTransform field exists"),
		FindFProperty<FProperty>(RowStruct, TEXT("LeftRelativeTransform")));
	TestNotNull(TEXT("RightRelativeTransform field exists"),
		FindFProperty<FProperty>(RowStruct, TEXT("RightRelativeTransform")));
	TestNull(TEXT("row name is the weapon ID, so no WeaponID field exists"),
		FindFProperty<FProperty>(RowStruct, TEXT("WeaponID")));

	const FDroneWeaponVisualRow DefaultRow;
	TestTrue(TEXT("default left transform is identity"),
		DefaultRow.LeftRelativeTransform.Equals(FTransform::Identity));
	TestTrue(TEXT("default right transform is identity"),
		DefaultRow.RightRelativeTransform.Equals(FTransform::Identity));
	TestNull(TEXT("default mesh is unset"), DefaultRow.WeaponMesh.Get());

	UDataTable* Table = NewObject<UDataTable>();
	Table->RowStruct = RowStruct;
	Table->AddRow(FName(TEXT("WEAPON_001")), DefaultRow);
	Table->AddRow(FName(TEXT("WEAPON_002")), DefaultRow);
	Table->AddRow(FName(TEXT("WEAPON_003")), DefaultRow);
	TestEqual(TEXT("the three canonical weapon IDs can be used as row names"),
		Table->GetRowNames().Num(), 3);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneWeaponVisualReplicationContractTest,
	"DroneProto.Visual.Weapon.ReplicationContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneWeaponVisualReplicationContractTest::RunTest(const FString& Parameters)
{
	UClass* DroneClass = ADrone::StaticClass();
	FStructProperty* LoadoutProperty = FindFProperty<FStructProperty>(DroneClass, TEXT("WeaponVisualLoadout"));
	TestNotNull(TEXT("the paired weapon visual loadout property exists"), LoadoutProperty);
	TestTrue(TEXT("the replicated property contains the paired weapon visual struct"),
		LoadoutProperty
		&& LoadoutProperty->Struct.Get() == FDroneWeaponVisualLoadout::StaticStruct());

	const FName ExpectedRepNotify(TEXT("OnRep_WeaponVisualLoadout"));
	if (LoadoutProperty)
	{
		TestTrue(TEXT("the paired weapon visual loadout replicates to clients"),
			LoadoutProperty->HasAnyPropertyFlags(CPF_Net));
		TestEqual(TEXT("the paired weapon visual loadout uses one RepNotify"),
			LoadoutProperty->RepNotifyFunc, ExpectedRepNotify);
	}
	FProperty* GameplayLeftProperty = FindFProperty<FProperty>(DroneClass, TEXT("EquippedLeftWeaponPartID"));
	FProperty* GameplayRightProperty = FindFProperty<FProperty>(DroneClass, TEXT("EquippedRightWeaponPartID"));
	TestTrue(TEXT("the gameplay left weapon ID is not separately replicated"),
		GameplayLeftProperty && !GameplayLeftProperty->HasAnyPropertyFlags(CPF_Net));
	TestTrue(TEXT("the gameplay right weapon ID is not separately replicated"),
		GameplayRightProperty && !GameplayRightProperty->HasAnyPropertyFlags(CPF_Net));

	UFunction* VisualEvent = DroneClass->FindFunctionByName(TEXT("BP_OnWeaponVisualLoadoutChanged"));
	TestNotNull(TEXT("Blueprint weapon visual event exists"), VisualEvent);
	TestTrue(TEXT("weapon visual notification is a Blueprint event"),
		VisualEvent && VisualEvent->HasAnyFunctionFlags(FUNC_BlueprintEvent));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneWeaponVisualLoadoutLifecycleTest,
	"DroneProto.Visual.Weapon.LoadoutLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneWeaponVisualLoadoutLifecycleTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("DroneWeaponVisualLifecycleWorld")));
	ADrone* Drone = World ? World->SpawnActor<ADrone>() : nullptr;
	TestNotNull(TEXT("test world is created"), World);
	TestNotNull(TEXT("drone is spawned"), Drone);
	if (!World || !Drone)
	{
		if (World)
		{
			World->DestroyWorld(false);
		}
		return false;
	}

	const FName Pulse = ADronePartInventory::GetPulseLaserPartID();
	const FName Vector = ADronePartInventory::GetVectorCannonPartID();
	TestTrue(TEXT("left and right weapon loadout applies together"),
		Drone->ApplyLoadout(NAME_None, Pulse, Vector));
	TestEqual(TEXT("left visual source ID follows the equipped left weapon"),
		Drone->GetEquippedLeftWeaponPartIDForTest(), Pulse);
	TestEqual(TEXT("right visual source ID follows the equipped right weapon"),
		Drone->GetEquippedRightWeaponPartIDForTest(), Vector);

	Drone->ClearEquippedLoadoutForServer(FName(TEXT("WeaponVisualAutomation")));
	TestEqual(TEXT("clear resets the left visual source ID"),
		Drone->GetEquippedLeftWeaponPartIDForTest(), FName(NAME_None));
	TestEqual(TEXT("clear resets the right visual source ID"),
		Drone->GetEquippedRightWeaponPartIDForTest(), FName(NAME_None));

	World->DestroyWorld(false);
	return true;
}

#endif
