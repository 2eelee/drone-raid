# POP-04 Raid Entry Spawns Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make every raid entrant spawn at one of four boss-relative 35m PlayerStarts chosen randomly with replacement, and notify the existing boss-pattern population path only after Pawn creation succeeds.

**Architecture:** `TestMap` owns the four explicit `RaidEntrySpawn` PlayerStarts. `ARaidGameMode` filters to exactly those four, caches one random choice per controller, permits different controllers to share a point, and emits the spawn-completed population notification after `RestartPlayer`. `UBossPatternComponent` remains the sole authority for deciding whether a recount is a `0 → 1` empty-raid restart.

**Tech Stack:** Unreal Engine 5.7 C++, Automation Tests, binary `.umap` edited through Unreal Editor, PowerShell, Git LFS.

## Global Constraints

- Keep the UE 5.7 Dedicated Server flow server-authoritative.
- Use the existing `RestartPlayer`, `SpawnDefaultPawnAtTransform`, `NotifyPatternPopulationChangedForServer`, and population recount paths; do not create a parallel spawn or pattern system.
- The only raid entry candidates are four `APlayerStart` actors tagged `RaidEntrySpawn` at 35m and 90-degree spacing around the world-origin boss.
- Different controllers may select the same point; repeated lookup for one controller must remain stable until Logout.
- Do not add entry invulnerability. Existing post-hit 0.7s HitLock remains unchanged.
- Do not change A → B → C server assignment, reservation tokens, capacity, tutorial, UMG, Blueprint classes, or boss attack behavior.
- Maintain UTF-8 source, append-only `docs/DEVLOG.md`, and only the affected `POP-04` implementation-map row plus current tracking.
- Do not push, open a PR, or move any Linear issue without explicit user approval.

## File Structure

- Modify `Source/DroneProto/Raid/RaidGameMode.cpp`: tagged random-with-replacement selection and successful spawn-completion notification.
- Modify `Source/DroneProto/Tests/BossPatternTests.cpp`: direct POP-04 selection, spawn-completion, and loaded-map contracts; retain POR-18 floor/boss scale coverage.
- Modify `Content/TestMap.umap`: replace the two existing PlayerStarts with four tagged cardinal PlayerStarts.
- Modify `docs/Audit/ImplementationMap_Current.md`: current work tracking before implementation, then the verified `POP-04` row at closeout.
- Modify `docs/DEVLOG.md`: append only the commands, discovered counts, and results actually observed.
- Modify `AGENTS.md`: replace stale current-state/next-step text with the verified POP-04 result and remaining PIE boundary.
- Do not modify `Source/DroneProto/Raid/RaidGameMode.h`: no new public or private interface is needed.

---

### Task 1: Random-With-Replacement Entry Selection and Spawn Handoff

**Files:**
- Modify: `docs/Audit/ImplementationMap_Current.md:7-22`
- Modify: `Source/DroneProto/Tests/BossPatternTests.cpp:636-680`
- Modify: `Source/DroneProto/Tests/BossPatternTests.cpp:1157-1245`
- Modify: `Source/DroneProto/Raid/RaidGameMode.cpp:218-283`

**Interfaces:**
- Consumes: `APlayerStart::ActorHasTag(FName)`, `ARaidGameState::GetRaidBoss()`, `ARaidBoss::NotifyPatternPopulationChangedForServer(FName)`.
- Produces: no new API; `ChoosePlayerStart_Implementation` gains the tagged random-with-replacement contract and `RestartPlayer` emits `PlayerSpawnCompleted` after a valid `ADrone` exists.

- [ ] **Step 1: Record active implementation tracking before code edits**

Replace the current-work block in `ImplementationMap_Current.md` with the exact live values:

```markdown
- 작업 브랜치: `codex/pop-04-entry-spawns`
- 시작 HEAD: `0fad6e8`
- current HEAD: `0fad6e8`
- 작업 목적: 초기·후입장 모두 보스 중심 35m의 지정 PlayerStart 4개 중 하나를 랜덤 선택하고 중복 선택을 허용하며, Pawn 생성 완료 뒤 빈 레이드 0→1일 때만 기존 패턴 재시작 경로를 사용한다.
- 수정 예정 파일: `RaidGameMode.cpp`, `BossPatternTests.cpp`, `Content/TestMap.umap`, 현행 지도·DEVLOG·AGENTS
- 영향받는 명세 ID: `POP-04`
- 실행 경로: server join → tagged PlayerStart random selection → existing RestartPlayer/Pawn spawn → spawn-completed population recount → existing 0→1 guard → 0.5초 Corrupted restart
```

Do not change any other implementation-status row at this step.

