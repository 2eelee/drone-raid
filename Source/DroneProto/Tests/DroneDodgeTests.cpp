#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "CoreGlobals.h"

#include "Drone.h"
#include "Raid/DronePartInventory.h"
#include "Raid/RaidBoss.h"
#include "Raid/RaidGameState.h"
#include "Raid/RaidPlayerController.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
struct FDroneDodgeTestContext
{
	UWorld* World = nullptr;
	ARaidGameState* GameState = nullptr;
	ARaidPlayerController* PC = nullptr;
	ADrone* Drone = nullptr;
	ARaidBoss* Boss = nullptr;
};

FDroneDodgeTestContext CreateDroneDodgeTestContext(const TCHAR* WorldName, bool bSpawnBoss = true)
{
	FDroneDodgeTestContext Context;
	Context.World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
	if (!Context.World)
	{
		return Context;
	}

	Context.GameState = Context.World->SpawnActor<ARaidGameState>();
	Context.PC = Context.World->SpawnActor<ARaidPlayerController>();
	Context.Drone = Context.World->SpawnActor<ADrone>();
	Context.Boss = bSpawnBoss ? Context.World->SpawnActor<ARaidBoss>() : nullptr;

	if (Context.GameState)
	{
		Context.World->SetGameState(Context.GameState);
		if (Context.Boss)
		{
			Context.Boss->SetActorLocation(FVector::ZeroVector);
			Context.GameState->SetRaidBossForServer(Context.Boss);
		}
	}

	if (Context.PC && Context.Drone)
	{
		Context.PC->Possess(Context.Drone);
		Context.PC->SetControlRotation(FRotator::ZeroRotator);
	}

	return Context;
}

void DestroyDroneDodgeTestContext(FDroneDodgeTestContext& Context)
{
	if (Context.World)
	{
		Context.World->DestroyWorld(false);
	}
	Context = FDroneDodgeTestContext();
}

bool PrepareInBattleDodgeTest(FAutomationTestBase& Test, FDroneDodgeTestContext& Context, const TCHAR* Label)
{
	Test.TestNotNull(FString::Printf(TEXT("%s world is created"), Label), Context.World);
	Test.TestNotNull(FString::Printf(TEXT("%s game state is spawned"), Label), Context.GameState);
	Test.TestNotNull(FString::Printf(TEXT("%s player controller is spawned"), Label), Context.PC);
	Test.TestNotNull(FString::Printf(TEXT("%s drone is spawned"), Label), Context.Drone);
	if (!Context.World || !Context.GameState || !Context.PC || !Context.Drone)
	{
		return false;
	}

	Context.PC->Server_RequestReadyForRaid_Implementation();
	Test.TestEqual(FString::Printf(TEXT("%s player enters battle"), Label),
		Context.PC->GetCurrentSelectionState(),
		EPlayerSelectionState::InBattle);
	return Context.PC->GetCurrentSelectionState() == EPlayerSelectionState::InBattle;
}

