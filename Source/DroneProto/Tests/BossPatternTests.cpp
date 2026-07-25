#include "CoreMinimal.h"
#include "CoreGlobals.h"
#include "Drone.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Raid/BossPatternActorBase.h"
#include "Raid/BossPatternComponent.h"
#include "Raid/BossPatternDataTableRows.h"
#include "Raid/BossPatternDataTableResolver.h"
#include "Raid/BossPatternTypes.h"
#include "Raid/CorruptedActinoPatternActor.h"
#include "Raid/DronePartInventory.h"
#include "Raid/RaidBoss.h"
#include "Raid/RaidGameMode.h"
#include "Raid/RaidGameState.h"
#include "Raid/RaidPlayerController.h"
#include "Raid/StellarRemnantPatternActor.h"
#include "TimerManager.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FBossPatternDataTableSet MakeEmptyBossPatternDataTableSet()
{
	return FBossPatternDataTableSet();
}

template <typename RowType>
UDataTable* MakeBossPatternTable()
{
	UDataTable* Table = NewObject<UDataTable>();
	Table->RowStruct = RowType::StaticStruct();
	return Table;
}

struct FCanonicalBossPatternTables
{
	UDataTable* Boss = MakeBossPatternTable<FBossPatternSystemRow>();
	UDataTable* Corrupted = MakeBossPatternTable<FCorruptedActinoRow>();
	UDataTable* Presets = MakeBossPatternTable<FCorruptedActinoPresetRow>();
	UDataTable* Stellar = MakeBossPatternTable<FStellarRemnantRow>();

	FCanonicalBossPatternTables()
	{
		FBossPatternSystemRow BossRow;
		BossRow.PatternSystemID = 10001;
		BossRow.PatternOrder = TEXT("1\u21922 Repeat");
		BossRow.PatternInterval = 2.0f;
		BossRow.FirstPatternDelay = 0.5f;
		BossRow.HitInvincibleDuration = 0.7f;
		BossRow.PlayerMaxHPReference = 100;
		Boss->AddRow(TEXT("BOSS_PATTERN_SYSTEM_001"), BossRow);

		FCorruptedActinoRow CorruptedRow;
		CorruptedRow.PatternID = 11001;
		CorruptedRow.PatternName = TEXT("Corrupted Actino");
		CorruptedRow.Difficulty = TEXT("Middle");
		CorruptedRow.Damage = 20;
		CorruptedRow.Duration = 5.0f;
		CorruptedRow.Telegraph = 1.0f;
		CorruptedRow.LaserCount = 4;
		CorruptedRow.StartDistance = 8.0f;
		CorruptedRow.EndDistance = 50.0f;
		CorruptedRow.Length = 42.0f;
		CorruptedRow.InnerHitWidth = 4.0f;
		CorruptedRow.OuterHitWidth = 8.0f;
		CorruptedRow.InnerVisualWidth = 5.0f;
		CorruptedRow.OuterVisualWidth = 12.0f;
		CorruptedRow.CollisionHeight = 1.5f;
		CorruptedRow.ZAmplitude = 3.0f;
		CorruptedRow.ZOscillationPeriod = 2.0f;
		CorruptedRow.AngleSweepRange = 25.0f;
		CorruptedRow.FirstUseTelegraph = false;
		Corrupted->AddRow(TEXT("PATTERN_001"), CorruptedRow);

		static const float BaseAngles[4] = {0.0f, 90.0f, 180.0f, 270.0f};
		static const float Phases[4] = {0.0f, 0.25f, 0.5f, 0.75f};
		static const TCHAR* Directions[4] =
		{
			TEXT("Clockwise"), TEXT("CounterClockwise"), TEXT("Clockwise"), TEXT("CounterClockwise")
		};
		static const TCHAR* ZStates[4] =
		{
			TEXT("Upper"), TEXT("CenterUp"), TEXT("Lower"), TEXT("CenterDown")
		};
		for (int32 Index = 0; Index < 4; ++Index)
		{
			FCorruptedActinoPresetRow Preset;
			Preset.LaserIndex = Index + 1;
			Preset.BaseAngle = BaseAngles[Index];
			Preset.SweepDirection = Directions[Index];
			Preset.XYPhase = Phases[Index];
			Preset.ZPhase = Phases[Index];
			Preset.ZStartState = ZStates[Index];
			Presets->AddRow(FName(*FString::Printf(TEXT("ACTINO_LASER_%02d"), Index + 1)), Preset);
		}

		FStellarRemnantRow StellarRow;
		StellarRow.PatternID = 11002;
		StellarRow.PatternName = TEXT("Stellar Remnant");
		StellarRow.Difficulty = TEXT("MiddleHigh");
		StellarRow.Damage = 25;
		StellarRow.Duration = 3.0f;
		StellarRow.Telegraph = 0.8f;
		StellarRow.WaveCount = 2;
		StellarRow.DamageCount = 32;
		StellarRow.VisualCount = 16;
		StellarRow.DamagePerWave = 16;
		StellarRow.VisualPerWave = 8;
		StellarRow.WaveInterval = 0.5f;
		StellarRow.StartDistance = 8.0f;
		StellarRow.EndDistance = 50.0f;
		StellarRow.Length = 42.0f;
		StellarRow.MoveDuration = 2.5f;
		StellarRow.MoveSpeed = 16.8f;
		StellarRow.HitRadius = 0.7f;
		StellarRow.VisualSizeMin = 1.0f;
		StellarRow.VisualSizeMax = 1.2f;
		StellarRow.SecondWaveOffset = 11.25f;
		StellarRow.VisualZOffset = 3.0f;
		StellarRow.VisualDamage = 0;
		Stellar->AddRow(TEXT("PATTERN_002"), StellarRow);
	}

	FBossPatternDataTableSet AsSet() const
	{
		return {Boss, Corrupted, Presets, Stellar};
	}
};

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

struct FBossPatternPlayerTestContext
{
	UWorld* World = nullptr;
	ARaidGameState* GameState = nullptr;
	ARaidBoss* Boss = nullptr;
	UBossPatternComponent* Component = nullptr;
	ARaidPlayerController* PlayerController = nullptr;
	ADrone* Drone = nullptr;
};

struct FBossPatternTestPlayer
{
	ARaidPlayerController* PlayerController = nullptr;
	ADrone* Drone = nullptr;
};

FBossPatternTestPlayer SpawnBossPatternTestPlayer(UWorld* World, bool bEnterBattle)
{
	FBossPatternTestPlayer Player;
	if (!World)
	{
		return Player;
	}
	Player.PlayerController = World->SpawnActor<ARaidPlayerController>();
	Player.Drone = World->SpawnActor<ADrone>();
	if (Player.PlayerController && Player.Drone)
	{
		Player.PlayerController->Possess(Player.Drone);
		if (bEnterBattle)
		{
			Player.PlayerController->Server_RequestReadyForRaid_Implementation();
		}
	}
	return Player;
}

FBossPatternPlayerTestContext CreateBossPatternPlayerTestContext(const TCHAR* WorldName)
{
	FBossPatternPlayerTestContext Context;
	Context.World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
	if (!Context.World)
	{
		return Context;
	}

	Context.GameState = Context.World->SpawnActor<ARaidGameState>();
	Context.Boss = Context.World->SpawnActor<ARaidBoss>();
	Context.Component = Context.Boss ? Context.Boss->FindComponentByClass<UBossPatternComponent>() : nullptr;
	if (Context.Component)
	{
		Context.Component->ResolvePatternDataForTest();
	}
	if (Context.GameState)
	{
		Context.World->SetGameState(Context.GameState);
		Context.GameState->SetRaidBossForServer(Context.Boss);
		Context.GameState->SetRaidStateForServer(ERaidState::Battle);
	}
	const FBossPatternTestPlayer Player = SpawnBossPatternTestPlayer(Context.World, true);
	Context.PlayerController = Player.PlayerController;
	Context.Drone = Player.Drone;
	return Context;
}

void DestroyBossPatternPlayerTestContext(FBossPatternPlayerTestContext& Context)
{
	if (Context.World)
	{
		Context.World->DestroyWorld(false);
	}
	Context = FBossPatternPlayerTestContext();
}

void TickBossPatternTimers(UWorld* World, float DeltaSeconds)
{
	if (World)
	{
		++GFrameCounter;
		World->GetTimerManager().Tick(KINDA_SMALL_NUMBER);
		++GFrameCounter;
		World->GetTimerManager().Tick(DeltaSeconds);
	}
}