- [ ] **Step 2: Replace the old distinct-assignment test with a failing POP-04 selection test**

Use four tagged starts plus one untagged decoy. Reset the global RNG to the same seed before each controller selection so both controllers must receive the same start, proving replacement semantics without a probabilistic assertion:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPop04RaidEntryRandomWithReplacementTest,
	"DroneProto.POP04.Entry.RandomWithReplacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPop04RaidEntryRandomWithReplacementTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("Pop04RandomEntryWorld")));
	ARaidGameMode* GameMode = World ? World->SpawnActor<ARaidGameMode>() : nullptr;
	TArray<APlayerStart*> TaggedStarts;
	for (int32 Index = 0; World && Index < 4; ++Index)
	{
		APlayerStart* Start = World->SpawnActor<APlayerStart>(
			FVector(3500.0f, 0.0f, 92.0f).RotateAngleAxis(90.0f * Index, FVector::UpVector),
			FRotator::ZeroRotator);
		Start->Tags.Add(FName(TEXT("RaidEntrySpawn")));
		TaggedStarts.Add(Start);
	}
	APlayerStart* UntaggedStart = World ? World->SpawnActor<APlayerStart>(FVector::ZeroVector, FRotator::ZeroRotator) : nullptr;
	ARaidPlayerController* FirstController = World ? World->SpawnActor<ARaidPlayerController>() : nullptr;
	ARaidPlayerController* SecondController = World ? World->SpawnActor<ARaidPlayerController>() : nullptr;

	FMath::RandInit(20260812);
	AActor* FirstAssignment = GameMode ? GameMode->ChoosePlayerStart(FirstController) : nullptr;
	FMath::RandInit(20260812);
	AActor* SecondAssignment = GameMode ? GameMode->ChoosePlayerStart(SecondController) : nullptr;
	AActor* FirstAssignmentAgain = GameMode ? GameMode->ChoosePlayerStart(FirstController) : nullptr;

	TestNotNull(TEXT("first controller receives a tagged start"), FirstAssignment);
	TestTrue(TEXT("selected start carries RaidEntrySpawn"), FirstAssignment && FirstAssignment->ActorHasTag(FName(TEXT("RaidEntrySpawn"))));
	TestNotEqual(TEXT("untagged start is excluded"), FirstAssignment, UntaggedStart);
	TestEqual(TEXT("same seed permits duplicate assignment across controllers"), SecondAssignment, FirstAssignment);
	TestEqual(TEXT("same controller keeps its assignment"), FirstAssignmentAgain, FirstAssignment);
	World->DestroyWorld(false);
	return true;
}
```

- [ ] **Step 3: Add a failing spawn-completion integration test**

Create a Battle world with a running pattern paused at zero players, an `InBattle` controller without a Pawn, and four tagged starts. Call the real `RestartPlayer` and assert that successful Pawn creation causes the existing component to recount and enter first delay:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPop04SpawnCompletionRestartsEmptyRaidTest,
	"DroneProto.POP04.Entry.SpawnCompletionRestartsEmptyRaid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPop04SpawnCompletionRestartsEmptyRaidTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("Pop04SpawnCompletionWorld")));
	ARaidGameState* GameState = World ? World->SpawnActor<ARaidGameState>() : nullptr;
	ARaidGameMode* GameMode = World ? World->SpawnActor<ARaidGameMode>() : nullptr;
	ARaidBoss* Boss = World ? World->SpawnActor<ARaidBoss>() : nullptr;
	UBossPatternComponent* Component = Boss ? Boss->FindComponentByClass<UBossPatternComponent>() : nullptr;
	if (!World || !GameState || !GameMode || !Boss || !Component)
	{
		if (World)
		{
			World->DestroyWorld(false);
		}
		return false;
	}

	World->SetGameState(GameState);
	GameMode->DefaultPawnClass = ADrone::StaticClass();
	GameState->SetRaidBossForServer(Boss);
	GameState->SetRaidStateForServer(ERaidState::Battle);
	Component->ResolvePatternDataForTest();
	TestTrue(TEXT("zero-player pattern start succeeds"), Boss->StartBossPatternForServer());
	TestEqual(TEXT("pattern waits while raid is empty"), Component->GetServerStateForTest(), EBossPatternServerState::PausedNoPlayers);

	for (int32 Index = 0; Index < 4; ++Index)
	{
		APlayerStart* Start = World->SpawnActor<APlayerStart>(
			FVector(3500.0f, 0.0f, 92.0f).RotateAngleAxis(90.0f * Index, FVector::UpVector),
			FRotator::ZeroRotator);
		Start->Tags.Add(FName(TEXT("RaidEntrySpawn")));
	}
	ARaidPlayerController* Controller = World->SpawnActor<ARaidPlayerController>();
	Controller->SetPlayerSelectionStateForServer(EPlayerSelectionState::InBattle);
	TestNull(TEXT("controller has no Pawn before RestartPlayer"), Controller->GetPawn());
	TestEqual(TEXT("selection alone cannot restart without a Pawn"), Component->GetServerStateForTest(), EBossPatternServerState::PausedNoPlayers);

	GameMode->RestartPlayer(Controller);
	TestTrue(TEXT("RestartPlayer creates an ADrone"), Controller->GetPawn() && Controller->GetPawn()->IsA<ADrone>());
	TestEqual(TEXT("spawn completion recounts one active player"), Component->GetActivePlayerCountForTest(), 1);
	TestEqual(TEXT("spawn completion enters first delay"), Component->GetServerStateForTest(), EBossPatternServerState::FirstDelay);
	TestEqual(TEXT("restart delay remains canonical"), Component->GetPendingDelayForTest(), 0.5f);
	TestTrue(TEXT("spawn completion schedules Corrupted restart"), Component->IsTransitionTimerActiveForTest());

	World->DestroyWorld(false);
	return true;
}
```

