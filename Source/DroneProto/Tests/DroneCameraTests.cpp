#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Drone.h"
#include "Raid/RaidBoss.h"
#include "Raid/RaidGameState.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"

namespace
{
struct FDroneCameraTestWorld
{
	UWorld* World = nullptr;
	ARaidGameState* GameState = nullptr;
};

FDroneCameraTestWorld CreateDroneCameraTestWorld(const TCHAR* WorldName)
{
	FDroneCameraTestWorld Context;
	Context.World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
	if (!Context.World)
	{
		return Context;
	}

	Context.GameState = Context.World->SpawnActor<ARaidGameState>();
	if (Context.GameState)
	{
		Context.World->SetGameState(Context.GameState);
	}
	return Context;
}

void DestroyDroneCameraTestWorld(FDroneCameraTestWorld& Context)
{
	if (Context.World)
	{
		Context.World->DestroyWorld(false);
	}
	Context = FDroneCameraTestWorld();
}

void SetBossPrimitiveComponentsVisible(ARaidBoss* Boss, bool bVisible)
{
	if (!Boss)
	{
		return;
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	Boss->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (PrimitiveComponent)
		{
			PrimitiveComponent->SetVisibility(bVisible, true);
		}
	}
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRaidBossPrototypeVisualReadyTest,
	"DroneProto.D18.Drone.RaidBossPrototypeVisualReady",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRaidBossPrototypeVisualReadyTest::RunTest(const FString& Parameters)
{
	FDroneCameraTestWorld Context = CreateDroneCameraTestWorld(TEXT("RaidBossPrototypeVisualReadyWorld"));
	TestNotNull(TEXT("visual ready world is created"), Context.World);
	if (!Context.World)
	{
		return false;
	}

	ARaidBoss* Boss = Context.World->SpawnActor<ARaidBoss>();
	TestNotNull(TEXT("default C++ raid boss is spawned"), Boss);
	if (!Boss)
	{
		DestroyDroneCameraTestWorld(Context);
		return false;
	}

	TestTrue(TEXT("default C++ raid boss exposes a camera-visible prototype visual"),
		Boss->IsVisualReadyForCamera());

	FName InvalidReason;
	TestTrue(TEXT("default C++ raid boss is a valid boss-facing camera target"),
		ADrone::IsValidFixedBossFacingCameraTarget(Boss, InvalidReason));
	TestEqual(TEXT("visual-ready boss has no invalid reason"),
		InvalidReason,
		FName());

	DestroyDroneCameraTestWorld(Context);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneFixedBossFacingCameraMathTest,
	"DroneProto.D18.Drone.FixedBossFacingQuarterViewCameraMath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneFixedBossFacingCameraMathTest::RunTest(const FString& Parameters)
{
	FDroneCombatCameraView CameraView;
	const FVector PlayerPosition(0.0f, 0.0f, 100.0f);
	const FVector BossPosition(1000.0f, 0.0f, 100.0f);

	TestTrue(TEXT("boss-facing camera calculation reports a valid boss"),
		ADrone::CalculateFixedBossFacingQuarterView(
			PlayerPosition,
			&BossPosition,
			FVector::ForwardVector,
			1400.0f,
			380.0f,
			-10.0f,
			CameraView));
	TestTrue(TEXT("camera is placed 14m behind the player on the boss axis"),
		FMath::IsNearlyEqual(FVector::Dist2D(PlayerPosition, CameraView.CameraLocation), 1400.0f, 0.01f));
	TestTrue(TEXT("camera height is 3.8m above the player"),
		FMath::IsNearlyEqual(CameraView.CameraLocation.Z - PlayerPosition.Z, 380.0f, 0.01f));
	TestTrue(TEXT("camera looks at the boss when a boss target exists"),
		CameraView.CameraRotation.Vector().Equals((BossPosition - CameraView.CameraLocation).GetSafeNormal(), 0.001f));
	TestTrue(TEXT("camera metadata records boss validity"),
		CameraView.bBossValid);

	FDroneCombatCameraView FallbackView;
	TestFalse(TEXT("camera calculation falls back safely when boss is missing"),
		ADrone::CalculateFixedBossFacingQuarterView(
			PlayerPosition,
			nullptr,
			FVector::RightVector,
			1400.0f,
			380.0f,
			-10.0f,
			FallbackView));
	TestTrue(TEXT("fallback camera still applies configured distance"),
		FMath::IsNearlyEqual(FVector::Dist2D(PlayerPosition, FallbackView.CameraLocation), 1400.0f, 0.01f));
	TestTrue(TEXT("fallback camera still applies configured height"),
		FMath::IsNearlyEqual(FallbackView.CameraLocation.Z - PlayerPosition.Z, 380.0f, 0.01f));
	TestTrue(TEXT("fallback camera uses configured fallback pitch"),
		FMath::IsNearlyEqual(FallbackView.CameraRotation.Pitch, -10.0f, 0.01f));
	TestFalse(TEXT("fallback camera metadata records missing boss"),
		FallbackView.bBossValid);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneFixedBossFacingCameraLocalGateTest,
	"DroneProto.D18.Drone.FixedBossFacingQuarterViewCameraLocalGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneFixedBossFacingCameraLocalGateTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("local player controller can apply the combat camera"),
		ADrone::ShouldApplyFixedBossFacingCamera(true, true, true));
	TestFalse(TEXT("non-local pawn does not apply the combat camera"),
		ADrone::ShouldApplyFixedBossFacingCamera(false, true, true));
	TestFalse(TEXT("non-player controller does not apply the combat camera"),
		ADrone::ShouldApplyFixedBossFacingCamera(true, false, false));
	TestFalse(TEXT("non-local player controller does not apply the combat camera"),
		ADrone::ShouldApplyFixedBossFacingCamera(true, true, false));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneFixedBossFacingCameraTargetValidationTest,
	"DroneProto.D18.Drone.FixedBossFacingQuarterViewCameraTargetValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneFixedBossFacingCameraTargetValidationTest::RunTest(const FString& Parameters)
{
	FDroneCameraTestWorld Context = CreateDroneCameraTestWorld(TEXT("DroneCameraTargetValidationWorld"));
	TestNotNull(TEXT("camera target validation world is created"), Context.World);
	TestNotNull(TEXT("camera target validation game state is spawned"), Context.GameState);
	if (!Context.World || !Context.GameState)
	{
		DestroyDroneCameraTestWorld(Context);
		return false;
	}

	ARaidBoss* OfficialBoss = Context.World->SpawnActor<ARaidBoss>();
	ARaidBoss* FallbackBoss = Context.World->SpawnActor<ARaidBoss>();
	TestNotNull(TEXT("official boss is spawned"), OfficialBoss);
	TestNotNull(TEXT("fallback boss is spawned"), FallbackBoss);
	if (!OfficialBoss || !FallbackBoss)
	{
		DestroyDroneCameraTestWorld(Context);
		return false;
	}

	OfficialBoss->SetActorLocation(FVector(1000.0f, 0.0f, 0.0f));
	FallbackBoss->SetActorLocation(FVector(0.0f, 1000.0f, 0.0f));
	Context.GameState->SetRaidBossForServer(OfficialBoss);

	FDroneCombatCameraTarget Target;
	TestTrue(TEXT("visible GameState boss is valid for camera"),
		ADrone::ResolveFixedBossFacingCameraTargetForWorld(Context.World, Target));
	TestEqual(TEXT("GameState boss has priority over fallback search"),
		Target.Boss.Get(),
		OfficialBoss);
	TestEqual(TEXT("target source records GameState"),
		Target.Source,
		FName(TEXT("GameState")));
	TestEqual(TEXT("valid target has no invalid reason"),
		Target.InvalidReason,
		FName());

	OfficialBoss->SetActorHiddenInGame(true);
	TestFalse(TEXT("hidden GameState boss is not a valid camera target"),
		ADrone::ResolveFixedBossFacingCameraTargetForWorld(Context.World, Target));
	TestEqual(TEXT("hidden official target keeps GameState source for diagnostics"),
		Target.Source,
		FName(TEXT("GameState")));
	TestEqual(TEXT("hidden official target reports HiddenBoss"),
		Target.InvalidReason,
		FName(TEXT("HiddenBoss")));
	TestEqual(TEXT("hidden official target does not fall through to fallback boss"),
		Target.Boss.Get(),
		OfficialBoss);

	OfficialBoss->SetActorHiddenInGame(false);
	SetBossPrimitiveComponentsVisible(OfficialBoss, false);
	TestFalse(TEXT("visible official boss without camera-visible visual is not a valid camera target"),
		ADrone::ResolveFixedBossFacingCameraTargetForWorld(Context.World, Target));
	TestEqual(TEXT("official target without visual keeps GameState source for diagnostics"),
		Target.Source,
		FName(TEXT("GameState")));
	TestEqual(TEXT("official target without visual reports NoVisibleMesh"),
		Target.InvalidReason,
		FName(TEXT("NoVisibleMesh")));
	TestEqual(TEXT("official target without visual does not fall through to fallback boss"),
		Target.Boss.Get(),
		OfficialBoss);

	Context.GameState->SetRaidBossForServer(nullptr);
	TestTrue(TEXT("fallback search is used only when official boss is missing"),
		ADrone::ResolveFixedBossFacingCameraTargetForWorld(Context.World, Target));
	TestEqual(TEXT("fallback search records fallback source"),
		Target.Source,
		FName(TEXT("FallbackSearch")));
	TestEqual(TEXT("fallback search can select the visible fallback boss"),
		Target.Boss.Get(),
		FallbackBoss);

	DestroyDroneCameraTestWorld(Context);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneFixedBossFacingCameraFallbackYawAndThrottleTest,
	"DroneProto.D18.Drone.FixedBossFacingQuarterViewCameraFallbackYawAndThrottle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneFixedBossFacingCameraFallbackYawAndThrottleTest::RunTest(const FString& Parameters)
{
	bool bHasCachedYaw = false;
	float CachedYaw = 0.0f;
	const float FirstYaw = ADrone::ResolveFixedBossFacingFallbackYaw(
		bHasCachedYaw,
		CachedYaw,
		45.0f,
		0.0f);
	TestTrue(TEXT("fallback yaw initializes from first local camera yaw"),
		FMath::IsNearlyEqual(FirstYaw, 45.0f, 0.001f));
	TestTrue(TEXT("fallback yaw cache is marked initialized"),
		bHasCachedYaw);

	const float SecondYaw = ADrone::ResolveFixedBossFacingFallbackYaw(
		bHasCachedYaw,
		CachedYaw,
		135.0f,
		0.0f);
	TestTrue(TEXT("fallback yaw stays fixed after first application"),
		FMath::IsNearlyEqual(SecondYaw, 45.0f, 0.001f));

	FDroneCombatCameraView FirstView;
	FDroneCombatCameraView SecondView;
	const FVector PlayerA(0.0f, 0.0f, 0.0f);
	const FVector PlayerB(300.0f, -200.0f, 0.0f);
	TestFalse(TEXT("first fallback view has no boss"),
		ADrone::CalculateFixedBossFacingQuarterView(
			PlayerA,
			nullptr,
			ADrone::BuildFixedBossFacingFallbackForward(SecondYaw),
			1400.0f,
			380.0f,
			-10.0f,
			FirstView));
	TestFalse(TEXT("second fallback view has no boss"),
		ADrone::CalculateFixedBossFacingQuarterView(
			PlayerB,
			nullptr,
			ADrone::BuildFixedBossFacingFallbackForward(SecondYaw),
			1400.0f,
			380.0f,
			-10.0f,
			SecondView));
	TestTrue(TEXT("fallback camera yaw does not rotate just because the player moved"),
		FMath::IsNearlyEqual(FirstView.CameraRotation.Yaw, SecondView.CameraRotation.Yaw, 0.001f));

	TestTrue(TEXT("summary log emits on first/forced event"),
		ADrone::ShouldEmitThrottledSummaryLog(0.0f, -1000.0f, 5.0f, true));
	TestFalse(TEXT("summary log is throttled for repeated event"),
		ADrone::ShouldEmitThrottledSummaryLog(1.0f, 0.0f, 5.0f, false));
	TestTrue(TEXT("summary log emits again after throttle window"),
		ADrone::ShouldEmitThrottledSummaryLog(5.1f, 0.0f, 5.0f, false));

	return true;
}

#endif
