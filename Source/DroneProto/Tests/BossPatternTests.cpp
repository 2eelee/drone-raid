#include "CoreMinimal.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Raid/BossPatternActorBase.h"
#include "Raid/BossPatternComponent.h"
#include "Raid/BossPatternTypes.h"
#include "Raid/RaidBoss.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
int32 CountPatternActors(UWorld* World)
{
	int32 Count = 0;
	for (TActorIterator<ABossPatternActorBase> It(World); It; ++It)
	{
		if (*It && !It->IsActorBeingDestroyed())
		{
			++Count;
		}
	}
	return Count;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneBossPatternCanonicalFallbackTest,
	"DroneProto.BossPattern.Contract.CanonicalFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneBossPatternCanonicalFallbackTest::RunTest(const FString& Parameters)
{
	const FBossPatternConfig Common;
	const FCorruptedActinoConfig Corrupted;
	const FStellarRemnantConfig Stellar;

	TestEqual(TEXT("first delay"), Common.FirstDelaySeconds, 0.5f);
	TestEqual(TEXT("intermission includes telegraph"), Common.IntermissionSeconds, 2.0f);
	TestEqual(TEXT("global HitLock"), Common.GlobalHitLockSeconds, 0.7f);
	TestEqual(TEXT("Corrupted duration"), Common.CorruptedDurationSeconds, 5.0f);
	TestEqual(TEXT("Corrupted telegraph"), Common.CorruptedTelegraphSeconds, 1.0f);
	TestEqual(TEXT("Corrupted damage"), Common.CorruptedDamage, 20);
	TestEqual(TEXT("Stellar duration"), Common.StellarDurationSeconds, 3.0f);
	TestEqual(TEXT("Stellar telegraph"), Common.StellarTelegraphSeconds, 0.8f);
	TestEqual(TEXT("Stellar damage"), Common.StellarDamage, 25);

	TestEqual(TEXT("Corrupted laser count"), Corrupted.LaserCount, 4);
	TestEqual(TEXT("Corrupted start radius"), Corrupted.StartRadiusCm, 800.0f);
	TestEqual(TEXT("Corrupted end radius"), Corrupted.EndRadiusCm, 5000.0f);
	TestEqual(TEXT("Corrupted length"), Corrupted.LengthCm, 4200.0f);
	TestEqual(TEXT("Corrupted inner collision full width"), Corrupted.InnerCollisionFullWidthCm, 400.0f);
	TestEqual(TEXT("Corrupted outer collision full width"), Corrupted.OuterCollisionFullWidthCm, 800.0f);
	TestEqual(TEXT("Corrupted inner visual full width"), Corrupted.InnerVisualFullWidthCm, 500.0f);
	TestEqual(TEXT("Corrupted outer visual full width"), Corrupted.OuterVisualFullWidthCm, 1200.0f);
	TestEqual(TEXT("Corrupted collision height"), Corrupted.CollisionFullHeightCm, 150.0f);
	TestEqual(TEXT("Corrupted Z amplitude is independent"), Corrupted.ZAmplitudeCm, 300.0f);
	TestEqual(TEXT("Corrupted Z period"), Corrupted.ZPeriodSeconds, 2.0f);
	TestEqual(TEXT("Corrupted XY amplitude"), Corrupted.XYAmplitudeDegrees, 25.0f);
	TestEqual(TEXT("Corrupted XY period"), Corrupted.XYPeriodSeconds, 5.0f);

	TestEqual(TEXT("preset A base angle"), Corrupted.Presets[0].BaseAngleDegrees, 0.0f);
	TestTrue(TEXT("preset A starts at -25 and rises"),
		FMath::IsNearlyEqual(Corrupted.Presets[0].XYPhaseRadians, -HALF_PI));
	TestTrue(TEXT("preset A starts at +3m"),
		FMath::IsNearlyEqual(Corrupted.Presets[0].ZPhaseRadians, HALF_PI));
	TestEqual(TEXT("preset B base angle"), Corrupted.Presets[1].BaseAngleDegrees, 90.0f);
	TestTrue(TEXT("preset B starts at +25 and falls"),
		FMath::IsNearlyEqual(Corrupted.Presets[1].XYPhaseRadians, HALF_PI));
	TestTrue(TEXT("preset B starts centered and rises"),
		FMath::IsNearlyZero(Corrupted.Presets[1].ZPhaseRadians));
	TestEqual(TEXT("preset C base angle"), Corrupted.Presets[2].BaseAngleDegrees, 180.0f);
	TestTrue(TEXT("preset C starts at -25 and rises"),
		FMath::IsNearlyEqual(Corrupted.Presets[2].XYPhaseRadians, -HALF_PI));
	TestTrue(TEXT("preset C starts at -3m"),
		FMath::IsNearlyEqual(Corrupted.Presets[2].ZPhaseRadians, -HALF_PI));
	TestEqual(TEXT("preset D base angle"), Corrupted.Presets[3].BaseAngleDegrees, 270.0f);
	TestTrue(TEXT("preset D starts at +25 and falls"),
		FMath::IsNearlyEqual(Corrupted.Presets[3].XYPhaseRadians, HALF_PI));
	TestTrue(TEXT("preset D starts centered and falls"),
		FMath::IsNearlyEqual(Corrupted.Presets[3].ZPhaseRadians, PI));

	TestEqual(TEXT("Stellar wave count"), Stellar.WaveCount, 2);
	TestEqual(TEXT("Stellar damage projectile count"), Stellar.DamageProjectileCount, 32);
	TestEqual(TEXT("Stellar visual-only count"), Stellar.VisualProjectileCount, 16);
	TestEqual(TEXT("Stellar damage per wave"), Stellar.DamageProjectilesPerWave, 16);
	TestEqual(TEXT("Stellar visual-only per wave"), Stellar.VisualProjectilesPerWave, 8);
	TestEqual(TEXT("Stellar wave interval"), Stellar.WaveIntervalSeconds, 0.5f);
	TestEqual(TEXT("Stellar travel time"), Stellar.TravelSeconds, 2.5f);
	TestEqual(TEXT("Stellar speed"), Stellar.SpeedCmPerSecond, 1680.0f);
	TestEqual(TEXT("Stellar collision radius"), Stellar.CollisionRadiusCm, 70.0f);
	TestEqual(TEXT("Stellar second-wave offset"), Stellar.SecondWaveOffsetDegrees, 11.25f);
	TestEqual(TEXT("Stellar visual Z offset"), Stellar.VisualZOffsetCm, 300.0f);
	TestEqual(TEXT("visual-only damage"), Stellar.VisualDamage, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneBossPatternLifecycleSequenceTest,
	"DroneProto.BossPattern.Lifecycle.Sequence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneBossPatternLifecycleSequenceTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("BossPatternLifecycleSequenceWorld")));
	ARaidBoss* Boss = World ? World->SpawnActor<ARaidBoss>() : nullptr;
	UBossPatternComponent* Component = Boss ? Boss->FindComponentByClass<UBossPatternComponent>() : nullptr;
	TestNotNull(TEXT("world is created"), World);
	TestNotNull(TEXT("boss is spawned"), Boss);
	TestNotNull(TEXT("pattern component exists"), Component);
	if (!World || !Boss || !Component)
	{
		if (World)
		{
			World->DestroyWorld(false);
		}
		return false;
	}

	TestTrue(TEXT("start succeeds"), Boss->StartBossPatternForServer());
	TestEqual(TEXT("start enters first delay"), Component->GetServerStateForTest(), EBossPatternServerState::FirstDelay);
	TestEqual(TEXT("first delay is canonical"), Component->GetPendingDelayForTest(), 0.5f);
	TestEqual(TEXT("first delay has no actor"), CountPatternActors(World), 0);

	TestTrue(TEXT("first transition fires"), Component->FireScheduledTransitionForTest());
	TestEqual(TEXT("first pattern is Corrupted"), Component->GetCurrentPatternForTest(), EBossPatternKind::CorruptedActino);
	TestEqual(TEXT("first Corrupted skips telegraph"), Component->GetServerStateForTest(), EBossPatternServerState::Active);
	TestEqual(TEXT("first Corrupted owns one actor"), CountPatternActors(World), 1);

	TestTrue(TEXT("Corrupted completes"), Component->FireScheduledTransitionForTest());
	TestEqual(TEXT("C to S enters intermission"), Component->GetServerStateForTest(), EBossPatternServerState::Intermission);
	TestEqual(TEXT("C to S wait excludes telegraph"), Component->GetPendingDelayForTest(), 1.2f);
	TestEqual(TEXT("intermission destroys actor"), CountPatternActors(World), 0);

	TestTrue(TEXT("Stellar telegraph starts"), Component->FireScheduledTransitionForTest());
	TestEqual(TEXT("next pattern is Stellar"), Component->GetCurrentPatternForTest(), EBossPatternKind::StellarRemnant);
	TestEqual(TEXT("Stellar telegraphs"), Component->GetServerStateForTest(), EBossPatternServerState::Telegraphing);
	TestEqual(TEXT("Stellar telegraph duration"), Component->GetPendingDelayForTest(), 0.8f);
	ABossPatternActorBase* StellarActor = Component->GetActivePatternActorForTest();
	TestNotNull(TEXT("Stellar telegraph owns actor"), StellarActor);
	TestEqual(TEXT("Stellar telegraph owns one actor"), CountPatternActors(World), 1);

	TestTrue(TEXT("Stellar becomes active"), Component->FireScheduledTransitionForTest());
	TestEqual(TEXT("Stellar is active"), Component->GetServerStateForTest(), EBossPatternServerState::Active);
	TestEqual(TEXT("Stellar keeps the telegraph actor"), Component->GetActivePatternActorForTest(), StellarActor);
	TestEqual(TEXT("Stellar active duration"), Component->GetPendingDelayForTest(), 3.0f);

	TestTrue(TEXT("Stellar completes"), Component->FireScheduledTransitionForTest());
	TestEqual(TEXT("S to C enters intermission"), Component->GetServerStateForTest(), EBossPatternServerState::Intermission);
	TestEqual(TEXT("S to C wait excludes telegraph"), Component->GetPendingDelayForTest(), 1.0f);

	TestTrue(TEXT("Corrupted telegraph starts"), Component->FireScheduledTransitionForTest());
	TestEqual(TEXT("loop returns to Corrupted"), Component->GetCurrentPatternForTest(), EBossPatternKind::CorruptedActino);
	TestEqual(TEXT("later Corrupted telegraphs"), Component->GetServerStateForTest(), EBossPatternServerState::Telegraphing);
	TestEqual(TEXT("Corrupted telegraph duration"), Component->GetPendingDelayForTest(), 1.0f);
	TestEqual(TEXT("loop still owns one actor"), CountPatternActors(World), 1);

	Boss->StopBossPatternForServer(FName(TEXT("Automation")));
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneBossPatternLifecycleGuardsTest,
	"DroneProto.BossPattern.Lifecycle.Guards",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneBossPatternLifecycleGuardsTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("BossPatternLifecycleGuardsWorld")));
	ARaidBoss* Boss = World ? World->SpawnActor<ARaidBoss>() : nullptr;
	UBossPatternComponent* Component = Boss ? Boss->FindComponentByClass<UBossPatternComponent>() : nullptr;
	if (!World || !Boss || !Component)
	{
		TestTrue(TEXT("guard test setup"), false);
		if (World)
		{
			World->DestroyWorld(false);
		}
		return false;
	}

	TestTrue(TEXT("initial start succeeds"), Boss->StartBossPatternForServer());
	TestFalse(TEXT("duplicate start is ignored"), Boss->StartBossPatternForServer());
	const int32 StaleSerial = Component->GetTransitionSerialForTest();
	Boss->StopBossPatternForServer(FName(TEXT("FirstStop")));
	Boss->StopBossPatternForServer(FName(TEXT("IdempotentStop")));
	TestEqual(TEXT("stop is idempotent"), Component->GetServerStateForTest(), EBossPatternServerState::Stopped);
	TestFalse(TEXT("stop clears timer"), Component->IsTransitionTimerActiveForTest());
	TestEqual(TEXT("stop clears actor"), CountPatternActors(World), 0);

	TestTrue(TEXT("restart succeeds"), Boss->StartBossPatternForServer());
	TestFalse(TEXT("stale transition is rejected"), Component->FireTransitionForTest(StaleSerial));
	TestEqual(TEXT("stale transition cannot skip first delay"), Component->GetServerStateForTest(), EBossPatternServerState::FirstDelay);
	TestEqual(TEXT("stale transition cannot spawn actor"), CountPatternActors(World), 0);

	Boss->StopBossPatternForServer(FName(TEXT("Automation")));
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneBossPatternProductionIsolationTest,
	"DroneProto.BossPattern.Lifecycle.ProductionIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneBossPatternProductionIsolationTest::RunTest(const FString& Parameters)
{
	const FString ComponentPath = FPaths::ProjectDir() / TEXT("Source/DroneProto/Raid/BossPatternComponent.cpp");
	FString ComponentSource;
	TestTrue(TEXT("production scheduler source loads"), FFileHelper::LoadFileToString(ComponentSource, *ComponentPath));
	if (ComponentSource.IsEmpty())
	{
		return false;
	}

	TestFalse(TEXT("production scheduler excludes direct circular attack"), ComponentSource.Contains(TEXT("PerformDebugAreaAttackForServer")));
	TestFalse(TEXT("production scheduler excludes circular telegraph API"), ComponentSource.Contains(TEXT("StartDebugTelegraphedAreaAttackForServer")));
	TestFalse(TEXT("production scheduler excludes circular telegraph actor"), ComponentSource.Contains(TEXT("ARaidBossAttackTelegraph")));
	return true;
}

#endif