Use the same explicit world cleanup pattern as `FDroneBossPatternPopulationPauseRestartTest`; do not add a test-only production method.

- [ ] **Step 4: Build and run the two tests to prove RED**

Run:

```powershell
& 'D:\Programs\UE_5.7\Engine\Build\BatchFiles\Build.bat' DroneProtoEditor Win64 Development -Project='D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject' -NoLiveCoding -WaitMutex
& 'D:\Programs\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject' -unattended -nop4 -nosplash -NullRHI "-ExecCmds=Automation RunTests DroneProto.POP04.Entry; Quit" "-TestExit=Automation Test Queue Empty" -log
```

Expected RED:

- `RandomWithReplacement` fails because the current code excludes already-claimed starts and includes the untagged decoy.
- `SpawnCompletionRestartsEmptyRaid` fails because current `RestartPlayer` does not notify the population path after Pawn creation.

- [ ] **Step 5: Implement tagged random selection with replacement**

Inside the existing unnamed namespace at the top of `RaidGameMode.cpp`, add the internal constants:

```cpp
namespace
{
const FName RaidEntrySpawnTag(TEXT("RaidEntrySpawn"));
constexpr int32 RequiredRaidEntrySpawnCount = 4;
}
```

Preserve the existing controller cache, remove the `ClaimedStarts` exclusion, collect only tagged starts, sort by name, validate exactly four, and choose one index:

```cpp
TArray<APlayerStart*> Starts;
for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
{
	APlayerStart* Start = *It;
	if (Start && Start->ActorHasTag(RaidEntrySpawnTag))
	{
		Starts.Add(Start);
	}
}
Starts.Sort([](const APlayerStart& Left, const APlayerStart& Right)
{
	return Left.GetFName().LexicalLess(Right.GetFName());
});

if (Starts.Num() == RequiredRaidEntrySpawnCount)
{
	APlayerStart* SelectedStart = Starts[FMath::RandRange(0, Starts.Num() - 1)];
	PlayerStartAssignments.Add(Player, SelectedStart);
	return SelectedStart;
}

UE_LOG(LogTemp, Error,
	TEXT("[DR_SUMMARY] RaidEntrySpawn Result=Fallback Reason=InvalidTaggedStartCount Count=%d Required=%d Player=%s"),
	Starts.Num(), RequiredRaidEntrySpawnCount, *BuildRaidGameModeControllerLogString(Player));
```

Keep the existing `Super::ChoosePlayerStart_Implementation` fallback and cache its returned actor. Keep `Logout` assignment removal unchanged.

- [ ] **Step 6: Notify population only after successful drone spawn**

After the existing `RestartPlayer` spawn log, add:

```cpp
if (HasAuthority() && Pawn && Pawn->IsA<ADrone>())
{
	if (ARaidGameState* GameState = GetGameState<ARaidGameState>())
	{
		if (ARaidBoss* Boss = GameState->GetRaidBoss())
		{
			Boss->NotifyPatternPopulationChangedForServer(FName(TEXT("PlayerSpawnCompleted")));
		}
	}
}
```

Do not notify when `Pawn` is null; the existing spawn-failure path remains authoritative.

- [ ] **Step 7: Rebuild and prove GREEN**

Run the same Editor build and `DroneProto.POP04.Entry` filter. Expected: 2/2 pass. Then run:

