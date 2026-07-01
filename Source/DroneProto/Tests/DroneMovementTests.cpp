#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Drone.h"
#include "Raid/RaidBoss.h"
#include "Raid/RaidGameState.h"
#include "Raid/RaidPlayerController.h"

#include "Engine/World.h"
#include "UObject/UnrealType.h"

namespace
{
struct FDroneMovementTestContext
{
	UWorld* World = nullptr;
	ARaidGameState* GameState = nullptr;
	ARaidPlayerController* PC = nullptr;
	ADrone* Drone = nullptr;
	ARaidBoss* Boss = nullptr;
};

FDroneMovementTestContext CreateDroneMovementTestContext(const TCHAR* WorldName, bool bSpawnBoss = true)
{
	FDroneMovementTestContext Context;
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

void DestroyDroneMovementTestContext(FDroneMovementTestContext& Context)
{
	if (Context.World)
	{
		Context.World->DestroyWorld(false);
	}
	Context = FDroneMovementTestContext();
}

bool SetFloatPropertyForMovementTest(UObject* Object, FName PropertyName, float Value)
{
	if (!Object)
	{
		return false;
	}

	FFloatProperty* FloatProperty = FindFProperty<FFloatProperty>(Object->GetClass(), PropertyName);
	if (!FloatProperty)
	{
		return false;
	}

	FloatProperty->SetPropertyValue_InContainer(Object, Value);
	return true;
}

bool PrepareInBattleMovementTest(FAutomationTestBase& Test, FDroneMovementTestContext& Context, const TCHAR* Label)
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

float MoveDroneForMovementTest(ADrone* Drone, FVector2D Axis, float DeltaSeconds)
{
	if (!Drone)
	{
		return 0.0f;
	}

	const FVector Before = Drone->GetActorLocation();
	Drone->ApplyMoveInputForServerForTest(Axis);
	Drone->ApplyPendingServerMoveInputForTest(DeltaSeconds);
	return FVector::Dist2D(Before, Drone->GetActorLocation());
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneBasicMovementSpeedAndAxisTest,
	"DroneProto.D16.Drone.BasicMovementSpeedAndAxis",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneBasicMovementSpeedAndAxisTest::RunTest(const FString& Parameters)
{
	FDroneMovementTestContext Context = CreateDroneMovementTestContext(TEXT("DroneBasicMovementSpeedWorld"));
	if (!PrepareInBattleMovementTest(*this, Context, TEXT("basic movement")))
	{
		DestroyDroneMovementTestContext(Context);
		return false;
	}

	TestTrue(TEXT("fixed Z test property exists"),
		SetFloatPropertyForMovementTest(Context.Drone, TEXT("FixedZPosition"), 123.0f));
	TestTrue(TEXT("base move speed is 4.5 meters per second"),
		FMath::IsNearlyEqual(Context.Drone->GetCurrentMoveSpeed(), 4.5f, 0.001f));

	Context.Drone->SetActorLocation(FVector(1000.0f, 1000.0f, 999.0f));
	Context.Drone->ResetAccumulatedMoveDistanceForServer();
	const float StraightDistanceCm = MoveDroneForMovementTest(Context.Drone, FVector2D(1.0f, 0.0f), 1.0f);
	TestTrue(TEXT("straight input moves at 4.5m/s"),
		FMath::IsNearlyEqual(StraightDistanceCm, 450.0f, 0.1f));
	TestTrue(TEXT("server movement locks Z to FixedZPosition"),
		FMath::IsNearlyEqual(Context.Drone->GetActorLocation().Z, 123.0f, 0.1f));

	Context.Drone->SetActorLocation(FVector(1000.0f, 1000.0f, 999.0f));
	Context.Drone->ResetAccumulatedMoveDistanceForServer();
	const float DiagonalDistanceCm = MoveDroneForMovementTest(Context.Drone, FVector2D(1.0f, 1.0f), 1.0f);
	TestTrue(TEXT("diagonal input is normalized to the same 4.5m/s speed"),
		FMath::IsNearlyEqual(DiagonalDistanceCm, StraightDistanceCm, 0.1f));

	Context.Drone->ApplyMoveInputForServerForTest(FVector2D(2.0f, 0.0f));
	TestTrue(TEXT("raw input longer than one is normalized on the server"),
		Context.Drone->GetLastServerMoveInputForTest().Equals(FVector2D(1.0f, 0.0f), 0.001f));

	TestFalse(TEXT("zero input is ignored as immediate stop"),
		Context.Drone->ApplyMoveInputForServerForTest(FVector2D::ZeroVector));
	TestTrue(TEXT("zero input leaves no pending server movement"),
		Context.Drone->GetLastServerMoveInputForTest().IsNearlyZero());

	DestroyDroneMovementTestContext(Context);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneMovementBoundaryAndBossClampTest,
	"DroneProto.D16.Drone.BoundaryAndBossClamp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneMovementBoundaryAndBossClampTest::RunTest(const FString& Parameters)
{
	FDroneMovementTestContext Context = CreateDroneMovementTestContext(TEXT("DroneMovementBoundaryWorld"));
	if (!PrepareInBattleMovementTest(*this, Context, TEXT("movement boundary")))
	{
		DestroyDroneMovementTestContext(Context);
		return false;
	}

	TestTrue(TEXT("fixed Z test property exists for boundary clamp"),
		SetFloatPropertyForMovementTest(Context.Drone, TEXT("FixedZPosition"), 0.0f));

	const FVector BoundaryClamped = Context.Drone->ClampPositionToMovementBoundaryForServer(FVector(6000.0f, 0.0f, 777.0f));
	TestTrue(TEXT("boundary helper clamps to the 50m circle"),
		FMath::IsNearlyEqual(FVector::Dist2D(BoundaryClamped, FVector::ZeroVector), 5000.0f, 0.1f));
	TestTrue(TEXT("boundary helper keeps Z locked"),
		FMath::IsNearlyEqual(BoundaryClamped.Z, 0.0f, 0.1f));

	Context.Drone->SetActorLocation(FVector(4990.0f, 0.0f, 777.0f));
	Context.Drone->ResetAccumulatedMoveDistanceForServer();
	MoveDroneForMovementTest(Context.Drone, FVector2D(1.0f, 0.0f), 1.0f);
	TestTrue(TEXT("server movement cannot leave the 50m circular boundary"),
		FVector::Dist2D(Context.Drone->GetActorLocation(), FVector::ZeroVector) <= 5000.0f + 0.1f);

	const FVector BossClamped = Context.Drone->ClampPositionOutsideBossCenterForServer(FVector(200.0f, 0.0f, 777.0f));
	TestTrue(TEXT("boss helper pushes targets out to 5m minimum distance"),
		FMath::IsNearlyEqual(FVector::Dist2D(BossClamped, FVector::ZeroVector), 500.0f, 0.1f));
	TestTrue(TEXT("boss helper keeps Z locked"),
		FMath::IsNearlyEqual(BossClamped.Z, 0.0f, 0.1f));

	Context.Drone->SetActorLocation(FVector(600.0f, 0.0f, 777.0f));
	Context.Drone->ResetAccumulatedMoveDistanceForServer();
	MoveDroneForMovementTest(Context.Drone, FVector2D(0.0f, -1.0f), 1.0f);
	TestTrue(TEXT("server movement cannot enter the boss 5m minimum distance"),
		FVector::Dist2D(Context.Drone->GetActorLocation(), FVector::ZeroVector) >= 500.0f - 0.1f);
	TestTrue(TEXT("server movement keeps Z locked after boss clamp"),
		FMath::IsNearlyEqual(Context.Drone->GetActorLocation().Z, 0.0f, 0.1f));

	DestroyDroneMovementTestContext(Context);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneMovementAllowedStateAndDistanceTest,
	"DroneProto.D16.Drone.AllowedStateAndDistance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneMovementAllowedStateAndDistanceTest::RunTest(const FString& Parameters)
{
	FDroneMovementTestContext SelectingContext = CreateDroneMovementTestContext(TEXT("DroneMovementSelectingWorld"));
	TestNotNull(TEXT("selecting movement drone is spawned"), SelectingContext.Drone);
	if (!SelectingContext.Drone)
	{
		DestroyDroneMovementTestContext(SelectingContext);
		return false;
	}
	FName IgnoreReason = NAME_None;
	TestFalse(TEXT("selection screen blocks movement"),
		SelectingContext.Drone->IsMovementAllowedForServer(IgnoreReason));
	TestEqual(TEXT("selection screen reports Selecting"),
		IgnoreReason,
		FName(TEXT("Selecting")));
	TestFalse(TEXT("selection screen move input is ignored"),
		SelectingContext.Drone->ApplyMoveInputForServerForTest(FVector2D(1.0f, 0.0f)));
	DestroyDroneMovementTestContext(SelectingContext);

	FDroneMovementTestContext LoadingContext = CreateDroneMovementTestContext(TEXT("DroneMovementLoadingWorld"), false);
	TestNotNull(TEXT("server loading movement drone is spawned"), LoadingContext.Drone);
	TestNotNull(TEXT("server loading controller is spawned"), LoadingContext.PC);
	TestNotNull(TEXT("server loading game state is spawned"), LoadingContext.GameState);
	if (!LoadingContext.Drone || !LoadingContext.PC || !LoadingContext.GameState)
	{
		DestroyDroneMovementTestContext(LoadingContext);
		return false;
	}
	LoadingContext.PC->Server_RequestReadyForRaid_Implementation();
	TestEqual(TEXT("server loading setup enters battle first"),
		LoadingContext.PC->GetCurrentSelectionState(),
		EPlayerSelectionState::InBattle);
	LoadingContext.GameState->SetRaidStateForServer(ERaidState::Waiting);
	IgnoreReason = NAME_None;
	TestFalse(TEXT("server loading raid state blocks movement"),
		LoadingContext.Drone->IsMovementAllowedForServer(IgnoreReason));
	TestEqual(TEXT("server loading reports ServerLoading"),
		IgnoreReason,
		FName(TEXT("ServerLoading")));
	DestroyDroneMovementTestContext(LoadingContext);

	FDroneMovementTestContext DeadContext = CreateDroneMovementTestContext(TEXT("DroneMovementDeadWorld"));
	if (!PrepareInBattleMovementTest(*this, DeadContext, TEXT("dead movement")))
	{
		DestroyDroneMovementTestContext(DeadContext);
		return false;
	}
	DeadContext.Drone->ApplyDamageForServer(DeadContext.Drone->GetMaxHealth() + 100, FName(TEXT("Automation")));
	IgnoreReason = NAME_None;
	TestFalse(TEXT("dead drones block movement"),
		DeadContext.Drone->IsMovementAllowedForServer(IgnoreReason));
	TestEqual(TEXT("dead movement reports Dead"),
		IgnoreReason,
		FName(TEXT("Dead")));
	DestroyDroneMovementTestContext(DeadContext);

	FDroneMovementTestContext ReportContext = CreateDroneMovementTestContext(TEXT("DroneMovementReportWorld"));
	if (!PrepareInBattleMovementTest(*this, ReportContext, TEXT("report movement")))
	{
		DestroyDroneMovementTestContext(ReportContext);
		return false;
	}
	TestTrue(TEXT("report generation succeeds for report movement gate"),
		ReportContext.PC->TryCreateDroneReportForServer(EDroneReportTrigger::RaidTimeLimit, false));
	IgnoreReason = NAME_None;
	TestFalse(TEXT("report state blocks movement"),
		ReportContext.Drone->IsMovementAllowedForServer(IgnoreReason));
	TestEqual(TEXT("report state reports Report"),
		IgnoreReason,
		FName(TEXT("Report")));
	DestroyDroneMovementTestContext(ReportContext);

	FDroneMovementTestContext DodgingContext = CreateDroneMovementTestContext(TEXT("DroneMovementDodgingWorld"));
	if (!PrepareInBattleMovementTest(*this, DodgingContext, TEXT("dodging movement")))
	{
		DestroyDroneMovementTestContext(DodgingContext);
		return false;
	}
	TestTrue(TEXT("existing server dodge starts the dodge/cooldown window"),
		DodgingContext.Drone->RequestDodgeForServer(FVector2D(1.0f, 0.0f)));
	IgnoreReason = NAME_None;
	TestFalse(TEXT("dodging state blocks normal movement"),
		DodgingContext.Drone->IsMovementAllowedForServer(IgnoreReason));
	TestEqual(TEXT("dodging state reports Dodging"),
		IgnoreReason,
		FName(TEXT("Dodging")));
	TestFalse(TEXT("dodging state move input is ignored"),
		DodgingContext.Drone->ApplyMoveInputForServerForTest(FVector2D(1.0f, 0.0f)));
	DestroyDroneMovementTestContext(DodgingContext);

	FDroneMovementTestContext DistanceContext = CreateDroneMovementTestContext(TEXT("DroneMovementDistance2DWorld"));
	if (!PrepareInBattleMovementTest(*this, DistanceContext, TEXT("distance movement")))
	{
		DestroyDroneMovementTestContext(DistanceContext);
		return false;
	}
	DistanceContext.Drone->ResetAccumulatedMoveDistanceForServer();
	DistanceContext.Drone->SetActorLocation(FVector(0.0f, 0.0f, 200.0f));
	DistanceContext.Drone->UpdateMoveDistanceForServerForTest(0.10f);
	DistanceContext.Drone->SetActorLocation(FVector(0.0f, 0.0f, 400.0f));
	DistanceContext.Drone->UpdateMoveDistanceForServerForTest(0.10f);
	TestTrue(TEXT("Z-only motion is excluded from movement distance"),
		FMath::IsNearlyZero(DistanceContext.Drone->GetAccumulatedMoveDistance(), 0.001f));

	DistanceContext.Drone->SetActorLocation(FVector(30.0f, 40.0f, 999.0f));
	DistanceContext.Drone->UpdateMoveDistanceForServerForTest(0.10f);
	TestTrue(TEXT("movement distance accumulates actual XY distance only"),
		FMath::IsNearlyEqual(DistanceContext.Drone->GetAccumulatedMoveDistance(), 0.5f, 0.001f));

	DistanceContext.Drone->ResetAccumulatedMoveDistanceForServer();
	DistanceContext.Drone->SetActorLocation(FVector(3000.0f, 0.0f, 0.0f));
	DistanceContext.Drone->UpdateMoveDistanceForServerForTest(0.10f);
	TestTrue(TEXT("reset/ready first sample prevents distance burst"),
		FMath::IsNearlyZero(DistanceContext.Drone->GetAccumulatedMoveDistance(), 0.001f));

	DestroyDroneMovementTestContext(DistanceContext);
	return true;
}

#endif
