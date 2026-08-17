#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Drone.h"
#include "Raid/RaidBoss.h"
#include "Raid/RaidGameState.h"

#include "Components/PrimitiveComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "GameFramework/SpringArmComponent.h"

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

UTextRenderComponent* FindPrototypeVisualLabel(ARaidBoss* Boss)
{
	if (!Boss)
	{
		return nullptr;
	}

	TArray<UTextRenderComponent*> TextRenderComponents;
	Boss->GetComponents<UTextRenderComponent>(TextRenderComponents);
	for (UTextRenderComponent* TextRenderComponent : TextRenderComponents)
	{
		if (TextRenderComponent && TextRenderComponent->GetFName() == FName(TEXT("PrototypeVisualLabel")))
		{
			return TextRenderComponent;
		}
	}
	return nullptr;
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
	UTextRenderComponent* PrototypeVisualLabel = FindPrototypeVisualLabel(Boss);
	TestNotNull(TEXT("default C++ raid boss keeps the legacy text label component for compatibility"),
		PrototypeVisualLabel);
	TestFalse(TEXT("default C++ raid boss hides the legacy 3D text label"),
		PrototypeVisualLabel && PrototypeVisualLabel->IsVisible());
	TestTrue(TEXT("hidden legacy label does not make the boss camera visual invalid"),
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
	FDroneFixedBossFacingCameraWorldRotationTest,
	"DroneProto.D18.Drone.FixedBossFacingCameraKeepsWorldRotationUnderDroneYaw",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneFixedBossFacingCameraWorldRotationTest::RunTest(const FString& Parameters)
{
	FDroneCameraTestWorld Context = CreateDroneCameraTestWorld(TEXT("FixedBossFacingCameraWorldRotationWorld"));
	TestNotNull(TEXT("camera world rotation world is created"), Context.World);
	if (!Context.World)
	{
		return false;
	}

	ADrone* Drone = Context.World->SpawnActor<ADrone>();
	TestNotNull(TEXT("drone is spawned for the combat camera rotation contract"), Drone);
	if (!Drone)
	{
		DestroyDroneCameraTestWorld(Context);
		return false;
	}

	USpringArmComponent* CombatCameraSpringArm = Drone->GetComponentByClass<USpringArmComponent>();
	TestNotNull(TEXT("drone exposes the combat camera spring arm"), CombatCameraSpringArm);
	if (!CombatCameraSpringArm)
	{
		DestroyDroneCameraTestWorld(Context);
		return false;
	}

	// 전투 카메라는 보스를 향하는 월드 회전을 계산해 SpringArm에 직접 넘긴다.
	// 드론은 보스를 향해 회전하므로, 부모 회전이 이 월드 회전을 상쇄하면 카메라가 보스 반대편을 본다.
	// POP-04 미표시는 드론 Yaw=180 지점에서 정확히 그렇게 재현됐다.
	const FRotator RequestedWorldRotation(-5.5f, 180.0f, 0.0f);
	const TArray<float> DroneYawDegrees = { 0.0f, 90.0f, 180.0f, -90.0f };
	for (const float DroneYaw : DroneYawDegrees)
	{
		Drone->SetActorRotation(FRotator(0.0f, DroneYaw, 0.0f));
		CombatCameraSpringArm->SetWorldLocationAndRotation(
			FVector(4900.0f, 0.0f, 472.0f),
			RequestedWorldRotation);

		const FRotator TargetRotation = CombatCameraSpringArm->GetTargetRotation();
		const float YawErrorDegrees = FMath::Abs(
			FRotator::NormalizeAxis(TargetRotation.Yaw - RequestedWorldRotation.Yaw));
		const float PitchErrorDegrees = FMath::Abs(
			FRotator::NormalizeAxis(TargetRotation.Pitch - RequestedWorldRotation.Pitch));

		TestTrue(
			*FString::Printf(
				TEXT("spring arm keeps the requested world yaw while the drone yaw is %.0f (error %.2f)"),
				DroneYaw,
				YawErrorDegrees),
			YawErrorDegrees <= 0.01f);
		TestTrue(
			*FString::Printf(
				TEXT("spring arm keeps the requested world pitch while the drone yaw is %.0f (error %.2f)"),
				DroneYaw,
				PitchErrorDegrees),
			PitchErrorDegrees <= 0.01f);
	}

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
	// 2026-08-17 A안 전환 — Pitch 는 고정값이고 Yaw 만 보스를 향한다. LookAt 유도는 폐기했다.
	TestTrue(TEXT("camera yaw faces the boss while pitch stays fixed"),
		FMath::IsNearlyEqual(
			CameraView.CameraRotation.Yaw,
			(BossPosition - CameraView.CameraLocation).Rotation().Yaw,
			0.01f));
	TestTrue(TEXT("camera uses the configured fixed pitch even with a valid boss"),
		FMath::IsNearlyEqual(CameraView.CameraRotation.Pitch, -10.0f, 0.01f));
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
	TestTrue(TEXT("fallback camera uses the same configured pitch"),
		FMath::IsNearlyEqual(FallbackView.CameraRotation.Pitch, -10.0f, 0.01f));
	TestFalse(TEXT("fallback camera metadata records missing boss"),
		FallbackView.bBossValid);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneFixedBossFacingCameraFixedPitchTest,
	"DroneProto.D18.Drone.FixedBossFacingCameraUsesFixedPitchRegardlessOfBossHeight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneFixedBossFacingCameraFixedPitchTest::RunTest(const FString& Parameters)
{
	// 확정 명세(드론 이동·회피 기획서 :213, :307) — CameraPitchAngle 은 조건 없는 고정값 -10° 다.
	// 계산식 :951 의 LookAt 유도는 2026-08-17 사용자 결정으로 폐기했다.
	const FVector PlayerPosition(0.0f, 0.0f, 92.0f);
	const float PitchDegrees = -10.0f;

	// 보스가 드론보다 훨씬 높이 떠 있어도 카메라 Pitch 는 고정값을 유지해야 한다.
	const FVector HighBossPosition(3500.0f, 0.0f, 800.0f);
	FDroneCombatCameraView HighView;
	ADrone::CalculateFixedBossFacingQuarterView(
		PlayerPosition,
		&HighBossPosition,
		FVector::ForwardVector,
		1400.0f,
		380.0f,
		PitchDegrees,
		HighView);
	TestTrue(TEXT("camera keeps the configured pitch even when the boss is far above the drone"),
		FMath::IsNearlyEqual(HighView.CameraRotation.Pitch, PitchDegrees, 0.01f));

	// 보스가 드론과 같은 높이여도 동일한 고정값이어야 한다.
	const FVector LevelBossPosition(3500.0f, 0.0f, 92.0f);
	FDroneCombatCameraView LevelView;
	ADrone::CalculateFixedBossFacingQuarterView(
		PlayerPosition,
		&LevelBossPosition,
		FVector::ForwardVector,
		1400.0f,
		380.0f,
		PitchDegrees,
		LevelView);
	TestTrue(TEXT("camera keeps the configured pitch when the boss is level with the drone"),
		FMath::IsNearlyEqual(LevelView.CameraRotation.Pitch, PitchDegrees, 0.01f));

	// Yaw 는 여전히 보스를 향해야 한다(Boss-Facing 계약 유지).
	const FVector SideBossPosition(0.0f, 3500.0f, 800.0f);
	FDroneCombatCameraView SideView;
	ADrone::CalculateFixedBossFacingQuarterView(
		PlayerPosition,
		&SideBossPosition,
		FVector::ForwardVector,
		1400.0f,
		380.0f,
		PitchDegrees,
		SideView);
	TestTrue(TEXT("camera yaw still faces the boss"),
		FMath::IsNearlyEqual(SideView.CameraRotation.Yaw, 90.0f, 0.01f));
	TestTrue(TEXT("camera roll stays zero"),
		FMath::IsNearlyEqual(SideView.CameraRotation.Roll, 0.0f, 0.01f));

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
	FDroneFixedBossFacingCameraRelativeInputTest,
	"DroneProto.D18.Drone.CameraRelativeInputConversion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneFixedBossFacingCameraRelativeInputTest::RunTest(const FString& Parameters)
{
	const auto ExpectDirection = [this](const TCHAR* Label, FVector2D RawAxis, float CameraYaw, FVector2D ExpectedDirection)
	{
		const FVector2D Converted = ADrone::ConvertScreenInputToWorldMoveDirection(
			RawAxis,
			FRotator(0.0f, CameraYaw, 0.0f));
		TestTrue(Label, Converted.Equals(ExpectedDirection, 0.001f));
	};

	ExpectDirection(TEXT("yaw 0 up maps to world +X"), FVector2D(0.0f, 1.0f), 0.0f, FVector2D(1.0f, 0.0f));
	ExpectDirection(TEXT("yaw 0 down maps to world -X"), FVector2D(0.0f, -1.0f), 0.0f, FVector2D(-1.0f, 0.0f));
	ExpectDirection(TEXT("yaw 0 right maps to world +Y"), FVector2D(1.0f, 0.0f), 0.0f, FVector2D(0.0f, 1.0f));
	ExpectDirection(TEXT("yaw 0 left maps to world -Y"), FVector2D(-1.0f, 0.0f), 0.0f, FVector2D(0.0f, -1.0f));

	ExpectDirection(TEXT("yaw 90 up maps to world +Y"), FVector2D(0.0f, 1.0f), 90.0f, FVector2D(0.0f, 1.0f));
	ExpectDirection(TEXT("yaw 90 right maps to world -X"), FVector2D(1.0f, 0.0f), 90.0f, FVector2D(-1.0f, 0.0f));
	ExpectDirection(TEXT("yaw 180 up maps to world -X"), FVector2D(0.0f, 1.0f), 180.0f, FVector2D(-1.0f, 0.0f));
	ExpectDirection(TEXT("yaw -90 up maps to world -Y"), FVector2D(0.0f, 1.0f), -90.0f, FVector2D(0.0f, -1.0f));

	const FVector2D Diagonal = ADrone::ConvertScreenInputToWorldMoveDirection(
		FVector2D(1.0f, 1.0f),
		FRotator(0.0f, 0.0f, 0.0f));
	TestTrue(TEXT("diagonal camera-relative input is normalized"),
		FMath::IsNearlyEqual(Diagonal.Size(), 1.0f, 0.001f));
	TestTrue(TEXT("zero camera-relative input stays zero"),
		ADrone::ConvertScreenInputToWorldMoveDirection(FVector2D::ZeroVector, FRotator::ZeroRotator).IsNearlyZero());

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