```powershell
& 'D:\Programs\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject' -unattended -nop4 -nosplash -NullRHI "-ExecCmds=Automation RunTests DroneProto.BossPattern.Population.PauseAndRestart; Quit" "-TestExit=Automation Test Queue Empty" -log
```

Expected: existing population test 1/1 passes.

- [ ] **Step 8: Review and commit Task 1**

```powershell
git diff --check
git diff -- Source/DroneProto/Raid/RaidGameMode.cpp Source/DroneProto/Tests/BossPatternTests.cpp
git add -- Source/DroneProto/Raid/RaidGameMode.cpp Source/DroneProto/Tests/BossPatternTests.cpp
git commit -m "feat: POP-04 입장 스폰 선택과 완료 전달 구현"
```

Do not stage the ignored implementation map yet; it remains the live local tracking surface until closeout.

---

### Task 2: Four Tagged TestMap PlayerStarts

**Files:**
- Modify: `Source/DroneProto/Tests/BossPatternTests.cpp:682-742`
- Modify: `Content/TestMap.umap`

**Interfaces:**
- Consumes: the `RaidEntrySpawn` tag and exact four-candidate requirement implemented in Task 1.
- Produces: a saved map with exactly four tagged PlayerStarts at the approved transforms.

- [ ] **Step 1: Separate the old POR-18 floor contract from the new POP-04 map contract**

Keep `DroneProto.POR18.Arena.MapScaleContract` assertions for Floor center/radius and other POR-18 map-scale behavior. Remove its old two-PlayerStart coordinates and add this separate test:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPop04ArenaFourTaggedStartsTest,
	"DroneProto.POP04.Arena.FourTaggedStarts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