FVector MakeCorruptedLaserPoint(
	const FTransform& BossTransform,
	const FCorruptedActinoLaserPreset& Preset,
	float ElapsedSeconds,
	float LongitudinalCm,
	float LateralCm,
	float VerticalOffsetCm)
{
	const float AngleRadians = FMath::DegreesToRadians(
		ACorruptedActinoPatternActor::EvaluateAngleDegrees(Preset, ElapsedSeconds));
	const FVector LocalDirection(FMath::Cos(AngleRadians), FMath::Sin(AngleRadians), 0.0f);
	const FVector LocalRight(-LocalDirection.Y, LocalDirection.X, 0.0f);
	const FCorruptedActinoConfig Config;
	const FVector LocalPoint = LocalDirection * (Config.StartRadiusCm + LongitudinalCm)
		+ LocalRight * LateralCm
		+ FVector::UpVector * (ACorruptedActinoPatternActor::EvaluateZCm(Preset, ElapsedSeconds) + VerticalOffsetCm);
	return BossTransform.TransformPosition(LocalPoint);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneBossPatternDataMissingTableReasonTest,
	"DroneProto.BossPattern.DataTable.MissingTableReason",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneBossPatternDataMissingTableReasonTest::RunTest(const FString& Parameters)
{
	FBossPatternResolvedConfig Resolved;
	EBossPatternDataFallbackReason Reason = EBossPatternDataFallbackReason::None;
	TestFalse(TEXT("an empty table set cannot resolve"), BossPatternData::TryResolve(
		MakeEmptyBossPatternDataTableSet(), Resolved, Reason));
	TestEqual(TEXT("the first deterministic reason is the common table"), Reason,
		EBossPatternDataFallbackReason::MissingBossPatternTable);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneBossPatternDataCanonicalResolveTest,
	"DroneProto.BossPattern.DataTable.CanonicalResolveAndStellarIndependence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneBossPatternDataCanonicalResolveTest::RunTest(const FString& Parameters)
{
	FCanonicalBossPatternTables Tables;
	FStellarRemnantRow* StellarRow = Tables.Stellar->FindRow<FStellarRemnantRow>(
		TEXT("PATTERN_002"), TEXT("Automation"));
	StellarRow->StartDistance = 9.0f;
	StellarRow->EndDistance = 51.0f;

	FBossPatternResolvedConfig Resolved;
	EBossPatternDataFallbackReason Reason = EBossPatternDataFallbackReason::MissingBossPatternTable;
	TestTrue(TEXT("canonical tables resolve"), BossPatternData::TryResolve(Tables.AsSet(), Resolved, Reason));
	TestEqual(TEXT("success clears reason"), Reason, EBossPatternDataFallbackReason::None);
	TestEqual(TEXT("common conversion"), Resolved.Common.GlobalHitLockSeconds, 0.7f);
	TestEqual(TEXT("Corrupted distance comes from Corrupted"), Resolved.Corrupted.StartRadiusCm, 800.0f);
	TestEqual(TEXT("Stellar start distance is independent"), Resolved.Stellar.StartRadiusCm, 900.0f);
	TestEqual(TEXT("Stellar end distance is independent"), Resolved.Stellar.EndRadiusCm, 5100.0f);
	TestTrue(TEXT("SweepDirection is runtime authority"),
		FMath::IsNearlyEqual(Resolved.Corrupted.Presets[0].XYPhaseRadians, -HALF_PI));
	TestTrue(TEXT("ZStartState is runtime authority"),
		FMath::IsNearlyEqual(Resolved.Corrupted.Presets[3].ZPhaseRadians, PI));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneBossPatternDataPresetAtomicFallbackTest,
	"DroneProto.BossPattern.DataTable.PresetContractAtomicFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneBossPatternDataPresetAtomicFallbackTest::RunTest(const FString& Parameters)
{
	FCanonicalBossPatternTables Tables;
	FCorruptedActinoPresetRow* Preset = Tables.Presets->FindRow<FCorruptedActinoPresetRow>(
		TEXT("ACTINO_LASER_03"), TEXT("Automation"));
	Preset->ZPhase = 0.75f;

	FBossPatternResolvedConfig Resolved;
	Resolved.Common.FirstDelaySeconds = 123.0f;
	Resolved.Stellar.StartRadiusCm = 456.0f;
	EBossPatternDataFallbackReason Reason = EBossPatternDataFallbackReason::None;
	TestFalse(TEXT("phase and state conflict fails all tables"),
		BossPatternData::TryResolve(Tables.AsSet(), Resolved, Reason));
	TestEqual(TEXT("preset failure reason is deterministic"), Reason,
		EBossPatternDataFallbackReason::InvalidCorruptedPresetContract);
	TestEqual(TEXT("failed resolve leaves common output untouched"), Resolved.Common.FirstDelaySeconds, 123.0f);
	TestEqual(TEXT("failed resolve leaves Stellar output untouched"), Resolved.Stellar.StartRadiusCm, 456.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneBossPatternResolvedSnapshotTest,
	"DroneProto.BossPattern.DataTable.RuntimeReadyAndActorSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneBossPatternResolvedSnapshotTest::RunTest(const FString& Parameters)
{
	UWorld* NotReadyWorld = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("BossPatternNotReadyWorld")));
	ARaidBoss* NotReadyBoss = NotReadyWorld ? NotReadyWorld->SpawnActor<ARaidBoss>() : nullptr;
	TestFalse(TEXT("pattern cannot start before resolved config is ready"),
		NotReadyBoss && NotReadyBoss->StartBossPatternForServer());
	TestEqual(TEXT("no actor exists before resolved config is ready"), CountPatternActors(NotReadyWorld), 0);
	if (NotReadyWorld)
	{
		NotReadyWorld->DestroyWorld(false);
	}

	FBossPatternPlayerTestContext Context = CreateBossPatternPlayerTestContext(TEXT("BossPatternResolvedSnapshotWorld"));
	TestNotNull(TEXT("component exists"), Context.Component);
	TestTrue(TEXT("resolved config is ready before start"),
		Context.Component && Context.Component->IsResolvedConfigReadyForTest());
	TestTrue(TEXT("pattern start succeeds after resolve"), Context.Boss && Context.Boss->StartBossPatternForServer());
	if (Context.Component)
	{
		Context.Component->FireScheduledTransitionForTest();
	}
	ABossPatternActorBase* Actor = Context.Component ? Context.Component->GetActivePatternActorForTest() : nullptr;
	TestNotNull(TEXT("pattern actor spawns"), Actor);
	TestTrue(TEXT("spawned actor owns a value snapshot"), Actor && Actor->HasResolvedConfigSnapshot());
	FBossPatternResolvedConfig OriginalSnapshot;
	TestTrue(TEXT("actor snapshot can be copied by value"),
		Actor && Actor->CopyResolvedConfigSnapshot(OriginalSnapshot));
	FBossPatternResolvedConfig Replacement = MakeCanonicalBossPatternResolvedConfig();
	Replacement.Common.FirstDelaySeconds = 99.0f;
	Replacement.Stellar.StartRadiusCm = 12345.0f;
	if (Actor)
	{
		Actor->SnapshotResolvedConfig(Replacement);
	}
	FBossPatternResolvedConfig SnapshotAfterSecondWrite;
	TestTrue(TEXT("actor snapshot remains readable"),
		Actor && Actor->CopyResolvedConfigSnapshot(SnapshotAfterSecondWrite));
	TestEqual(TEXT("actor snapshot is immutable after creation"),
		SnapshotAfterSecondWrite.Common.FirstDelaySeconds, OriginalSnapshot.Common.FirstDelaySeconds);
	TestEqual(TEXT("actor never rereads replacement Stellar data"),
		SnapshotAfterSecondWrite.Stellar.StartRadiusCm, OriginalSnapshot.Stellar.StartRadiusCm);
	DestroyBossPatternPlayerTestContext(Context);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneBossPatternDataAssetContractTest,
	"DroneProto.BossPattern.DataTable.AssetPathsAndHardReferences",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneBossPatternDataAssetContractTest::RunTest(const FString& Parameters)
{
	struct FExpectedAsset
	{
		const TCHAR* Path;
		const TCHAR* ComponentProperty;
		UScriptStruct* RowStruct;
	};
	const FExpectedAsset ExpectedAssets[] =
	{
		{TEXT("/Game/Data/BossPattern/DT_BossPattern.DT_BossPattern"), TEXT("BossPatternDataTable"), FBossPatternSystemRow::StaticStruct()},
		{TEXT("/Game/Data/BossPattern/DT_CorruptedActino.DT_CorruptedActino"), TEXT("CorruptedActinoDataTable"), FCorruptedActinoRow::StaticStruct()},
		{TEXT("/Game/Data/BossPattern/DT_CorruptedActinoPreset.DT_CorruptedActinoPreset"), TEXT("CorruptedActinoPresetDataTable"), FCorruptedActinoPresetRow::StaticStruct()},
		{TEXT("/Game/Data/BossPattern/DT_StellarRemnant.DT_StellarRemnant"), TEXT("StellarRemnantDataTable"), FStellarRemnantRow::StaticStruct()}
	};
	const UBossPatternComponent* ComponentCDO = GetDefault<UBossPatternComponent>();
	for (const FExpectedAsset& Expected : ExpectedAssets)
	{
		const UDataTable* Table = Cast<UDataTable>(FSoftObjectPath(Expected.Path).TryLoad());
		TestNotNull(FString::Printf(TEXT("asset loads: %s"), Expected.Path), Table);
		if (Table)
		{
			TestEqual(FString::Printf(TEXT("typed row: %s"), Expected.Path), Table->RowStruct.Get(), Expected.RowStruct);
		}
		const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(
			UBossPatternComponent::StaticClass(), FName(Expected.ComponentProperty));
		TestNotNull(FString::Printf(TEXT("hard reference property: %s"), Expected.ComponentProperty), Property);
		if (Property && ComponentCDO)
		{
			const UObject* ReferencedObject = Property->GetObjectPropertyValue_InContainer(ComponentCDO);
			TestTrue(FString::Printf(TEXT("CDO hard reference: %s"), Expected.Path), ReferencedObject == Table);
		}
	}

	FString ComponentSource;
	const FString ComponentPath = FPaths::ProjectDir() / TEXT("Source/DroneProto/Raid/BossPatternComponent.cpp");
	TestTrue(TEXT("component source loads"), FFileHelper::LoadFileToString(ComponentSource, *ComponentPath));
	for (const FExpectedAsset& Expected : ExpectedAssets)
	{
		TestTrue(FString::Printf(TEXT("hard reference is present: %s"), Expected.Path),
			ComponentSource.Contains(Expected.Path));
	}
	TestFalse(TEXT("runtime does not read CSV"), ComponentSource.Contains(TEXT("FillDataTableFromCSV"))
		|| ComponentSource.Contains(TEXT("LoadFileToString")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneBossPatternDataAssetCanonicalEqualityTest,
	"DroneProto.BossPattern.DataTable.AssetCanonicalEquality",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneBossPatternDataAssetCanonicalEqualityTest::RunTest(const FString& Parameters)
{
	const FBossPatternDataTableSet Tables =
	{
		Cast<UDataTable>(FSoftObjectPath(TEXT("/Game/Data/BossPattern/DT_BossPattern.DT_BossPattern")).TryLoad()),
		Cast<UDataTable>(FSoftObjectPath(TEXT("/Game/Data/BossPattern/DT_CorruptedActino.DT_CorruptedActino")).TryLoad()),
		Cast<UDataTable>(FSoftObjectPath(TEXT("/Game/Data/BossPattern/DT_CorruptedActinoPreset.DT_CorruptedActinoPreset")).TryLoad()),
		Cast<UDataTable>(FSoftObjectPath(TEXT("/Game/Data/BossPattern/DT_StellarRemnant.DT_StellarRemnant")).TryLoad())
	};
	FBossPatternResolvedConfig Actual;
	EBossPatternDataFallbackReason Reason = EBossPatternDataFallbackReason::None;
	TestTrue(TEXT("asset tables resolve"), BossPatternData::TryResolve(Tables, Actual, Reason));
	FBossPatternResolvedConfig ClientActual;
	EBossPatternDataFallbackReason ClientReason = EBossPatternDataFallbackReason::None;
	TestTrue(TEXT("client resolves the same local assets"), BossPatternData::TryResolve(Tables, ClientActual, ClientReason));
	TestEqual(TEXT("server and client common config match"),
		Actual.Common.FirstDelaySeconds, ClientActual.Common.FirstDelaySeconds);
	TestEqual(TEXT("server and client Corrupted config match"),
		Actual.Corrupted.Presets[3].ZPhaseRadians, ClientActual.Corrupted.Presets[3].ZPhaseRadians);
	TestEqual(TEXT("server and client Stellar config match"),
		Actual.Stellar.StartRadiusCm, ClientActual.Stellar.StartRadiusCm);
	const FBossPatternResolvedConfig Expected = MakeCanonicalBossPatternResolvedConfig();

	TestEqual(TEXT("common first delay"), Actual.Common.FirstDelaySeconds, Expected.Common.FirstDelaySeconds);
	TestEqual(TEXT("common interval"), Actual.Common.IntermissionSeconds, Expected.Common.IntermissionSeconds);
	TestEqual(TEXT("common hit lock"), Actual.Common.GlobalHitLockSeconds, Expected.Common.GlobalHitLockSeconds);
	TestEqual(TEXT("Corrupted duration"), Actual.Common.CorruptedDurationSeconds, Expected.Common.CorruptedDurationSeconds);
	TestEqual(TEXT("Corrupted telegraph"), Actual.Common.CorruptedTelegraphSeconds, Expected.Common.CorruptedTelegraphSeconds);
	TestEqual(TEXT("Corrupted damage"), Actual.Common.CorruptedDamage, Expected.Common.CorruptedDamage);
	TestEqual(TEXT("Stellar duration"), Actual.Common.StellarDurationSeconds, Expected.Common.StellarDurationSeconds);
	TestEqual(TEXT("Stellar telegraph"), Actual.Common.StellarTelegraphSeconds, Expected.Common.StellarTelegraphSeconds);
	TestEqual(TEXT("Stellar damage"), Actual.Common.StellarDamage, Expected.Common.StellarDamage);

	TestEqual(TEXT("Corrupted laser count"), Actual.Corrupted.LaserCount, Expected.Corrupted.LaserCount);
	TestEqual(TEXT("Corrupted start"), Actual.Corrupted.StartRadiusCm, Expected.Corrupted.StartRadiusCm);
	TestEqual(TEXT("Corrupted end"), Actual.Corrupted.EndRadiusCm, Expected.Corrupted.EndRadiusCm);
	TestEqual(TEXT("Corrupted length"), Actual.Corrupted.LengthCm, Expected.Corrupted.LengthCm);
	TestEqual(TEXT("Corrupted inner hit"), Actual.Corrupted.InnerCollisionFullWidthCm, Expected.Corrupted.InnerCollisionFullWidthCm);
	TestEqual(TEXT("Corrupted outer hit"), Actual.Corrupted.OuterCollisionFullWidthCm, Expected.Corrupted.OuterCollisionFullWidthCm);
	TestEqual(TEXT("Corrupted inner visual"), Actual.Corrupted.InnerVisualFullWidthCm, Expected.Corrupted.InnerVisualFullWidthCm);
	TestEqual(TEXT("Corrupted outer visual"), Actual.Corrupted.OuterVisualFullWidthCm, Expected.Corrupted.OuterVisualFullWidthCm);
	TestEqual(TEXT("Corrupted collision height"), Actual.Corrupted.CollisionFullHeightCm, Expected.Corrupted.CollisionFullHeightCm);
	TestEqual(TEXT("Corrupted Z amplitude"), Actual.Corrupted.ZAmplitudeCm, Expected.Corrupted.ZAmplitudeCm);
	TestEqual(TEXT("Corrupted Z period"), Actual.Corrupted.ZPeriodSeconds, Expected.Corrupted.ZPeriodSeconds);
	TestEqual(TEXT("Corrupted XY amplitude"), Actual.Corrupted.XYAmplitudeDegrees, Expected.Corrupted.XYAmplitudeDegrees);
	TestEqual(TEXT("Corrupted XY period"), Actual.Corrupted.XYPeriodSeconds, Expected.Corrupted.XYPeriodSeconds);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		TestEqual(TEXT("preset base"), Actual.Corrupted.Presets[Index].BaseAngleDegrees, Expected.Corrupted.Presets[Index].BaseAngleDegrees);
		TestTrue(TEXT("preset direction phase"), FMath::IsNearlyEqual(
			Actual.Corrupted.Presets[Index].XYPhaseRadians, Expected.Corrupted.Presets[Index].XYPhaseRadians));
		TestTrue(TEXT("preset Z state phase"), FMath::IsNearlyEqual(
			Actual.Corrupted.Presets[Index].ZPhaseRadians, Expected.Corrupted.Presets[Index].ZPhaseRadians));
	}

	TestEqual(TEXT("Stellar waves"), Actual.Stellar.WaveCount, Expected.Stellar.WaveCount);
	TestEqual(TEXT("Stellar damage count"), Actual.Stellar.DamageProjectileCount, Expected.Stellar.DamageProjectileCount);
	TestEqual(TEXT("Stellar visual count"), Actual.Stellar.VisualProjectileCount, Expected.Stellar.VisualProjectileCount);
	TestEqual(TEXT("Stellar damage per wave"), Actual.Stellar.DamageProjectilesPerWave, Expected.Stellar.DamageProjectilesPerWave);
	TestEqual(TEXT("Stellar visual per wave"), Actual.Stellar.VisualProjectilesPerWave, Expected.Stellar.VisualProjectilesPerWave);
	TestEqual(TEXT("Stellar interval"), Actual.Stellar.WaveIntervalSeconds, Expected.Stellar.WaveIntervalSeconds);
	TestEqual(TEXT("Stellar independent start"), Actual.Stellar.StartRadiusCm, Expected.Stellar.StartRadiusCm);
	TestEqual(TEXT("Stellar independent end"), Actual.Stellar.EndRadiusCm, Expected.Stellar.EndRadiusCm);
	TestEqual(TEXT("Stellar independent length"), Actual.Stellar.LengthCm, Expected.Stellar.LengthCm);
	TestEqual(TEXT("Stellar travel"), Actual.Stellar.TravelSeconds, Expected.Stellar.TravelSeconds);
	TestTrue(TEXT("Stellar speed"), FMath::IsNearlyEqual(
		Actual.Stellar.SpeedCmPerSecond, Expected.Stellar.SpeedCmPerSecond, 0.01f));
	TestEqual(TEXT("Stellar hit radius"), Actual.Stellar.CollisionRadiusCm, Expected.Stellar.CollisionRadiusCm);
	TestEqual(TEXT("Stellar angle step"), Actual.Stellar.DamageAngleStepDegrees, Expected.Stellar.DamageAngleStepDegrees);
	TestEqual(TEXT("Stellar wave offset"), Actual.Stellar.SecondWaveOffsetDegrees, Expected.Stellar.SecondWaveOffsetDegrees);
	TestEqual(TEXT("Stellar visual Z"), Actual.Stellar.VisualZOffsetCm, Expected.Stellar.VisualZOffsetCm);
	TestEqual(TEXT("Stellar visual min"), Actual.Stellar.VisualFullSizeMinCm, Expected.Stellar.VisualFullSizeMinCm);
	TestEqual(TEXT("Stellar visual max"), Actual.Stellar.VisualFullSizeMaxCm, Expected.Stellar.VisualFullSizeMaxCm);
	TestEqual(TEXT("Stellar visual damage"), Actual.Stellar.VisualDamage, Expected.Stellar.VisualDamage);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneBossPatternDataValidationReasonsTest,
	"DroneProto.BossPattern.DataTable.DeterministicValidationReasons",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneBossPatternDataValidationReasonsTest::RunTest(const FString& Parameters)
{
	auto ResolveReason = [](const FCanonicalBossPatternTables& Tables)
	{
		FBossPatternResolvedConfig Config;
		EBossPatternDataFallbackReason Reason = EBossPatternDataFallbackReason::None;
		BossPatternData::TryResolve(Tables.AsSet(), Config, Reason);
		return Reason;
	};
	{
		FCanonicalBossPatternTables Tables;
		FBossPatternSystemRow Extra = *Tables.Boss->FindRow<FBossPatternSystemRow>(TEXT("BOSS_PATTERN_SYSTEM_001"), TEXT("Automation"));
		Tables.Boss->AddRow(TEXT("EXTRA"), Extra);
		TestEqual(TEXT("row count reason"), ResolveReason(Tables), EBossPatternDataFallbackReason::UnexpectedBossPatternRowCount);
	}
	{
		FCanonicalBossPatternTables Tables;
		Tables.Presets->FindRow<FCorruptedActinoPresetRow>(TEXT("ACTINO_LASER_04"), TEXT("Automation"))->LaserIndex = 3;
		TestEqual(TEXT("duplicate preset reason"), ResolveReason(Tables), EBossPatternDataFallbackReason::DuplicatePresetLaserIndex);
	}
	{
		FCanonicalBossPatternTables Tables;
		Tables.Boss->FindRow<FBossPatternSystemRow>(TEXT("BOSS_PATTERN_SYSTEM_001"), TEXT("Automation"))->PatternOrder = TEXT("2\u21921 Repeat");
		TestEqual(TEXT("pattern order contract"), ResolveReason(Tables), EBossPatternDataFallbackReason::InvalidBossPatternContract);
	}
	{
		FCanonicalBossPatternTables Tables;
		Tables.Corrupted->FindRow<FCorruptedActinoRow>(TEXT("PATTERN_001"), TEXT("Automation"))->FirstUseTelegraph = true;
		TestEqual(TEXT("Corrupted nonnumeric contract"), ResolveReason(Tables), EBossPatternDataFallbackReason::InvalidCorruptedContract);
	}
	{
		FCanonicalBossPatternTables Tables;
		Tables.Stellar->FindRow<FStellarRemnantRow>(TEXT("PATTERN_002"), TEXT("Automation"))->VisualDamage = 1;
		TestEqual(TEXT("Stellar nonnumeric contract"), ResolveReason(Tables), EBossPatternDataFallbackReason::InvalidStellarContract);
	}
	{
		FCanonicalBossPatternTables Tables;
		Tables.Stellar->FindRow<FStellarRemnantRow>(TEXT("PATTERN_002"), TEXT("Automation"))->Length = 41.0f;
		TestEqual(TEXT("Stellar range reason"), ResolveReason(Tables), EBossPatternDataFallbackReason::InvalidStellarRange);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBossPatternArenaBossSpawnTransformTest,
	"DroneProto.POR18.Arena.BossSpawnTransform",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBossPatternArenaBossSpawnTransformTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("BossPatternArenaBossSpawnTransformWorld")));
	TestNotNull(TEXT("test world is created"), World);
	if (!World)
	{
		return false;
	}

	const FVector ExpectedLocation(600.0f, 0.0f, 100.0f);
	ARaidBoss* Boss = World->SpawnActor<ARaidBoss>(
		ARaidBoss::StaticClass(),
		ExpectedLocation,
		FRotator::ZeroRotator);
	TestNotNull(TEXT("boss is spawned"), Boss);
	if (Boss)
	{
		TestTrue(TEXT("boss root preserves the requested spawn location"), Boss->GetActorLocation().Equals(ExpectedLocation, 0.01f));
	}

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBossPatternArenaPlayerStartAssignmentTest,
	"DroneProto.POR18.Arena.PlayerStartAssignment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBossPatternArenaPlayerStartAssignmentTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("BossPatternArenaPlayerStartAssignmentWorld")));
	TestNotNull(TEXT("test world is created"), World);
	if (!World)
	{
		return false;
	}

	ARaidGameMode* GameMode = World->SpawnActor<ARaidGameMode>();
	APlayerStart* FirstStart = World->SpawnActor<APlayerStart>(FVector(-720.0f, 170.0f, 192.0f), FRotator::ZeroRotator);
	APlayerStart* SecondStart = World->SpawnActor<APlayerStart>(FVector(-660.0f, -500.0f, 92.0f), FRotator::ZeroRotator);
	ARaidPlayerController* FirstController = World->SpawnActor<ARaidPlayerController>();
	ARaidPlayerController* SecondController = World->SpawnActor<ARaidPlayerController>();
	TestNotNull(TEXT("game mode is spawned"), GameMode);
	TestNotNull(TEXT("first player start is spawned"), FirstStart);
	TestNotNull(TEXT("second player start is spawned"), SecondStart);
	TestNotNull(TEXT("first controller is spawned"), FirstController);
	TestNotNull(TEXT("second controller is spawned"), SecondController);
	if (!GameMode || !FirstStart || !SecondStart || !FirstController || !SecondController)
	{
		World->DestroyWorld(false);
		return false;
	}

	FMath::RandInit(20260720);
	AActor* FirstAssignment = GameMode->ChoosePlayerStart(FirstController);
	FMath::RandInit(20260720);
	AActor* SecondAssignment = GameMode->ChoosePlayerStart(SecondController);
	AActor* FirstAssignmentAgain = GameMode->ChoosePlayerStart(FirstController);

	TestNotNull(TEXT("first controller receives a player start"), FirstAssignment);
	TestNotNull(TEXT("second controller receives a player start"), SecondAssignment);
	TestNotEqual(TEXT("controllers receive different player starts"), FirstAssignment, SecondAssignment);
	TestEqual(TEXT("the same controller keeps its player start"), FirstAssignmentAgain, FirstAssignment);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBossPatternArenaMapScaleContractTest,
	"DroneProto.POR18.Arena.MapScaleContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBossPatternArenaMapScaleContractTest::RunTest(const FString& Parameters)
{
	UWorld* ArenaWorld = LoadObject<UWorld>(nullptr, TEXT("/Game/TestMap.TestMap"));
	TestNotNull(TEXT("TestMap loads"), ArenaWorld);
	if (!ArenaWorld || !ArenaWorld->PersistentLevel)
	{
		return false;
	}

	const FVector2D BossXY(600.0f, 0.0f);
	TArray<APlayerStart*> PlayerStarts;
	AActor* Floor = nullptr;
	for (AActor* Actor : ArenaWorld->PersistentLevel->Actors)
	{
		if (APlayerStart* PlayerStart = Cast<APlayerStart>(Actor))
		{
			PlayerStarts.Add(PlayerStart);
		}
#if WITH_EDITOR
		else if (Actor && Actor->GetActorLabel() == TEXT("Floor"))
		{
			Floor = Actor;
		}
#endif
	}

	TestEqual(TEXT("TestMap has two PlayerStarts"), PlayerStarts.Num(), 2);
	if (PlayerStarts.Num() == 2)
	{
		TestNotEqual(TEXT("PlayerStarts are distinct actors"), PlayerStarts[0], PlayerStarts[1]);
		const FVector FirstLocation = PlayerStarts[0]->GetActorLocation();
		const FVector SecondLocation = PlayerStarts[1]->GetActorLocation();
		TestFalse(TEXT("PlayerStarts use different angles around the boss"),
			FVector2D(FirstLocation.X - BossXY.X, FirstLocation.Y - BossXY.Y).GetSafeNormal().Equals(
				FVector2D(SecondLocation.X - BossXY.X, SecondLocation.Y - BossXY.Y).GetSafeNormal(), 0.001f));
		for (int32 Index = 0; Index < PlayerStarts.Num(); ++Index)
		{
			const FVector Location = PlayerStarts[Index]->GetActorLocation();
			const float DistanceCm = FVector2D(Location.X - BossXY.X, Location.Y - BossXY.Y).Size();
			TestTrue(
				*FString::Printf(TEXT("PlayerStart %d is 10m to 12m from the boss"), Index + 1),
				DistanceCm >= 1000.0f && DistanceCm <= 1200.0f);
		}
	}

	TestNotNull(TEXT("TestMap Floor exists"), Floor);
	if (Floor)
	{
		FVector BoundsOrigin;
		FVector BoundsExtent;
		Floor->GetActorBounds(false, BoundsOrigin, BoundsExtent);
		TestTrue(TEXT("Floor center matches boss XY"), FVector2D(BoundsOrigin.X, BoundsOrigin.Y).Equals(BossXY, 1.0f));
		TestTrue(TEXT("Floor radius is 55m"),
			FMath::IsNearlyEqual(BoundsExtent.X, 5500.0f, 1.0f)
			&& FMath::IsNearlyEqual(BoundsExtent.Y, 5500.0f, 1.0f));
		TestTrue(TEXT("Floor radius exceeds the 50m MoveClamp"),
			BoundsExtent.X > 5000.0f && BoundsExtent.Y > 5000.0f);
	}

	FString DroneHeaderSource;
	const FString DroneHeaderPath = FPaths::ProjectDir() / TEXT("Source/DroneProto/Drone.h");
	TestTrue(TEXT("Drone header loads"), FFileHelper::LoadFileToString(DroneHeaderSource, *DroneHeaderPath));
	TestTrue(TEXT("MoveClamp remains 50m"),
		DroneHeaderSource.Contains(TEXT("float MovementBoundaryRadiusCm = 5000.0f;")));
	FString DroneSource;
	const FString DroneSourcePath = FPaths::ProjectDir() / TEXT("Source/DroneProto/Drone.cpp");
	TestTrue(TEXT("Drone source loads"), FFileHelper::LoadFileToString(DroneSource, *DroneSourcePath));
	TestTrue(TEXT("MoveClamp center remains the boss location"),
		DroneSource.Contains(TEXT("MovementBoundaryCenter = Boss->GetActorLocation();")));
	return true;
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
	TestEqual(TEXT("Stellar start radius"), Stellar.StartRadiusCm, 800.0f);
	TestEqual(TEXT("Stellar end radius"), Stellar.EndRadiusCm, 5000.0f);
	TestEqual(TEXT("Stellar length"), Stellar.LengthCm, 4200.0f);
	TestEqual(TEXT("Stellar travel time"), Stellar.TravelSeconds, 2.5f);
	TestEqual(TEXT("Stellar speed"), Stellar.SpeedCmPerSecond, 1680.0f);
	TestEqual(TEXT("Stellar collision radius"), Stellar.CollisionRadiusCm, 70.0f);
	TestEqual(TEXT("Stellar damage angle step"), Stellar.DamageAngleStepDegrees, 22.5f);
	TestEqual(TEXT("Stellar second-wave offset"), Stellar.SecondWaveOffsetDegrees, 11.25f);
	TestEqual(TEXT("Stellar visual Z offset"), Stellar.VisualZOffsetCm, 300.0f);
	TestEqual(TEXT("Stellar visual full-size minimum"), Stellar.VisualFullSizeMinCm, 100.0f);
	TestEqual(TEXT("Stellar visual full-size maximum"), Stellar.VisualFullSizeMaxCm, 120.0f);
	TestEqual(TEXT("visual-only damage"), Stellar.VisualDamage, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneBossPatternLifecycleSequenceTest,
	"DroneProto.BossPattern.Lifecycle.Sequence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneBossPatternLifecycleSequenceTest::RunTest(const FString& Parameters)
{
	FBossPatternPlayerTestContext Context = CreateBossPatternPlayerTestContext(TEXT("BossPatternLifecycleSequenceWorld"));
	UWorld* World = Context.World;
	ARaidBoss* Boss = Context.Boss;
	UBossPatternComponent* Component = Context.Component;
	TestNotNull(TEXT("world is created"), World);
	TestNotNull(TEXT("boss is spawned"), Boss);
	TestNotNull(TEXT("pattern component exists"), Component);
	if (!World || !Boss || !Component)
	{
		DestroyBossPatternPlayerTestContext(Context);
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
	DestroyBossPatternPlayerTestContext(Context);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneBossPatternLifecycleGuardsTest,
	"DroneProto.BossPattern.Lifecycle.Guards",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneBossPatternLifecycleGuardsTest::RunTest(const FString& Parameters)
{
	FBossPatternPlayerTestContext Context = CreateBossPatternPlayerTestContext(TEXT("BossPatternLifecycleGuardsWorld"));
	UWorld* World = Context.World;
	ARaidBoss* Boss = Context.Boss;
	UBossPatternComponent* Component = Context.Component;
	if (!World || !Boss || !Component)
	{
		TestTrue(TEXT("guard test setup"), false);
		DestroyBossPatternPlayerTestContext(Context);
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
	DestroyBossPatternPlayerTestContext(Context);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneBossPatternStableKeyDamageGateTest,
	"DroneProto.BossPattern.DamageGate.StableKeyAndActualDamage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneBossPatternStableKeyDamageGateTest::RunTest(const FString& Parameters)
{
	FBossPatternPlayerTestContext Context = CreateBossPatternPlayerTestContext(TEXT("BossPatternDamageGateWorld"));
	TestNotNull(TEXT("damage gate world is created"), Context.World);
	TestNotNull(TEXT("damage gate component exists"), Context.Component);
	TestNotNull(TEXT("damage gate player exists"), Context.PlayerController);
	TestNotNull(TEXT("damage gate drone exists"), Context.Drone);
	if (!Context.World || !Context.GameState || !Context.Boss || !Context.Component || !Context.PlayerController || !Context.Drone)
	{
		DestroyBossPatternPlayerTestContext(Context);
		return false;
	}

	const FString StableKey = ARaidGameMode::BuildStablePlayerKeyForServer(Context.PlayerController);
	TestFalse(TEXT("stable player key is not empty"), StableKey.IsEmpty());
	TestEqual(TEXT("stable player key is repeatable"), ARaidGameMode::BuildStablePlayerKeyForServer(Context.PlayerController), StableKey);

	TestTrue(TEXT("pattern starts"), Context.Boss->StartBossPatternForServer());
	TestTrue(TEXT("first Corrupted transition fires"), Context.Component->FireScheduledTransitionForTest());
	TestFalse(TEXT("null target is rejected"), Context.Component->TryApplyPatternDamageForServer(nullptr, 20));

	ARaidPlayerController* SelectingPlayer = Context.World->SpawnActor<ARaidPlayerController>();
	ADrone* SelectingDrone = Context.World->SpawnActor<ADrone>();
	TestNotNull(TEXT("Selecting player is spawned"), SelectingPlayer);
	TestNotNull(TEXT("Selecting drone is spawned"), SelectingDrone);
	if (SelectingPlayer && SelectingDrone)
	{
		SelectingPlayer->Possess(SelectingDrone);
		TestFalse(TEXT("non-InBattle target is rejected"), Context.Component->TryApplyPatternDamageForServer(SelectingDrone, 20));
	}
	Context.GameState->SetRaidStateForServer(ERaidState::Waiting);
	TestFalse(TEXT("non-Battle raid is rejected"), Context.Component->TryApplyPatternDamageForServer(Context.Drone, 20));
	Context.GameState->SetRaidStateForServer(ERaidState::Battle);

	TestTrue(TEXT("dodge starts"), Context.Drone->RequestDodgeForServer(FVector2D(1.0f, 0.0f)));
	TestTrue(TEXT("dodge invincibility is active"), Context.Drone->IsInvincibleForDamage());
	TestFalse(TEXT("dodge-invincible target is rejected"), Context.Component->TryApplyPatternDamageForServer(Context.Drone, 20));
	TestFalse(TEXT("repeated dodge-invincible target is rejected"), Context.Component->TryApplyPatternDamageForServer(Context.Drone, 20));
	TestEqual(TEXT("dodge ignore log is tracked once per invincibility window"),
		Context.Component->GetDodgeIgnoredLogKeyCountForTest(), 1);
	Context.Drone->CancelDodgeForServer(FName(TEXT("Automation")));

	const int32 HealthBeforeFirstHit = Context.Drone->GetHealth();
	TestTrue(TEXT("first valid hit succeeds"), Context.Component->TryApplyPatternDamageForServer(Context.Drone, 20));
	TestEqual(TEXT("first valid hit lowers HP"), Context.Drone->GetHealth(), HealthBeforeFirstHit - 20);
	TestFalse(TEXT("immediate repeated hit is locked"), Context.Component->TryApplyPatternDamageForServer(Context.Drone, 20));

	TestTrue(TEXT("Corrupted completes"), Context.Component->FireScheduledTransitionForTest());
	TestTrue(TEXT("Stellar telegraph starts"), Context.Component->FireScheduledTransitionForTest());
	TestTrue(TEXT("Stellar becomes active"), Context.Component->FireScheduledTransitionForTest());
	TestEqual(TEXT("pattern changed while lock remains"), Context.Component->GetCurrentPatternForTest(), EBossPatternKind::StellarRemnant);
	TestFalse(TEXT("cross-pattern repeated hit is locked"), Context.Component->TryApplyPatternDamageForServer(Context.Drone, 25));
	TestEqual(TEXT("HitLock ignore log is tracked once per lock window"),
		Context.Component->GetHitLockIgnoredLogKeyCountForTest(), 1);

	TickBossPatternTimers(Context.World, 0.701f);
	TestEqual(TEXT("expired HitLock clears its log suppression"),
		Context.Component->GetHitLockIgnoredLogKeyCountForTest(), 0);
	const int32 HealthBeforeExpiredHit = Context.Drone->GetHealth();
	TestTrue(TEXT("hit succeeds after 0.7 seconds"), Context.Component->TryApplyPatternDamageForServer(Context.Drone, 25));
	TestEqual(TEXT("expired lock hit lowers HP"), Context.Drone->GetHealth(), HealthBeforeExpiredHit - 25);

	TickBossPatternTimers(Context.World, 0.701f);
	TestFalse(TEXT("zero damage is rejected"), Context.Component->TryApplyPatternDamageForServer(Context.Drone, 0));
	TestTrue(TEXT("zero damage creates no lock"), Context.Component->TryApplyPatternDamageForServer(Context.Drone, 1));

	TickBossPatternTimers(Context.World, 0.701f);
	Context.Drone->ApplyDamageForServer(Context.Drone->GetMaxHealth() + 1, FName(TEXT("AutomationDeath")));
	TestFalse(TEXT("dead target is rejected"), Context.Component->TryApplyPatternDamageForServer(Context.Drone, 20));

	DestroyBossPatternPlayerTestContext(Context);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneBossPatternPopulationPauseRestartTest,
	"DroneProto.BossPattern.Population.PauseAndRestart",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneBossPatternPopulationPauseRestartTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("BossPatternPopulationWorld")));
	ARaidGameState* GameState = World ? World->SpawnActor<ARaidGameState>() : nullptr;
	ARaidGameMode* GameMode = World ? World->SpawnActor<ARaidGameMode>() : nullptr;
	ARaidBoss* Boss = World ? World->SpawnActor<ARaidBoss>() : nullptr;
	UBossPatternComponent* Component = Boss ? Boss->FindComponentByClass<UBossPatternComponent>() : nullptr;
	if (Component)
	{
		Component->ResolvePatternDataForTest();
	}
	TestNotNull(TEXT("population world is created"), World);
	TestNotNull(TEXT("population game state exists"), GameState);
	TestNotNull(TEXT("population game mode exists"), GameMode);
	TestNotNull(TEXT("population boss exists"), Boss);
	TestNotNull(TEXT("population component exists"), Component);
	if (!World || !GameState || !GameMode || !Boss || !Component)
	{
		if (World)
		{
			World->DestroyWorld(false);
		}
		return false;
	}

	World->SetGameState(GameState);
	GameState->SetRaidBossForServer(Boss);
	GameState->SetRaidStateForServer(ERaidState::Battle);
	GameState->SetRaidTimeEndServerTimeForServer(123.0f);
	TestTrue(TEXT("zero-player pattern start is accepted"), Boss->StartBossPatternForServer());
	TestEqual(TEXT("zero-player start pauses"), Component->GetServerStateForTest(), EBossPatternServerState::PausedNoPlayers);
	TestEqual(TEXT("zero-player active count"), Component->GetActivePlayerCountForTest(), 0);
	TestFalse(TEXT("zero-player pause clears transition timer"), Component->IsTransitionTimerActiveForTest());
	TestEqual(TEXT("zero-player pause has no actor"), CountPatternActors(World), 0);

	const FBossPatternTestPlayer FirstPlayer = SpawnBossPatternTestPlayer(World, true);
	TestNotNull(TEXT("first active player exists"), FirstPlayer.PlayerController);
	TestNotNull(TEXT("first active drone exists"), FirstPlayer.Drone);
	TestEqual(TEXT("0 to 1 restarts first delay"), Component->GetServerStateForTest(), EBossPatternServerState::FirstDelay);
	TestEqual(TEXT("restart recounts one player"), Component->GetActivePlayerCountForTest(), 1);
	TestEqual(TEXT("restart delay is canonical"), Component->GetPendingDelayForTest(), 0.5f);
	TestTrue(TEXT("restart schedules transition"), Component->IsTransitionTimerActiveForTest());
	TestTrue(TEXT("restart enters first Corrupted"), Component->FireScheduledTransitionForTest());

	const FBossPatternTestPlayer SecondPlayer = SpawnBossPatternTestPlayer(World, true);
	TestNotNull(TEXT("second active player exists"), SecondPlayer.PlayerController);
	TestNotNull(TEXT("second active drone exists"), SecondPlayer.Drone);
	TestEqual(TEXT("second player recounts two"), Component->GetActivePlayerCountForTest(), 2);
	TestTrue(TEXT("second player receives pattern damage"), Component->TryApplyPatternDamageForServer(SecondPlayer.Drone, 20));
	TestEqual(TEXT("successful hit creates one lock"), Component->GetHitLockCountForTest(), 1);

	const float BossHealthBeforeDeaths = Boss->GetCurrentHP();
	const EBossState BossStateBeforeDeaths = Boss->GetBossState();
	const float RaidEndTimeBeforeDeaths = GameState->GetRaidTimeEndServerTime();
	FirstPlayer.Drone->ApplyDamageForServer(FirstPlayer.Drone->GetMaxHealth() + 1, FName(TEXT("PopulationDeathOne")));
	TickBossPatternTimers(World, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("one of two deaths leaves one active"), Component->GetActivePlayerCountForTest(), 1);
	TestEqual(TEXT("one of two deaths keeps pattern active"), Component->GetServerStateForTest(), EBossPatternServerState::Active);
	TestTrue(TEXT("one of two deaths keeps transition timer"), Component->IsTransitionTimerActiveForTest());
	TestEqual(TEXT("one of two deaths keeps shared lock"), Component->GetHitLockCountForTest(), 1);

	SecondPlayer.Drone->ApplyDamageForServer(SecondPlayer.Drone->GetMaxHealth() + 1, FName(TEXT("PopulationDeathLast")));
	TickBossPatternTimers(World, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("last death pauses pattern"), Component->GetServerStateForTest(), EBossPatternServerState::PausedNoPlayers);
	TestEqual(TEXT("last death clears active count"), Component->GetActivePlayerCountForTest(), 0);
	TestFalse(TEXT("last death clears transition timer"), Component->IsTransitionTimerActiveForTest());
	TestEqual(TEXT("last death clears pattern actor"), CountPatternActors(World), 0);
	TestEqual(TEXT("last death clears all HitLocks"), Component->GetHitLockCountForTest(), 0);
	TestEqual(TEXT("pause resets next pattern to Corrupted"), Component->GetNextPatternForTest(), EBossPatternKind::CorruptedActino);
	TestEqual(TEXT("pause preserves boss HP"), Boss->GetCurrentHP(), BossHealthBeforeDeaths);
	TestEqual(TEXT("pause preserves boss state"), Boss->GetBossState(), BossStateBeforeDeaths);
	TestEqual(TEXT("pause preserves raid timer"), GameState->GetRaidTimeEndServerTime(), RaidEndTimeBeforeDeaths);

	const FBossPatternTestPlayer LogoutPlayer = SpawnBossPatternTestPlayer(World, true);
	TestEqual(TEXT("new active player restarts after death pause"), Component->GetServerStateForTest(), EBossPatternServerState::FirstDelay);
	GameMode->Logout(LogoutPlayer.PlayerController);
	LogoutPlayer.PlayerController->Destroy();
	TickBossPatternTimers(World, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("logout of last active player pauses"), Component->GetServerStateForTest(), EBossPatternServerState::PausedNoPlayers);
	TestEqual(TEXT("logout recounts zero"), Component->GetActivePlayerCountForTest(), 0);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneCorruptedActinoAnalyticGeometryTest,
	"DroneProto.BossPattern.CorruptedActino.AnalyticGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneCorruptedActinoAnalyticGeometryTest::RunTest(const FString& Parameters)
{
	const FCorruptedActinoConfig Config;
	const float ExpectedAnglesAtStart[4] = {-25.0f, 115.0f, 155.0f, 295.0f};
	const float ExpectedAnglesAtQuarterXY[4] = {0.0f, 90.0f, 180.0f, 270.0f};
	const float ExpectedZAtStart[4] = {300.0f, 0.0f, -300.0f, 0.0f};
	const float ExpectedZAtQuarterZ[4] = {0.0f, 300.0f, 0.0f, -300.0f};
	for (int32 Index = 0; Index < Config.LaserCount; ++Index)
	{
		TestTrue(*FString::Printf(TEXT("laser %d start angle"), Index),
			FMath::IsNearlyEqual(ACorruptedActinoPatternActor::EvaluateAngleDegrees(Config.Presets[Index], 0.0f), ExpectedAnglesAtStart[Index]));
		TestTrue(*FString::Printf(TEXT("laser %d representative angle"), Index),
			FMath::IsNearlyEqual(ACorruptedActinoPatternActor::EvaluateAngleDegrees(Config.Presets[Index], 1.25f), ExpectedAnglesAtQuarterXY[Index]));
		TestTrue(*FString::Printf(TEXT("laser %d start Z"), Index),
			FMath::IsNearlyEqual(ACorruptedActinoPatternActor::EvaluateZCm(Config.Presets[Index], 0.0f), ExpectedZAtStart[Index], 0.01f));
		TestTrue(*FString::Printf(TEXT("laser %d representative Z"), Index),
			FMath::IsNearlyEqual(ACorruptedActinoPatternActor::EvaluateZCm(Config.Presets[Index], 0.5f), ExpectedZAtQuarterZ[Index], 0.01f));
	}

	const FTransform BossTransform(FRotator(0.0f, 37.0f, 0.0f), FVector(1200.0f, -700.0f, 250.0f));
	const FCorruptedActinoLaserPreset& Preset = Config.Presets[0];
	auto IsInside = [&BossTransform, &Preset](float LongitudinalCm, float LateralCm, float VerticalOffsetCm)
	{
		return ACorruptedActinoPatternActor::IsPointInsideLaser(
			MakeCorruptedLaserPoint(BossTransform, Preset, 0.0f, LongitudinalCm, LateralCm, VerticalOffsetCm),
			BossTransform,
			Preset,
			0.0f);
	};

	TestTrue(TEXT("inner longitudinal boundary is inclusive"), IsInside(0.0f, 0.0f, 0.0f));
	TestTrue(TEXT("outer longitudinal boundary is inclusive"), IsInside(4200.0f, 0.0f, 0.0f));
	TestFalse(TEXT("before inner boundary is excluded"), IsInside(-0.1f, 0.0f, 0.0f));
	TestFalse(TEXT("after outer boundary is excluded"), IsInside(4200.1f, 0.0f, 0.0f));
	TestTrue(TEXT("inner collision half-width is inclusive"), IsInside(0.0f, 200.0f, 0.0f));
	TestFalse(TEXT("inner visual-only width does not damage"), IsInside(0.0f, 200.1f, 0.0f));
	TestTrue(TEXT("outer collision half-width is inclusive"), IsInside(4200.0f, 400.0f, 0.0f));
	TestFalse(TEXT("outside outer collision width is excluded"), IsInside(4200.0f, 400.1f, 0.0f));
	TestTrue(TEXT("collision half-height is inclusive"), IsInside(2100.0f, 0.0f, 75.0f));
	TestFalse(TEXT("outside collision height is excluded"), IsInside(2100.0f, 0.0f, 75.1f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneCorruptedActinoRuntimeDamageTest,
	"DroneProto.BossPattern.CorruptedActino.RuntimeDamageAndLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneCorruptedActinoRuntimeDamageTest::RunTest(const FString& Parameters)
{
	FBossPatternPlayerTestContext Context = CreateBossPatternPlayerTestContext(TEXT("CorruptedActinoRuntimeWorld"));
	if (!Context.World || !Context.Boss || !Context.Component || !Context.Drone)
	{
		TestTrue(TEXT("runtime test setup"), false);
		DestroyBossPatternPlayerTestContext(Context);
		return false;
	}

	const FVector BossStartLocation(900.0f, -450.0f, 125.0f);
	const FRotator BossStartRotation(0.0f, 31.0f, 0.0f);
	Context.Boss->SetActorLocationAndRotation(BossStartLocation, BossStartRotation);
	TestTrue(TEXT("pattern starts"), Context.Boss->StartBossPatternForServer());
	TestTrue(TEXT("first Corrupted starts"), Context.Component->FireScheduledTransitionForTest());
	ACorruptedActinoPatternActor* CorruptedActor = Cast<ACorruptedActinoPatternActor>(Context.Component->GetActivePatternActorForTest());
	TestNotNull(TEXT("Corrupted uses dedicated actor"), CorruptedActor);
	TestEqual(TEXT("first use skips telegraph"), Context.Component->GetServerStateForTest(), EBossPatternServerState::Active);
	if (!CorruptedActor)
	{
		DestroyBossPatternPlayerTestContext(Context);
		return false;
	}
	TestTrue(TEXT("actor snapshots Boss start location"), CorruptedActor->GetActorLocation().Equals(BossStartLocation, 0.01f));
	TestTrue(TEXT("actor snapshots Boss start rotation"), CorruptedActor->GetActorRotation().Equals(BossStartRotation, 0.01f));

	const FCorruptedActinoConfig Config;
	Context.Boss->SetActorLocation(BossStartLocation + FVector(2000.0f, 1000.0f, 0.0f));
	Context.Drone->SetActorLocation(MakeCorruptedLaserPoint(
		CorruptedActor->GetActorTransform(), Config.Presets[0], 0.0f, 1000.0f, 0.0f, 0.0f));
	const int32 HealthBefore = Context.Drone->GetHealth();
	CorruptedActor->Tick(0.0f);
	TestEqual(TEXT("active laser deals canonical damage"), Context.Drone->GetHealth(), HealthBefore - 20);
	TestEqual(TEXT("each Drone receives at most one damage attempt per tick"), CorruptedActor->GetDamageAttemptCountForTest(), 1);

	TestTrue(TEXT("first Corrupted completes"), Context.Component->FireScheduledTransitionForTest());
	TestTrue(TEXT("Stellar telegraph starts"), Context.Component->FireScheduledTransitionForTest());
	TestTrue(TEXT("Stellar starts"), Context.Component->FireScheduledTransitionForTest());
	TestTrue(TEXT("Stellar completes"), Context.Component->FireScheduledTransitionForTest());
	TestTrue(TEXT("later Corrupted telegraph starts"), Context.Component->FireScheduledTransitionForTest());
	TestEqual(TEXT("later Corrupted telegraph is one second"), Context.Component->GetPendingDelayForTest(), 1.0f);
	TestNotNull(TEXT("later Corrupted also uses dedicated actor"),
		Cast<ACorruptedActinoPatternActor>(Context.Component->GetActivePatternActorForTest()));

	Context.Boss->StopBossPatternForServer(FName(TEXT("Automation")));
	DestroyBossPatternPlayerTestContext(Context);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneCorruptedActinoDebugVisualizationContractTest,
	"DroneProto.BossPattern.CorruptedActino.DebugVisualizationContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneCorruptedActinoDebugVisualizationContractTest::RunTest(const FString& Parameters)
{
	const FString ActorPath = FPaths::ProjectDir() / TEXT("Source/DroneProto/Raid/CorruptedActinoPatternActor.cpp");
	FString ActorSource;
	TestTrue(TEXT("Corrupted actor source loads"), FFileHelper::LoadFileToString(ActorSource, *ActorPath));
	TestTrue(TEXT("visual timing uses server world time"), ActorSource.Contains(TEXT("GetServerWorldTimeSeconds")));
	TestTrue(TEXT("dedicated server skips debug drawing"), ActorSource.Contains(TEXT("NM_DedicatedServer")));
	TestTrue(TEXT("telegraph is a translucent yellow sector"),
		ActorSource.Contains(TEXT("FColor(255, 196, 0, 96)")));
	TestTrue(TEXT("active attack is a translucent red sector"),
		ActorSource.Contains(TEXT("FColor(255, 32, 32, 112)")));
	TestTrue(TEXT("Corrupted uses one filled trapezoid helper"),
		ActorSource.Contains(TEXT("DrawFilledTrapezoid(")));
	TestTrue(TEXT("Corrupted uses native debug mesh"), ActorSource.Contains(TEXT("DrawDebugMesh(")));
	TestFalse(TEXT("collision debug boundary is removed"), ActorSource.Contains(TEXT("FColor::Cyan")));
	TestFalse(TEXT("visual debug boundary is removed"), ActorSource.Contains(TEXT("FColor::Magenta")));
	TestFalse(TEXT("Corrupted no longer draws dashed line bundles"),
		ActorSource.Contains(TEXT("DrawDashedDebugLine(")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneBossPatternForegroundVisibilityContractTest,
	"DroneProto.BossPattern.Visual.ForegroundVisibilityContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneBossPatternForegroundVisibilityContractTest::RunTest(const FString& Parameters)
{
	const FString BasePath = FPaths::ProjectDir() / TEXT("Source/DroneProto/Raid/BossPatternActorBase.cpp");
	FString BaseSource;
	TestTrue(TEXT("pattern actor base source loads"), FFileHelper::LoadFileToString(BaseSource, *BasePath));
	TestTrue(TEXT("dashes are visually separated"), BaseSource.Contains(TEXT("DebugDashLengthCm = 100.0f")));
	TestTrue(TEXT("dash gaps stay visible from raid camera"), BaseSource.Contains(TEXT("DebugDashGapCm = 300.0f")));
	TestTrue(TEXT("debug primitives keep a short trail"), BaseSource.Contains(TEXT("DebugPrimitiveLifetimeSeconds = 0.12f")));
	TestTrue(TEXT("debug primitives draw in foreground"), BaseSource.Contains(TEXT("DebugForegroundDepthPriority = 1")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneStellarRemnantLogicalSamplesTest,
	"DroneProto.BossPattern.StellarRemnant.LogicalSamples",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneStellarRemnantLogicalSamplesTest::RunTest(const FString& Parameters)
{
	const FStellarRemnantConfig Config;
	const TArray<FStellarRemnantSample> Samples = AStellarRemnantPatternActor::BuildLogicalSamples();
	TestEqual(TEXT("total logical samples"), Samples.Num(), 48);

	int32 DamageCount = 0;
	int32 VisualOnlyCount = 0;
	int32 DamageIndexByWave[2] = {0, 0};
	int32 VisualIndexByWave[2] = {0, 0};
	int32 VisualAboveByWave[2] = {0, 0};
	int32 VisualBelowByWave[2] = {0, 0};
	bool bSawVisualMinSize = false;
	bool bSawVisualMaxSize = false;
	for (const FStellarRemnantSample& Sample : Samples)
	{
		TestTrue(TEXT("sample wave is valid"), Sample.WaveIndex == 0 || Sample.WaveIndex == 1);
		const float ExpectedStartTime = Sample.WaveIndex == 0 ? 0.0f : 0.5f;
		TestEqual(TEXT("sample uses canonical wave start"), Sample.StartTimeSeconds, ExpectedStartTime);
		if (Sample.bVisualOnly)
		{
			++VisualOnlyCount;
			++VisualIndexByWave[Sample.WaveIndex];
			TestEqual(TEXT("visual-only damage is zero"), Sample.Damage, 0);
			TestTrue(TEXT("visual-only Z uses canonical offset"),
				FMath::IsNearlyEqual(FMath::Abs(Sample.VisualZOffsetCm), Config.VisualZOffsetCm));
			TestTrue(TEXT("visual-only full size stays in canonical range"),
				Sample.VisualFullSizeCm >= Config.VisualFullSizeMinCm
				&& Sample.VisualFullSizeCm <= Config.VisualFullSizeMaxCm);
			VisualAboveByWave[Sample.WaveIndex] += Sample.VisualZOffsetCm > 0.0f ? 1 : 0;
			VisualBelowByWave[Sample.WaveIndex] += Sample.VisualZOffsetCm < 0.0f ? 1 : 0;
			bSawVisualMinSize |= FMath::IsNearlyEqual(Sample.VisualFullSizeCm, Config.VisualFullSizeMinCm);
			bSawVisualMaxSize |= FMath::IsNearlyEqual(Sample.VisualFullSizeCm, Config.VisualFullSizeMaxCm);
			TestTrue(TEXT("visual-only evaluated position preserves Z offset"),
				FMath::IsNearlyEqual(
					AStellarRemnantPatternActor::EvaluateLocalPosition(Sample, Sample.StartTimeSeconds).Z,
					Sample.VisualZOffsetCm));
		}
		else
		{
			const int32 DamageIndex = DamageIndexByWave[Sample.WaveIndex]++;
			const float ExpectedAngle = DamageIndex * 22.5f + (Sample.WaveIndex == 0 ? 0.0f : 11.25f);
			TestEqual(TEXT("damage sample angle"), Sample.AngleDegrees, ExpectedAngle);
			TestEqual(TEXT("damage sample uses canonical damage"), Sample.Damage, 25);
			++DamageCount;
		}
	}
	TestEqual(TEXT("damage sample count"), DamageCount, Config.DamageProjectileCount);
	TestEqual(TEXT("visual-only sample count"), VisualOnlyCount, Config.VisualProjectileCount);
	TestEqual(TEXT("wave one damage count"), DamageIndexByWave[0], Config.DamageProjectilesPerWave);
	TestEqual(TEXT("wave two damage count"), DamageIndexByWave[1], Config.DamageProjectilesPerWave);
	TestEqual(TEXT("wave one visual-only count"), VisualIndexByWave[0], Config.VisualProjectilesPerWave);
	TestEqual(TEXT("wave two visual-only count"), VisualIndexByWave[1], Config.VisualProjectilesPerWave);
	TestEqual(TEXT("wave one visual-only above count"), VisualAboveByWave[0], 4);
	TestEqual(TEXT("wave one visual-only below count"), VisualBelowByWave[0], 4);
	TestEqual(TEXT("wave two visual-only above count"), VisualAboveByWave[1], 4);
	TestEqual(TEXT("wave two visual-only below count"), VisualBelowByWave[1], 4);
	TestTrue(TEXT("visual-only samples include minimum size"), bSawVisualMinSize);
	TestTrue(TEXT("visual-only samples include maximum size"), bSawVisualMaxSize);

	const FStellarRemnantSample& FirstDamageSample = Samples[0];
	TestTrue(TEXT("wave one is active at t0"), AStellarRemnantPatternActor::IsSampleActive(FirstDamageSample, 0.0f));
	TestTrue(TEXT("wave one is active through 2.5 seconds"), AStellarRemnantPatternActor::IsSampleActive(FirstDamageSample, 2.5f));
	TestFalse(TEXT("wave one ends after 2.5 seconds"), AStellarRemnantPatternActor::IsSampleActive(FirstDamageSample, 2.501f));
	TestTrue(TEXT("start position is 800cm"),
		AStellarRemnantPatternActor::EvaluateLocalPosition(FirstDamageSample, 0.0f).Equals(FVector(800.0f, 0.0f, 0.0f), 0.01f));
	TestTrue(TEXT("one-second position uses 1680cm per second"),
		AStellarRemnantPatternActor::EvaluateLocalPosition(FirstDamageSample, 1.0f).Equals(FVector(2480.0f, 0.0f, 0.0f), 0.01f));
	TestTrue(TEXT("flight ends at 5000cm"),
		AStellarRemnantPatternActor::EvaluateLocalPosition(FirstDamageSample, 2.5f).Equals(FVector(5000.0f, 0.0f, 0.0f), 0.01f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneStellarRemnantSweptCollisionTest,
	"DroneProto.BossPattern.StellarRemnant.SweptCollision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneStellarRemnantSweptCollisionTest::RunTest(const FString& Parameters)
{
	const TArray<FStellarRemnantSample> Samples = AStellarRemnantPatternActor::BuildLogicalSamples();
	const FStellarRemnantSample& WaveOneSample = Samples[0];
	const FTransform BossTransform(FRotator(0.0f, 37.0f, 0.0f), FVector(900.0f, -350.0f, 125.0f));
	auto WorldPoint = [&BossTransform](float RadialCm, float LateralCm)
	{
		return BossTransform.TransformPosition(FVector(RadialCm, LateralCm, 0.0f));
	};

	TestTrue(TEXT("sweep hits between frame endpoints"), AStellarRemnantPatternActor::IsPointInsideSweptSample(
		WorldPoint(1640.0f, 0.0f), BossTransform, WaveOneSample, 0.0f, 1.0f));
	TestTrue(TEXT("collision radius 70cm is inclusive"), AStellarRemnantPatternActor::IsPointInsideSweptSample(
		WorldPoint(1640.0f, 70.0f), BossTransform, WaveOneSample, 0.0f, 1.0f));
	TestFalse(TEXT("outside collision radius is excluded"), AStellarRemnantPatternActor::IsPointInsideSweptSample(
		WorldPoint(1640.0f, 70.1f), BossTransform, WaveOneSample, 0.0f, 1.0f));
	TestTrue(TEXT("target hit radius includes visible edge contact"), AStellarRemnantPatternActor::IsPointInsideSweptSample(
		WorldPoint(1640.0f, 120.0f), BossTransform, WaveOneSample, 0.0f, 1.0f, FStellarRemnantConfig(), 50.0f));
	TestFalse(TEXT("outside combined projectile and target radii is excluded"), AStellarRemnantPatternActor::IsPointInsideSweptSample(
		WorldPoint(1640.0f, 120.1f), BossTransform, WaveOneSample, 0.0f, 1.0f, FStellarRemnantConfig(), 50.0f));

	const FStellarRemnantSample* WaveTwoSample = Samples.FindByPredicate([](const FStellarRemnantSample& Sample)
	{
		return !Sample.bVisualOnly && Sample.WaveIndex == 1;
	});
	TestNotNull(TEXT("wave two damage sample exists"), WaveTwoSample);
	if (WaveTwoSample)
	{
		TestFalse(TEXT("wave two cannot collide before 0.5 seconds"), AStellarRemnantPatternActor::IsPointInsideSweptSample(
			BossTransform.TransformPosition(AStellarRemnantPatternActor::EvaluateLocalPosition(*WaveTwoSample, 0.5f)),
			BossTransform,
			*WaveTwoSample,
			0.0f,
			0.49f));
	}
	const FStellarRemnantSample* VisualOnlySample = Samples.FindByPredicate([](const FStellarRemnantSample& Sample)
	{
		return Sample.bVisualOnly;
	});
	TestNotNull(TEXT("visual-only sample exists"), VisualOnlySample);
	if (VisualOnlySample)
	{
		TestFalse(TEXT("visual-only sample never collides"), AStellarRemnantPatternActor::IsPointInsideSweptSample(
			BossTransform.TransformPosition(AStellarRemnantPatternActor::EvaluateLocalPosition(*VisualOnlySample, 1.0f)),
			BossTransform,
			*VisualOnlySample,
			0.0f,
			1.0f));
	}
	TestFalse(TEXT("finished sample no longer collides"), AStellarRemnantPatternActor::IsPointInsideSweptSample(
		WorldPoint(5000.0f, 0.0f), BossTransform, WaveOneSample, 2.501f, 3.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneStellarRemnantRuntimeDamageTest,
	"DroneProto.BossPattern.StellarRemnant.RuntimeDamageAndPiercing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneStellarRemnantRuntimeDamageTest::RunTest(const FString& Parameters)
{
	FBossPatternPlayerTestContext Context = CreateBossPatternPlayerTestContext(TEXT("StellarRemnantRuntimeWorld"));
	if (!Context.World || !Context.Boss || !Context.Component || !Context.Drone)
	{
		TestTrue(TEXT("runtime test setup"), false);
		DestroyBossPatternPlayerTestContext(Context);
		return false;
	}

	TestTrue(TEXT("pattern starts"), Context.Boss->StartBossPatternForServer());
	TestTrue(TEXT("first Corrupted starts"), Context.Component->FireScheduledTransitionForTest());
	TestTrue(TEXT("first Corrupted completes"), Context.Component->FireScheduledTransitionForTest());
	TestTrue(TEXT("Stellar telegraph starts"), Context.Component->FireScheduledTransitionForTest());
	AStellarRemnantPatternActor* StellarActor = Cast<AStellarRemnantPatternActor>(Context.Component->GetActivePatternActorForTest());
	TestNotNull(TEXT("Stellar uses dedicated actor"), StellarActor);
	TestEqual(TEXT("Stellar telegraph is 0.8 seconds"), Context.Component->GetPendingDelayForTest(), 0.8f);
	if (!StellarActor)
	{
		DestroyBossPatternPlayerTestContext(Context);
		return false;
	}

	Context.Drone->SetActorLocation(StellarActor->GetActorTransform().TransformPosition(FVector(800.0f, 0.0f, 0.0f)));
	const int32 TelegraphHealth = Context.Drone->GetHealth();
	StellarActor->Tick(0.0f);
	TestEqual(TEXT("telegraph deals no damage"), Context.Drone->GetHealth(), TelegraphHealth);
	TestTrue(TEXT("Stellar becomes active"), Context.Component->FireScheduledTransitionForTest());

	const FBossPatternTestPlayer SecondPlayer = SpawnBossPatternTestPlayer(Context.World, true);
	TestNotNull(TEXT("second piercing target exists"), SecondPlayer.Drone);
	const FVector SweptHitPoint = StellarActor->GetActorTransform().TransformPosition(FVector(1640.0f, 0.0f, 0.0f));
	Context.Drone->SetActorLocation(SweptHitPoint);
	if (SecondPlayer.Drone)
	{
		SecondPlayer.Drone->SetActorLocation(SweptHitPoint);
	}
	const int32 FirstHealthBefore = Context.Drone->GetHealth();
	const int32 SecondHealthBefore = SecondPlayer.Drone ? SecondPlayer.Drone->GetHealth() : 0;
	StellarActor->ApplyDamageForServerForTest(0.0f, 1.0f);
	TestEqual(TEXT("first target receives canonical damage"), Context.Drone->GetHealth(), FirstHealthBefore - 25);
	if (SecondPlayer.Drone)
	{
		TestEqual(TEXT("same sample pierces second target"), SecondPlayer.Drone->GetHealth(), SecondHealthBefore - 25);
	}
	TestEqual(TEXT("hits do not consume logical samples"), StellarActor->GetLogicalSampleCountForTest(), 48);

	const TArray<FStellarRemnantSample> HitLockSamples = AStellarRemnantPatternActor::BuildLogicalSamples();
	const FStellarRemnantSample* WaveTwoDamageSample = HitLockSamples.FindByPredicate(
		[](const FStellarRemnantSample& Sample)
		{
			return !Sample.bVisualOnly && Sample.WaveIndex == 1;
		});
	TestNotNull(TEXT("wave two damage sample exists for HitLock regression"), WaveTwoDamageSample);
	if (WaveTwoDamageSample)
	{
		if (SecondPlayer.Drone)
		{
			SecondPlayer.Drone->SetActorLocation(FVector(100000.0f, 100000.0f, 100000.0f));
		}

		TickBossPatternTimers(Context.World, 0.5f);
		Context.Drone->SetActorLocation(StellarActor->GetActorTransform().TransformPosition(
			AStellarRemnantPatternActor::EvaluateLocalPosition(*WaveTwoDamageSample, 1.5f)));
		const int32 HealthDuringHitLock = Context.Drone->GetHealth();
		StellarActor->ApplyDamageForServerForTest(1.0f, 1.5f);
		TestEqual(TEXT("wave two collision inside 0.7 seconds is blocked"),
			Context.Drone->GetHealth(), HealthDuringHitLock);

		TickBossPatternTimers(Context.World, 0.201f);
		Context.Drone->SetActorLocation(StellarActor->GetActorTransform().TransformPosition(
			AStellarRemnantPatternActor::EvaluateLocalPosition(*WaveTwoDamageSample, 1.701f)));
		StellarActor->ApplyDamageForServerForTest(1.5f, 1.701f);
		TestEqual(TEXT("collision after 0.7 seconds can damage again"),
			Context.Drone->GetHealth(), HealthDuringHitLock - 25);
	}

	Context.Boss->StopBossPatternForServer(FName(TEXT("Automation")));
	DestroyBossPatternPlayerTestContext(Context);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneStellarRemnantDebugVisualizationContractTest,
	"DroneProto.BossPattern.StellarRemnant.DebugVisualizationContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneStellarRemnantDebugVisualizationContractTest::RunTest(const FString& Parameters)
{
	const FString ActorPath = FPaths::ProjectDir() / TEXT("Source/DroneProto/Raid/StellarRemnantPatternActor.cpp");
	FString ActorSource;
	TestTrue(TEXT("Stellar actor source loads"), FFileHelper::LoadFileToString(ActorSource, *ActorPath));
	TestTrue(TEXT("visual timing uses server world time"), ActorSource.Contains(TEXT("GetServerWorldTimeSeconds")));
	TestTrue(TEXT("dedicated server skips debug drawing"), ActorSource.Contains(TEXT("NM_DedicatedServer")));
	TestTrue(TEXT("telegraph rays are yellow"), ActorSource.Contains(TEXT("FColor::Yellow")));
	TestTrue(TEXT("damage samples are red"), ActorSource.Contains(TEXT("FColor::Red")));
	TestTrue(TEXT("visual-only samples are purple"), ActorSource.Contains(TEXT("FColor::Purple")));
	TestTrue(TEXT("Stellar telegraph rays are dashed"), ActorSource.Contains(TEXT("DrawDashedDebugLine(")));
	TestFalse(TEXT("Stellar actor has no direct solid debug lines"), ActorSource.Contains(TEXT("DrawDebugLine(")));
	TestTrue(TEXT("Stellar telegraph rays are highly visible"),
		ActorSource.Contains(TEXT("FColor::Yellow, 8.0f")));
	TestTrue(TEXT("Stellar active samples use foreground spheres"),
		ActorSource.Contains(TEXT("DrawForegroundDebugSphere(")));
	TestFalse(TEXT("Stellar actor has no world-depth debug spheres"),
		ActorSource.Contains(TEXT("DrawDebugSphere(")));
	TestFalse(TEXT("Stellar actor does not spawn projectile actors"), ActorSource.Contains(TEXT("SpawnActor")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneBossPatternIntegrationLoopPopulationTest,
	"DroneProto.BossPattern.Integration.LoopPopulationAndPreservation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneBossPatternIntegrationLoopPopulationTest::RunTest(const FString& Parameters)
{
	FBossPatternPlayerTestContext Context = CreateBossPatternPlayerTestContext(TEXT("BossPatternIntegrationLoopWorld"));
	ARaidGameMode* GameMode = Context.World ? Context.World->SpawnActor<ARaidGameMode>() : nullptr;
	if (!Context.World || !Context.GameState || !Context.Boss || !Context.Component
		|| !Context.PlayerController || !Context.Drone || !GameMode)
	{
		TestTrue(TEXT("integration loop setup"), false);
		DestroyBossPatternPlayerTestContext(Context);
		return false;
	}

	Context.GameState->SetRaidTimeEndServerTimeForServer(123.0f);
	const FString ContributionKey = ARaidGameMode::BuildStablePlayerKeyForServer(Context.PlayerController);
	TestTrue(TEXT("contribution setup succeeds"), GameMode->RecordBossDamageForServer(Context.PlayerController, 12.5f));
	TestTrue(TEXT("pattern starts"), Context.Boss->StartBossPatternForServer());
	TestTrue(TEXT("C starts"), Context.Component->FireScheduledTransitionForTest());
	ABossPatternActorBase* CorruptedActor = Context.Component->GetActivePatternActorForTest();
	TestNotNull(TEXT("C owns an actor"), Cast<ACorruptedActinoPatternActor>(CorruptedActor));
	TestEqual(TEXT("C owns exactly one actor"), CountPatternActors(Context.World), 1);
	TestTrue(TEXT("C completes"), Context.Component->FireScheduledTransitionForTest());
	TestTrue(TEXT("S telegraph starts"), Context.Component->FireScheduledTransitionForTest());
	ABossPatternActorBase* StellarActor = Context.Component->GetActivePatternActorForTest();
	TestNotNull(TEXT("S owns a Stellar actor"), Cast<AStellarRemnantPatternActor>(StellarActor));
	TestTrue(TEXT("S becomes active"), Context.Component->FireScheduledTransitionForTest());
	TestEqual(TEXT("S reuses its telegraph actor"), Context.Component->GetActivePatternActorForTest(), StellarActor);
	TestTrue(TEXT("S completes"), Context.Component->FireScheduledTransitionForTest());
	TestTrue(TEXT("C telegraph starts again"), Context.Component->FireScheduledTransitionForTest());
	TestNotNull(TEXT("loop returns to Corrupted actor"),
		Cast<ACorruptedActinoPatternActor>(Context.Component->GetActivePatternActorForTest()));

	const FBossPatternTestPlayer SecondPlayer = SpawnBossPatternTestPlayer(Context.World, true);
	TestNotNull(TEXT("second player enters Battle"), SecondPlayer.Drone);
	TestTrue(TEXT("looped C becomes active"), Context.Component->FireScheduledTransitionForTest());
	ABossPatternActorBase* ActiveActorBeforeDeath = Context.Component->GetActivePatternActorForTest();
	const float BossHealthBefore = Context.Boss->GetCurrentHP();
	const EBossState BossStateBefore = Context.Boss->GetBossState();
	const float RaidEndTimeBefore = Context.GameState->GetRaidTimeEndServerTime();
	const float ContributionBefore = GameMode->GetBossDamageForPlayerKeyForServer(ContributionKey);

	Context.Drone->ApplyDamageForServer(Context.Drone->GetMaxHealth() + 1, FName(TEXT("IntegrationFirstDeath")));
	TickBossPatternTimers(Context.World, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("one of two deaths keeps active state"), Context.Component->GetServerStateForTest(), EBossPatternServerState::Active);
	TestEqual(TEXT("one of two deaths keeps current actor"), Context.Component->GetActivePatternActorForTest(), ActiveActorBeforeDeath);
	TestEqual(TEXT("one of two deaths keeps one actor"), CountPatternActors(Context.World), 1);

	const int32 StaleSerial = Context.Component->GetTransitionSerialForTest();
	if (SecondPlayer.Drone)
	{
		SecondPlayer.Drone->ApplyDamageForServer(SecondPlayer.Drone->GetMaxHealth() + 1, FName(TEXT("IntegrationLastDeath")));
	}
	TickBossPatternTimers(Context.World, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("last death pauses"), Context.Component->GetServerStateForTest(), EBossPatternServerState::PausedNoPlayers);
	TestEqual(TEXT("last death clears actor"), CountPatternActors(Context.World), 0);
	TestFalse(TEXT("last death clears timer"), Context.Component->IsTransitionTimerActiveForTest());
	TestEqual(TEXT("pause preserves Boss HP"), Context.Boss->GetCurrentHP(), BossHealthBefore);
	TestEqual(TEXT("pause preserves Boss state"), Context.Boss->GetBossState(), BossStateBefore);
	TestEqual(TEXT("pause preserves raid end time"), Context.GameState->GetRaidTimeEndServerTime(), RaidEndTimeBefore);
	TestEqual(TEXT("pause preserves contribution"), GameMode->GetBossDamageForPlayerKeyForServer(ContributionKey), ContributionBefore);

	const FBossPatternTestPlayer RejoinedPlayer = SpawnBossPatternTestPlayer(Context.World, true);
	TestNotNull(TEXT("new InBattle player exists"), RejoinedPlayer.Drone);
	TestEqual(TEXT("rejoin schedules first delay"), Context.Component->GetServerStateForTest(), EBossPatternServerState::FirstDelay);
	TestFalse(TEXT("pre-pause callback remains stale"), Context.Component->FireTransitionForTest(StaleSerial));
	TestEqual(TEXT("stale callback cannot skip restart delay"), Context.Component->GetServerStateForTest(), EBossPatternServerState::FirstDelay);
	TestTrue(TEXT("current callback restarts Corrupted"), Context.Component->FireScheduledTransitionForTest());
	TestNotNull(TEXT("restart uses Corrupted actor"),
		Cast<ACorruptedActinoPatternActor>(Context.Component->GetActivePatternActorForTest()));

	Context.Boss->StopBossPatternForServer(FName(TEXT("Automation")));
	DestroyBossPatternPlayerTestContext(Context);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneBossPatternIntegrationTerminalCleanupTest,
	"DroneProto.BossPattern.Integration.TerminalCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneBossPatternIntegrationTerminalCleanupTest::RunTest(const FString& Parameters)
{
	FBossPatternPlayerTestContext BossDead = CreateBossPatternPlayerTestContext(TEXT("BossPatternIntegrationBossDeadWorld"));
	if (!BossDead.World || !BossDead.Boss || !BossDead.Component || !BossDead.Drone)
	{
		TestTrue(TEXT("BossDead setup"), false);
		DestroyBossPatternPlayerTestContext(BossDead);
		return false;
	}
	TestTrue(TEXT("BossDead pattern starts"), BossDead.Boss->StartBossPatternForServer());
	TestTrue(TEXT("BossDead active pattern starts"), BossDead.Component->FireScheduledTransitionForTest());
	TestTrue(TEXT("BossDead creates a HitLock before cleanup"), BossDead.Component->TryApplyPatternDamageForServer(BossDead.Drone, 1));
	const int32 BossDeadStaleSerial = BossDead.Component->GetTransitionSerialForTest();
	BossDead.Boss->ApplyDamageForServer(BossDead.Boss->GetMaxHP() + 1.0f, BossDead.PlayerController, BossDead.Drone);
	TestEqual(TEXT("BossDead stops component"), BossDead.Component->GetServerStateForTest(), EBossPatternServerState::Stopped);
	TestEqual(TEXT("BossDead clears actor"), CountPatternActors(BossDead.World), 0);
	TestFalse(TEXT("BossDead clears timer"), BossDead.Component->IsTransitionTimerActiveForTest());
	TestEqual(TEXT("BossDead clears HitLock"), BossDead.Component->GetHitLockCountForTest(), 0);
	TestFalse(TEXT("BossDead rejects stale callback"), BossDead.Component->FireTransitionForTest(BossDeadStaleSerial));
	TestFalse(TEXT("BossDead rejects direct pattern damage"), BossDead.Component->TryApplyPatternDamageForServer(BossDead.Drone, 20));
	DestroyBossPatternPlayerTestContext(BossDead);

	FBossPatternPlayerTestContext TimeOver = CreateBossPatternPlayerTestContext(TEXT("BossPatternIntegrationTimeOverWorld"));
	ARaidGameMode* TimeOverGameMode = TimeOver.World ? TimeOver.World->SpawnActor<ARaidGameMode>() : nullptr;
	ADronePartInventory* Inventory = TimeOver.World ? TimeOver.World->SpawnActor<ADronePartInventory>() : nullptr;
	if (!TimeOver.World || !TimeOver.GameState || !TimeOver.Boss || !TimeOver.Component
		|| !TimeOverGameMode || !Inventory)
	{
		TestTrue(TEXT("TimeOver setup"), false);
		DestroyBossPatternPlayerTestContext(TimeOver);
		return false;
	}
	TimeOver.GameState->SetDronePartInventory(Inventory);
	TimeOver.World->AddController(TimeOver.PlayerController);
	TestTrue(TEXT("TimeOver pattern starts"), TimeOver.Boss->StartBossPatternForServer());
	TestTrue(TEXT("TimeOver active pattern starts"), TimeOver.Component->FireScheduledTransitionForTest());
	TimeOverGameMode->ExpireRaidTimeLimitForTest();
	TestEqual(TEXT("TimeOver enters RaidEnd"), TimeOver.GameState->RaidState, ERaidState::End);
	TestEqual(TEXT("TimeOver stops component"), TimeOver.Component->GetServerStateForTest(), EBossPatternServerState::Stopped);
	TestEqual(TEXT("TimeOver clears actor"), CountPatternActors(TimeOver.World), 0);
	TestFalse(TEXT("TimeOver rejects pattern damage"), TimeOver.Component->TryApplyPatternDamageForServer(TimeOver.Drone, 20));
	DestroyBossPatternPlayerTestContext(TimeOver);

	FBossPatternPlayerTestContext DestroyedBoss = CreateBossPatternPlayerTestContext(TEXT("BossPatternIntegrationDestroyedWorld"));
	if (!DestroyedBoss.World || !DestroyedBoss.Boss || !DestroyedBoss.Component)
	{
		TestTrue(TEXT("Destroyed setup"), false);
		DestroyBossPatternPlayerTestContext(DestroyedBoss);
		return false;
	}
	TestTrue(TEXT("Destroyed pattern starts"), DestroyedBoss.Boss->StartBossPatternForServer());
	TestTrue(TEXT("Destroyed active pattern starts"), DestroyedBoss.Component->FireScheduledTransitionForTest());
	TestEqual(TEXT("Destroyed setup owns actor"), CountPatternActors(DestroyedBoss.World), 1);
	DestroyedBoss.Boss->Destroy();
	TestEqual(TEXT("Boss Destroyed clears pattern actor"), CountPatternActors(DestroyedBoss.World), 0);
	TestFalse(TEXT("Boss Destroyed clears transition timer"), DestroyedBoss.Component->IsTransitionTimerActiveForTest());
	DestroyBossPatternPlayerTestContext(DestroyedBoss);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneBossPatternIntegrationMarkerContractTest,
	"DroneProto.BossPattern.Integration.RequiredMarkers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDroneBossPatternIntegrationMarkerContractTest::RunTest(const FString& Parameters)
{
	const FString ComponentPath = FPaths::ProjectDir() / TEXT("Source/DroneProto/Raid/BossPatternComponent.cpp");
	FString ComponentSource;
	TestTrue(TEXT("component source loads"), FFileHelper::LoadFileToString(ComponentSource, *ComponentPath));
	const TCHAR* RequiredMarkers[] =
	{
		TEXT("BossPattern State="),
		TEXT("Spawn Pattern="),
		TEXT("BossPattern Hit Pattern="),
		TEXT("HitIgnored Reason=DodgeInvincible"),
		TEXT("HitIgnored Reason=PatternHitLock"),
		TEXT("Pause Reason=NoAlivePlayers"),
		TEXT("Restart Reason=AlivePlayerJoined"),
		TEXT("Cleanup Reason=")
	};
	for (const TCHAR* Marker : RequiredMarkers)
	{
		TestTrue(*FString::Printf(TEXT("required marker exists: %s"), Marker), ComponentSource.Contains(Marker));
	}
	return true;
}

#endif
