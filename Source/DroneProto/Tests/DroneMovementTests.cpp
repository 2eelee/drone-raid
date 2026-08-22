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

	Context.PC->SetControlRotation(FRotator(0.0f, 90.0f, 0.0f));
	Context.Drone->SetActorLocation(FVector(1000.0f, 1000.0f, 123.0f));
	Context.Drone->ResetAccumulatedMoveDistanceForServer();
	const FVector WorldVectorStart = Context.Drone->GetActorLocation();
	MoveDroneForMovementTest(Context.Drone, FVector2D(1.0f, 0.0f), 1.0f);
	const FVector WorldVectorDelta = Context.Drone->GetActorLocation() - WorldVectorStart;
	TestTrue(TEXT("server treats move input as a requested world X/Y vector, independent of control yaw"),
		FMath::IsNearlyEqual(WorldVectorDelta.X, 450.0f, 0.1f)
		&& FMath::IsNearlyZero(WorldVectorDelta.Y, 0.1f));
	Context.PC->SetControlRotation(FRotator::ZeroRotator);

	Context.Drone->ApplyMoveInputForServerForTest(FVector2D(2.0f, 0.0f));
	TestTrue(TEXT("raw input longer than one is normalized on the server"),
		Context.Drone->GetLastServerMoveInputForTest().Equals(FVector2D(1.0f, 0.0f), 0.001f));

	TestTrue(TEXT("move release test starts with cached dodge direction"),
		Context.Drone->CacheMoveInputForDodgeForTest(FVector2D(1.0f, 0.0f)));
	Context.Drone->ClearMoveInputForDodgeForTest();
	TestTrue(TEXT("move release clears pending server movement"),
		Context.Drone->GetLastServerMoveInputForTest().IsNearlyZero());
	TestTrue(TEXT("move release clears cached dodge direction"),
		Context.Drone->GetCachedMoveInputForDodgeForTest().IsNearlyZero());

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
	TestTrue(TEXT("boss helper pushes targets out to 8m minimum distance"),
		FMath::IsNearlyEqual(FVector::Dist2D(BossClamped, FVector::ZeroVector), 800.0f, 0.1f));
	TestTrue(TEXT("boss helper keeps Z locked"),
		FMath::IsNearlyEqual(BossClamped.Z, 0.0f, 0.1f));

	Context.Drone->SetActorLocation(FVector(900.0f, 0.0f, 777.0f));
	Context.Drone->ResetAccumulatedMoveDistanceForServer();
	MoveDroneForMovementTest(Context.Drone, FVector2D(-1.0f, 0.0f), 1.0f);
	TestTrue(TEXT("server movement cannot enter the boss 8m minimum distance"),
		FVector::Dist2D(Context.Drone->GetActorLocation(), FVector::ZeroVector) >= 800.0f - 0.1f);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneMoveAcceptedLogThrottleTest,
	"DroneProto.D16.Drone.MoveAcceptedLogThrottle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneMoveAcceptedLogThrottleTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("move accepted summary interval is long enough for manual PIE readability"),
		ADrone::GetMoveAcceptedSummaryLogIntervalSecondsForTest() >= 3.0f);
	TestTrue(TEXT("move distance summary interval is long enough for manual PIE readability"),
		ADrone::GetMoveDistanceSummaryLogIntervalSecondsForTest() >= 3.0f);
	TestTrue(TEXT("first accepted move emits a summary"),
		ADrone::ShouldEmitMoveAcceptedSummaryLogForTest(
			0.0f,
			-1000.0f,
			false,
			false,
			FVector2D::ZeroVector,
			FVector2D(1.0f, 0.0f)));
	TestFalse(TEXT("same accepted axis inside interval is throttled"),
		ADrone::ShouldEmitMoveAcceptedSummaryLogForTest(
			1.0f,
			0.0f,
			true,
			true,
			FVector2D(1.0f, 0.0f),
			FVector2D(1.0f, 0.0f)));
	TestTrue(TEXT("accepted axis change emits a summary inside interval"),
		ADrone::ShouldEmitMoveAcceptedSummaryLogForTest(
			1.0f,
			0.0f,
			true,
			true,
			FVector2D(1.0f, 0.0f),
			FVector2D(0.0f, 1.0f)));
	TestTrue(TEXT("same accepted axis emits again after interval"),
		ADrone::ShouldEmitMoveAcceptedSummaryLogForTest(
			3.1f,
			0.0f,
			true,
			true,
			FVector2D(1.0f, 0.0f),
			FVector2D(1.0f, 0.0f)));
	TestFalse(TEXT("same accepted move axis does not force a log"),
		ADrone::IsMoveAcceptedSummaryAxisChangeSignificant(FVector2D(1.0f, 0.0f), FVector2D(1.0f, 0.0f)));
	TestFalse(TEXT("tiny camera-relative axis drift does not force a log"),
		ADrone::IsMoveAcceptedSummaryAxisChangeSignificant(FVector2D(1.0f, 0.0f), FVector2D(0.9998f, 0.0175f)));
	TestTrue(TEXT("meaningful direction change forces a log"),
		ADrone::IsMoveAcceptedSummaryAxisChangeSignificant(FVector2D(1.0f, 0.0f), FVector2D(0.9848f, 0.1736f)));
	TestTrue(TEXT("movement start forces a log"),
		ADrone::IsMoveAcceptedSummaryAxisChangeSignificant(FVector2D::ZeroVector, FVector2D(1.0f, 0.0f)));
	TestTrue(TEXT("movement stop is a significant state transition"),
		ADrone::IsMoveAcceptedSummaryAxisChangeSignificant(FVector2D(1.0f, 0.0f), FVector2D::ZeroVector));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneBossFacingRotationTest,
	"DroneProto.D16.Drone.BossFacingRotation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneBossFacingRotationTest::RunTest(const FString& Parameters)
{
	// 기획자 2026-08-22 확정: 드론은 전투 중 항상 보스를 바라본다.
	// 이동 방향으로는 회전하지 않으며, 후방 이동 시에도 정면은 보스를 향한 채 뒤로 간다.
	// 회피도 같은 방향을 유지하고, 순간이동 구간에 별도 회전 연출을 넣지 않는다.

	// 1) 순수 계산. 수평 투영 Yaw이므로 보스 높이는 결과를 바꾸지 않는다.
	const FVector Origin = FVector::ZeroVector;
	struct FBossFacingCase
	{
		FVector BossLocation;
		float ExpectedYaw;
		const TCHAR* Label;
	};
	const FBossFacingCase Cases[] = {
		{FVector(1000.0f, 0.0f, 0.0f), 0.0f, TEXT("+X")},
		{FVector(0.0f, 1000.0f, 0.0f), 90.0f, TEXT("+Y")},
		{FVector(-1000.0f, 0.0f, 0.0f), 180.0f, TEXT("-X")},
		{FVector(0.0f, -1000.0f, 0.0f), -90.0f, TEXT("-Y")},
		{FVector(1000.0f, 0.0f, 5000.0f), 0.0f, TEXT("+X with a tall boss")},
	};
	for (const FBossFacingCase& Case : Cases)
	{
		float Yaw = 0.0f;
		TestTrue(*FString::Printf(TEXT("boss at %s resolves a facing yaw"), Case.Label),
			ADrone::CalculateBossFacingDroneYaw(Origin, &Case.BossLocation, Yaw));
		TestTrue(*FString::Printf(TEXT("boss at %s faces %.0f degrees"), Case.Label, Case.ExpectedYaw),
			FMath::IsNearlyZero(FRotator::NormalizeAxis(Yaw - Case.ExpectedYaw), 0.01f));
	}

	float UnusedYaw = 0.0f;
	TestFalse(TEXT("no boss resolves no facing yaw"),
		ADrone::CalculateBossFacingDroneYaw(Origin, nullptr, UnusedYaw));
	const FVector Overlapping(0.0f, 0.0f, 3000.0f);
	TestFalse(TEXT("a boss directly overhead resolves no facing yaw"),
		ADrone::CalculateBossFacingDroneYaw(Origin, &Overlapping, UnusedYaw));

	// 2) 카메라와 같은 Yaw여야 한다. 두 값이 어긋나면 카메라-드론-보스가 일직선에서 벗어나
	// 화면 안에서 드론이 조금씩 틀어져 보인다 — 이 계약의 존재 이유다.
	const FVector DroneLocation(-2500.0f, 1200.0f, 0.0f);
	const FVector BossLocation(0.0f, 0.0f, 800.0f);
	float DroneYaw = 0.0f;
	TestTrue(TEXT("facing yaw resolves for the camera comparison"),
		ADrone::CalculateBossFacingDroneYaw(DroneLocation, &BossLocation, DroneYaw));

	FDroneCombatCameraView CameraView;
	ADrone::CalculateFixedBossFacingQuarterView(
		DroneLocation, &BossLocation, FVector::ForwardVector, 1400.0f, 380.0f, -10.0f, CameraView);
	TestTrue(TEXT("drone facing yaw matches the combat camera yaw"),
		FMath::IsNearlyZero(FRotator::NormalizeAxis(CameraView.CameraRotation.Yaw - DroneYaw), 0.01f));

	// 3) 전투 중 실제 적용. 보스는 원점에 있다.
	FDroneMovementTestContext Context = CreateDroneMovementTestContext(TEXT("DroneBossFacingRotationWorld"));
	if (!PrepareInBattleMovementTest(*this, Context, TEXT("boss facing rotation")))
	{
		DestroyDroneMovementTestContext(Context);
		return false;
	}
	TestNotNull(TEXT("boss facing rotation boss is spawned"), Context.Boss);
	if (!Context.Boss)
	{
		DestroyDroneMovementTestContext(Context);
		return false;
	}

	auto ExpectFacingBoss = [this, &Context](const TCHAR* Label)
	{
		const FVector ToBoss = Context.Boss->GetActorLocation() - Context.Drone->GetActorLocation();
		const float ExpectedYaw = FVector(ToBoss.X, ToBoss.Y, 0.0f).GetSafeNormal().Rotation().Yaw;
		const FRotator Actual = Context.Drone->GetActorRotation();
		TestTrue(*FString::Printf(TEXT("%s faces the boss"), Label),
			FMath::IsNearlyZero(FRotator::NormalizeAxis(Actual.Yaw - ExpectedYaw), 0.01f));
		// XY 평면 이동이므로 기울지 않는다(MOVE-01·MOVE-04).
		TestTrue(*FString::Printf(TEXT("%s stays level"), Label),
			FMath::IsNearlyZero(Actual.Pitch, 0.01f) && FMath::IsNearlyZero(Actual.Roll, 0.01f));
	};

	// 스폰 회전이 엉뚱해도 첫 갱신에서 보스를 향한다.
	Context.Drone->SetActorLocation(FVector(-3000.0f, 0.0f, 0.0f));
	Context.Drone->SetActorRotation(FRotator(0.0f, 137.0f, 0.0f));
	Context.Drone->UpdateBossFacingRotationForServer();
	ExpectFacingBoss(TEXT("initial update"));

	// 4) 이동 방향과 무관하다. 후방 이동(보스에서 멀어짐)에도 정면은 보스를 향한다.
	const FVector2D MoveAxes[] = {
		FVector2D(-1.0f, 0.0f),
		FVector2D(0.0f, 1.0f),
		FVector2D(1.0f, 1.0f),
		FVector2D(0.0f, -1.0f),
	};
	for (const FVector2D& Axis : MoveAxes)
	{
		MoveDroneForMovementTest(Context.Drone, Axis, 0.2f);
		Context.Drone->UpdateBossFacingRotationForServer();
		ExpectFacingBoss(*FString::Printf(TEXT("after moving %s"), *Axis.ToString()));
	}

	// 5) 회피 중에도 보스를 향한다 — 회전 게이트가 이동 게이트와 다른 유일한 지점이다.
	TestTrue(TEXT("dodge input is cached"),
		Context.Drone->CacheMoveInputForDodgeForTest(FVector2D(-1.0f, 0.0f)));
	TestTrue(TEXT("dodge starts for the rotation gate"),
		Context.Drone->RequestDodgeFromCurrentMoveInputForTest());
	TestTrue(TEXT("drone is dodging"), Context.Drone->IsDodging());

	FName MoveReason = NAME_None;
	TestFalse(TEXT("dodging blocks movement"), Context.Drone->IsMovementAllowedForServer(MoveReason));
	TestEqual(TEXT("dodging movement reason is Dodging"), MoveReason, FName(TEXT("Dodging")));

	FName RotationReason = NAME_None;
	TestTrue(TEXT("dodging still allows boss facing rotation"),
		Context.Drone->IsBossFacingRotationAllowedForServer(RotationReason));
	TestEqual(TEXT("allowed rotation reports no block reason"), RotationReason, NAME_None);

	// 순간이동으로 각도가 크게 바뀌어도 정면은 따라온다.
	Context.Drone->SetActorLocation(FVector(2200.0f, -2600.0f, 0.0f));
	Context.Drone->UpdateBossFacingRotationForServer();
	ExpectFacingBoss(TEXT("mid-dodge blink"));

	DestroyDroneMovementTestContext(Context);

	// 6) 전투가 아니면 돌지 않는다. 선택 화면에서는 스폰 회전이 그대로 유지된다.
	FDroneMovementTestContext SelectingContext =
		CreateDroneMovementTestContext(TEXT("DroneBossFacingSelectingWorld"));
	TestNotNull(TEXT("selecting rotation drone is spawned"), SelectingContext.Drone);
	if (!SelectingContext.Drone)
	{
		DestroyDroneMovementTestContext(SelectingContext);
		return false;
	}
	SelectingContext.Drone->SetActorLocation(FVector(-3000.0f, 0.0f, 0.0f));
	SelectingContext.Drone->SetActorRotation(FRotator(0.0f, 137.0f, 0.0f));
	FName SelectingReason = NAME_None;
	TestFalse(TEXT("selection screen blocks boss facing rotation"),
		SelectingContext.Drone->IsBossFacingRotationAllowedForServer(SelectingReason));
	TestEqual(TEXT("selection screen rotation reason is Selecting"),
		SelectingReason, FName(TEXT("Selecting")));
	SelectingContext.Drone->UpdateBossFacingRotationForServer();
	TestTrue(TEXT("selection screen keeps the spawn rotation"),
		FMath::IsNearlyZero(
			FRotator::NormalizeAxis(SelectingContext.Drone->GetActorRotation().Yaw - 137.0f), 0.01f));
	DestroyDroneMovementTestContext(SelectingContext);

	// 7) 사망하면 멈춘다. 시신이 보스를 따라 도는 일은 없어야 한다.
	FDroneMovementTestContext DeadContext = CreateDroneMovementTestContext(TEXT("DroneBossFacingDeadWorld"));
	if (!PrepareInBattleMovementTest(*this, DeadContext, TEXT("dead boss facing")))
	{
		DestroyDroneMovementTestContext(DeadContext);
		return false;
	}
	DeadContext.Drone->SetActorLocation(FVector(-3000.0f, 0.0f, 0.0f));
	DeadContext.Drone->UpdateBossFacingRotationForServer();
	DeadContext.Drone->ApplyDamageForServer(DeadContext.Drone->GetMaxHealth() + 100, FName(TEXT("Automation")));

	const FRotator RotationAtDeath = DeadContext.Drone->GetActorRotation();
	FName DeadReason = NAME_None;
	TestFalse(TEXT("dead drones block boss facing rotation"),
		DeadContext.Drone->IsBossFacingRotationAllowedForServer(DeadReason));
	TestEqual(TEXT("dead rotation reason is Dead"), DeadReason, FName(TEXT("Dead")));

	DeadContext.Drone->SetActorLocation(FVector(0.0f, 4000.0f, 0.0f));
	DeadContext.Drone->UpdateBossFacingRotationForServer();
	TestTrue(TEXT("dead drones keep their last rotation"),
		FMath::IsNearlyZero(
			FRotator::NormalizeAxis(DeadContext.Drone->GetActorRotation().Yaw - RotationAtDeath.Yaw), 0.01f));
	DestroyDroneMovementTestContext(DeadContext);

	return true;
}

#endif