```

Load `/Game/TestMap.TestMap`, collect every `APlayerStart`, and assert:

```cpp
const TArray<FVector> ExpectedLocations = {
	FVector(3500.0f, 0.0f, 92.0f),
	FVector(0.0f, 3500.0f, 92.0f),
	FVector(-3500.0f, 0.0f, 92.0f),
	FVector(0.0f, -3500.0f, 92.0f),
};
TestEqual(TEXT("TestMap has exactly four PlayerStarts"), PlayerStarts.Num(), 4);
```

For every start, require the tag, one matching expected location within `0.01cm`, XY distance `3500cm`, and a forward vector facing the origin within dot product `0.999`.

- [ ] **Step 2: Build and run the map contract to prove RED**

Run the Editor build, then:

```powershell
& 'D:\Programs\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject' -unattended -nop4 -nosplash -NullRHI "-ExecCmds=Automation RunTests DroneProto.POP04.Arena; Quit" "-TestExit=Automation Test Queue Empty" -log
```

Expected RED: the current map has two untagged starts at approximately 10-12m, not four tagged starts at 35m.

- [ ] **Step 3: Edit TestMap through Unreal Editor**

Open `Content/TestMap.umap` in UE 5.7. In the World Outliner, delete only the two existing `PlayerStart` actors. Add four standard `PlayerStart` actors with these exact Details values:

| Label | Location | Rotation | Actor Tags |
|---|---|---|---|
| `RaidEntrySpawn_East` | `X=3500 Y=0 Z=92` | `Pitch=0 Yaw=180 Roll=0` | `RaidEntrySpawn` |
| `RaidEntrySpawn_North` | `X=0 Y=3500 Z=92` | `Pitch=0 Yaw=-90 Roll=0` | `RaidEntrySpawn` |
| `RaidEntrySpawn_West` | `X=-3500 Y=0 Z=92` | `Pitch=0 Yaw=0 Roll=0` | `RaidEntrySpawn` |
| `RaidEntrySpawn_South` | `X=0 Y=-3500 Z=92` | `Pitch=0 Yaw=90 Roll=0` | `RaidEntrySpawn` |

Save only `TestMap`. Do not move the boss, Floor, boundary, or any other actor.

- [ ] **Step 4: Prove the saved map contract GREEN**

Run `DroneProto.POP04.Arena` again. Expected: 1/1 passes. Then run:

```powershell
& 'D:\Programs\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject' -unattended -nop4 -nosplash -NullRHI "-ExecCmds=Automation RunTests DroneProto.POR18.Arena; Quit" "-TestExit=Automation Test Queue Empty" -log
```

Expected: remaining POR-18 arena contracts pass with no boss/Floor regression.

- [ ] **Step 5: Inspect the binary scope and commit Task 2**

```powershell
git status --short
git diff --stat -- Content/TestMap.umap Source/DroneProto/Tests/BossPatternTests.cpp
git lfs ls-files | rg "Content/TestMap.umap"
git add -- Content/TestMap.umap Source/DroneProto/Tests/BossPatternTests.cpp
git commit -m "feat: POP-04 스폰 포인트 4개 배치"
```

Confirm no unrelated `.uasset`, `.umap`, autosave, or config file is staged.

---

### Task 3: Final Verification and Canonical Closeout

**Files:**
- Modify: `docs/Audit/ImplementationMap_Current.md:7-22,331`
- Modify: `docs/DEVLOG.md` append-only
- Modify: `AGENTS.md` current state and next step only

**Interfaces:**
- Consumes: the two verified implementation commits and their exact test logs/counts.
- Produces: canonical POP-04 implementation status and a clean Linear-ready handoff; no Linear state mutation.

- [ ] **Step 1: Run final targeted verification**

```powershell
& 'D:\Programs\UE_5.7\Engine\Build\BatchFiles\Build.bat' DroneProtoEditor Win64 Development -Project='D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject' -NoLiveCoding -WaitMutex
& 'D:\Programs\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject' -unattended -nop4 -nosplash -NullRHI "-ExecCmds=Automation RunTests DroneProto.POP04; Quit" "-TestExit=Automation Test Queue Empty" -log
& 'D:\Programs\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject' -unattended -nop4 -nosplash -NullRHI "-ExecCmds=Automation RunTests DroneProto.BossPattern.Population.PauseAndRestart; Quit" "-TestExit=Automation Test Queue Empty" -log
& 'D:\Programs\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject' -unattended -nop4 -nosplash -NullRHI "-ExecCmds=Automation RunTests DroneProto.POR18.Arena; Quit" "-TestExit=Automation Test Queue Empty" -log
```

Expected discovered counts after this plan: POP-04 3/3, population 1/1, POR-18 Arena 2/2. Record actual discovered counts if Unreal reports different values.

- [ ] **Step 2: Run the required full regression**

Because `ARaidGameMode` and connection/spawn lifecycle changed, run:

```powershell
& 'D:\Programs\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject' -unattended -nop4 -nosplash -NullRHI "-ExecCmds=Automation RunTests DroneProto; Quit" "-TestExit=Automation Test Queue Empty" -log
```

Expected baseline after adding two net-new tests: 141/141. Use the actual discovered total as the canonical result.

- [ ] **Step 3: Perform the proportional GUI PIE boundary check**

In UE 5.7 PIE, verify at minimum:

1. Initial raid entrants appear at the four 35m cardinal points, not the removed near-boss starts.
2. Repeated entries can visibly reuse a point without spawn failure.
3. After the last living combatant dies or leaves during Battle, one new completed entrant restarts Corrupted after 0.5s.
4. Joining while another combatant survives does not reset the active pattern.
5. No entry protection timer or invulnerability indicator appears.

If multi-client PIE is not run, record each item as `미검증` rather than inferring it from automation.

- [ ] **Step 4: Update canonical documentation from observed evidence**

In `ImplementationMap_Current.md`:

- Update current tracking with the actual current HEAD, changed files, call path, authority boundary, test commands/counts, and PIE status.
- Change only `POP-04` from `Missing` to `Complete` if the direct POP-04 automation passed in this change.
- Cite `RaidGameMode.cpp`, `BossPatternComponent.cpp`, `BossPatternTests.cpp`, and `Content/TestMap.umap` as appropriate.
- Preserve `Unknown`/`미검증` for every PIE boundary not actually run.

Append one dated `docs/DEVLOG.md` entry containing the two RED failures, GREEN counts, Editor Build result, full regression result, map asset change, and manual PIE boundary.

Update `AGENTS.md` current state with the branch/commit/result and remove `POP-04` as a remaining Missing candidate. Do not rewrite unrelated historical bullets.

- [ ] **Step 5: Run closeout checks**

```powershell
git diff --check
git status --short --branch
git diff -- Source/DroneProto/Raid/RaidGameMode.cpp Source/DroneProto/Tests/BossPatternTests.cpp AGENTS.md
git diff --stat -- Content/TestMap.umap
git diff --no-index NUL docs/Audit/ImplementationMap_Current.md
git diff --no-index NUL docs/DEVLOG.md
```

For ignored docs, compare against the versions at the start of the task or use explicit forced staging review; do not rely on `git status` alone.

- [ ] **Step 6: Commit only canonical closeout documents**

```powershell
git add -- AGENTS.md
git add -f -- docs/Audit/ImplementationMap_Current.md docs/DEVLOG.md
git diff --cached --check
git diff --cached --stat
git commit -m "docs: POP-04 입장 스폰 검증 기록"
```

End with `git status --short --branch` and `git rev-parse --short HEAD`. Leave merge/push and any issue state change to the user.