void TickDodgeForDodgeTest(FDroneDodgeTestContext& Context, float DurationSeconds)
{
	if (!Context.World || DurationSeconds <= 0.0f)
	{
		return;
	}

	++GFrameCounter;
	if (Context.Drone)
	{
		Context.Drone->TickForTest(DurationSeconds);
	}
	++GFrameCounter;
	Context.World->GetTimerManager().Tick(DurationSeconds + 0.001f);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneDodgeInvalidStatesTest,
	"DroneProto.D17.Drone.DodgeInvalidStates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneDodgeInvalidStatesTest::RunTest(const FString& Parameters)
{
	FDroneDodgeTestContext Selecting = CreateDroneDodgeTestContext(TEXT("DroneDodgeSelectingWorld"));
	TestNotNull(TEXT("selecting dodge drone is spawned"), Selecting.Drone);
	if (Selecting.Drone)
	{
		TestFalse(TEXT("Selecting state rejects dodge"),
			Selecting.Drone->RequestDodgeForServer(FVector2D(1.0f, 0.0f)));
	}
	DestroyDroneDodgeTestContext(Selecting);

	FDroneDodgeTestContext NoDirection = CreateDroneDodgeTestContext(TEXT("DroneDodgeNoDirectionWorld"));
	if (!PrepareInBattleDodgeTest(*this, NoDirection, TEXT("no direction dodge")))
	{
		DestroyDroneDodgeTestContext(NoDirection);
		return false;
	}
	TestFalse(TEXT("dodge rejects C without a current direction"),
		NoDirection.Drone->RequestDodgeForServer(FVector2D::ZeroVector));
	DestroyDroneDodgeTestContext(NoDirection);

	FDroneDodgeTestContext Report = CreateDroneDodgeTestContext(TEXT("DroneDodgeReportWorld"));
	if (!PrepareInBattleDodgeTest(*this, Report, TEXT("report dodge")))
	{
		DestroyDroneDodgeTestContext(Report);
		return false;
	}
	TestTrue(TEXT("report is generated for dodge gate"),
		Report.PC->TryCreateDroneReportForServer(EDroneReportTrigger::RaidTimeLimit, false));
	TestFalse(TEXT("Report state rejects dodge"),
		Report.Drone->RequestDodgeForServer(FVector2D(1.0f, 0.0f)));
	DestroyDroneDodgeTestContext(Report);

	FDroneDodgeTestContext Dead = CreateDroneDodgeTestContext(TEXT("DroneDodgeDeadWorld"));
	if (!PrepareInBattleDodgeTest(*this, Dead, TEXT("dead dodge")))
	{
		DestroyDroneDodgeTestContext(Dead);
		return false;
	}
	Dead.Drone->ApplyDamageForServer(Dead.Drone->GetMaxHealth() + 100, FName(TEXT("Automation")));
	TestTrue(TEXT("dead dodge setup kills the drone"), Dead.Drone->IsDead());
	TestFalse(TEXT("Dead state rejects dodge"),
		Dead.Drone->RequestDodgeForServer(FVector2D(1.0f, 0.0f)));
	DestroyDroneDodgeTestContext(Dead);

	FDroneDodgeTestContext Loading = CreateDroneDodgeTestContext(TEXT("DroneDodgeServerLoadingWorld"), false);
	if (!PrepareInBattleDodgeTest(*this, Loading, TEXT("server loading dodge")))
	{
		DestroyDroneDodgeTestContext(Loading);
		return false;
	}
	Loading.GameState->SetRaidStateForServer(ERaidState::Waiting);
	TestFalse(TEXT("ServerLoading state rejects dodge"),
		Loading.Drone->RequestDodgeForServer(FVector2D(1.0f, 0.0f)));
	DestroyDroneDodgeTestContext(Loading);

	FDroneDodgeTestContext Attacking = CreateDroneDodgeTestContext(TEXT("DroneDodgeAttackingWorld"));
	if (!PrepareInBattleDodgeTest(*this, Attacking, TEXT("attacking dodge")))
	{
		DestroyDroneDodgeTestContext(Attacking);
		return false;
	}
	Attacking.Drone->SetIsAttackingForTest(true);
	TestFalse(TEXT("Attacking state rejects dodge"),
		Attacking.Drone->RequestDodgeForServer(FVector2D(1.0f, 0.0f)));
	Attacking.Drone->SetIsAttackingForTest(false);
	DestroyDroneDodgeTestContext(Attacking);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneDodgeTimingCooldownAndAttackBlockTest,
	"DroneProto.D17.Drone.DodgeTimingCooldownAndAttackBlock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneDodgeTimingCooldownAndAttackBlockTest::RunTest(const FString& Parameters)
{
	FDroneDodgeTestContext Context = CreateDroneDodgeTestContext(TEXT("DroneDodgeTimingWorld"));
	if (!PrepareInBattleDodgeTest(*this, Context, TEXT("timing dodge")))
	{
		DestroyDroneDodgeTestContext(Context);
		return false;
	}

	TestTrue(TEXT("dodge test weapon loadout applies"),
		Context.Drone->ApplyLoadout(NAME_None, ADronePartInventory::GetFractureBurstPartID(), NAME_None));
	Context.Drone->SetActorLocation(FVector(1000.0f, 1000.0f, 0.0f));
	Context.Drone->ResetAccumulatedMoveDistanceForServer();
	UStaticMeshComponent* TestVisual = NewObject<UStaticMeshComponent>(Context.Drone, TEXT("DodgeInvincibleVisualTestMesh"));
	TestNotNull(TEXT("dodge visual test mesh is created"), TestVisual);
	if (TestVisual)
	{
		TestVisual->SetupAttachment(Context.Drone->GetRootComponent());
		TestVisual->RegisterComponentWithWorld(Context.World);
		TestVisual->SetVisibility(true, true);
	}
	const int32 HealthBeforeDodgeDamage = Context.Drone->GetHealth();

	TestTrue(TEXT("valid dodge request succeeds"),
		Context.Drone->RequestDodgeForServer(FVector2D(1.0f, 0.0f)));
	TestTrue(TEXT("dodge state starts immediately"),
		Context.Drone->IsDodgingForTest());
	TestTrue(TEXT("invincibility starts immediately"),
		Context.Drone->IsInvincibleForTest());
	TestTrue(TEXT("dodge start does not instantly teleport to the end location"),
		FMath::IsNearlyEqual(Context.Drone->GetActorLocation().X, 1000.0f, 0.1f));
	if (TestVisual)
	{
		TestFalse(TEXT("dodge invincible window hides the visual mesh"),
			TestVisual->IsVisible());
	}

	TestFalse(TEXT("already dodging rejects another dodge"),
		Context.Drone->RequestDodgeForServer(FVector2D(1.0f, 0.0f)));

	Context.Drone->ApplyDamageForServer(10, FName(TEXT("Automation")));
	TestEqual(TEXT("invincible window ignores damage"),
		Context.Drone->GetHealth(),
		HealthBeforeDodgeDamage);

	const float BossHPBeforeAttack = Context.Boss ? Context.Boss->GetCurrentHP() : 0.0f;
	Context.Drone->RequestAttackBoss();
	if (Context.Boss)
	{
		TestEqual(TEXT("attack during dodge is ignored"),
			Context.Boss->GetCurrentHP(),
			BossHPBeforeAttack);
	}

	TickDodgeForDodgeTest(Context, 0.16f);
	TestFalse(TEXT("invincibility ends after 0.15 seconds"),
		Context.Drone->IsInvincibleForTest());
	TestTrue(TEXT("dodge state remains during the last 0.10 seconds"),
		Context.Drone->IsDodgingForTest());
	if (TestVisual)
	{
		TestTrue(TEXT("dodge visual mesh is shown when invincibility ends"),
			TestVisual->IsVisible());
	}
	TestTrue(TEXT("dodge is interpolating between start and target halfway through"),
		Context.Drone->GetActorLocation().X > 1000.0f
		&& Context.Drone->GetActorLocation().X < 1600.0f);

	Context.Drone->ApplyDamageForServer(10, FName(TEXT("Automation")));
	TestEqual(TEXT("damage applies after invincibility ends"),
		Context.Drone->GetHealth(),
		HealthBeforeDodgeDamage - 10);

	TickDodgeForDodgeTest(Context, 0.10f);
	TestFalse(TEXT("dodge state ends after 0.25 seconds"),
		Context.Drone->IsDodgingForTest());
	TestTrue(TEXT("dodge reaches the target location after duration"),
		FMath::IsNearlyEqual(Context.Drone->GetActorLocation().X, 1600.0f, 0.1f));
	TestTrue(TEXT("cooldown starts when dodge ends"),
		Context.Drone->GetDodgeCooldownRemaining() > 1.0f);
	TestFalse(TEXT("cooldown rejects immediate dodge after end"),
		Context.Drone->RequestDodgeForServer(FVector2D(1.0f, 0.0f)));

	Context.Drone->SetLastDodgeEndTimeForTest(-1.21f);
	TestTrue(TEXT("dodge succeeds after end-based cooldown expires"),
		Context.Drone->RequestDodgeForServer(FVector2D(1.0f, 0.0f)));

	DestroyDroneDodgeTestContext(Context);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneDodgeBoundaryAndDistanceTest,
	"DroneProto.D17.Drone.DodgeBoundaryAndDistance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneDodgeBoundaryAndDistanceTest::RunTest(const FString& Parameters)
{
	FDroneDodgeTestContext Normal = CreateDroneDodgeTestContext(TEXT("DroneDodgeDistanceWorld"));
	if (!PrepareInBattleDodgeTest(*this, Normal, TEXT("dodge distance")))
	{
		DestroyDroneDodgeTestContext(Normal);
		return false;
	}

	Normal.Drone->SetActorLocation(FVector(1000.0f, 1000.0f, 777.0f));
	Normal.Drone->ResetAccumulatedMoveDistanceForServer();
	TestTrue(TEXT("normal dodge succeeds"),
		Normal.Drone->RequestDodgeForServer(FVector2D(1.0f, 0.0f)));
	TestTrue(TEXT("normal dodge does not instantly move to the target"),
		FMath::IsNearlyEqual(Normal.Drone->GetActorLocation().X, 1000.0f, 0.1f));
	TickDodgeForDodgeTest(Normal, 0.125f);
	TestTrue(TEXT("normal dodge halfway position is between start and target"),
		Normal.Drone->GetActorLocation().X > 1000.0f
		&& Normal.Drone->GetActorLocation().X < 1600.0f);
	TickDodgeForDodgeTest(Normal, 0.125f);
	TestTrue(TEXT("normal dodge distance is 6m in XY"),
		FMath::IsNearlyEqual(Normal.Drone->GetAccumulatedMoveDistance(), 6.0f, 0.001f));
	TestTrue(TEXT("normal dodge adds Vector movement distance"),
		FMath::IsNearlyEqual(Normal.Drone->GetVectorAccumulatedMoveDistanceForTest(), 6.0f, 0.001f));
	TestTrue(TEXT("normal dodge adds Booster movement distance"),
		FMath::IsNearlyEqual(Normal.Drone->GetBoosterAccumulatedMoveDistanceForTest(), 6.0f, 0.001f));
	TestTrue(TEXT("dodge keeps Z locked to the combat plane"),
		FMath::IsNearlyEqual(Normal.Drone->GetActorLocation().Z, 0.0f, 0.1f));
	DestroyDroneDodgeTestContext(Normal);

	FDroneDodgeTestContext WorldVector = CreateDroneDodgeTestContext(TEXT("DroneDodgeWorldVectorWorld"));
	if (!PrepareInBattleDodgeTest(*this, WorldVector, TEXT("dodge world vector")))
	{
		DestroyDroneDodgeTestContext(WorldVector);
		return false;
	}

	WorldVector.PC->SetControlRotation(FRotator(0.0f, 90.0f, 0.0f));
	WorldVector.Drone->SetActorLocation(FVector(1000.0f, 1000.0f, 0.0f));
	const FVector WorldVectorStart = WorldVector.Drone->GetActorLocation();
	TestTrue(TEXT("world-vector dodge succeeds"),
		WorldVector.Drone->RequestDodgeForServer(FVector2D(1.0f, 0.0f)));
	TickDodgeForDodgeTest(WorldVector, 0.25f);
	const FVector WorldVectorDelta = WorldVector.Drone->GetActorLocation() - WorldVectorStart;
	TestTrue(TEXT("server treats dodge input as a requested world X/Y vector, independent of control yaw"),
		FMath::IsNearlyEqual(WorldVectorDelta.X, 600.0f, 0.1f)
		&& FMath::IsNearlyZero(WorldVectorDelta.Y, 0.1f));
	DestroyDroneDodgeTestContext(WorldVector);

	FDroneDodgeTestContext Boundary = CreateDroneDodgeTestContext(TEXT("DroneDodgeBoundaryWorld"));
	if (!PrepareInBattleDodgeTest(*this, Boundary, TEXT("dodge boundary")))
	{
		DestroyDroneDodgeTestContext(Boundary);
		return false;
	}

	Boundary.Drone->SetActorLocation(FVector(4990.0f, 0.0f, 0.0f));
	Boundary.Drone->ResetAccumulatedMoveDistanceForServer();
	TestTrue(TEXT("boundary dodge succeeds"),
		Boundary.Drone->RequestDodgeForServer(FVector2D(1.0f, 0.0f)));
	TickDodgeForDodgeTest(Boundary, 0.25f);
	TestTrue(TEXT("boundary dodge cannot leave the 50m circle"),
		FVector::Dist2D(Boundary.Drone->GetActorLocation(), FVector::ZeroVector) <= 5000.0f + 0.1f);
	TestTrue(TEXT("boundary-clamped dodge records actual distance only"),
		Boundary.Drone->GetAccumulatedMoveDistance() < 6.0f);
	TestTrue(TEXT("boundary-clamped dodge records the 2D clamp distance"),
		FMath::IsNearlyEqual(Boundary.Drone->GetAccumulatedMoveDistance(), 0.10f, 0.01f));

	DestroyDroneDodgeTestContext(Boundary);

	FDroneDodgeTestContext BossMin = CreateDroneDodgeTestContext(TEXT("DroneDodgeBossMinDistanceWorld"));
	if (!PrepareInBattleDodgeTest(*this, BossMin, TEXT("dodge boss min distance")))
	{
		DestroyDroneDodgeTestContext(BossMin);
		return false;
	}

	BossMin.Drone->SetActorLocation(FVector(600.0f, 0.0f, 0.0f));
	BossMin.Drone->ResetAccumulatedMoveDistanceForServer();
	TestTrue(TEXT("boss-min dodge succeeds"),
		BossMin.Drone->RequestDodgeForServer(FVector2D(-1.0f, 0.0f)));
	TickDodgeForDodgeTest(BossMin, 0.25f);
	TestTrue(TEXT("boss-min dodge cannot enter the 5m boss center exclusion"),
		FVector::Dist2D(BossMin.Drone->GetActorLocation(), FVector::ZeroVector) >= 500.0f - 0.1f);
	TestTrue(TEXT("boss-min-clamped dodge records actual distance only"),
		FMath::IsNearlyEqual(BossMin.Drone->GetAccumulatedMoveDistance(), 1.0f, 0.01f));
	TestTrue(TEXT("boss-min-clamped dodge adds actual Vector movement distance"),
		FMath::IsNearlyEqual(BossMin.Drone->GetVectorAccumulatedMoveDistanceForTest(), 1.0f, 0.01f));
	TestTrue(TEXT("boss-min-clamped dodge adds actual Booster movement distance"),
		FMath::IsNearlyEqual(BossMin.Drone->GetBoosterAccumulatedMoveDistanceForTest(), 1.0f, 0.01f));

	DestroyDroneDodgeTestContext(BossMin);
	return true;
}

#endif
