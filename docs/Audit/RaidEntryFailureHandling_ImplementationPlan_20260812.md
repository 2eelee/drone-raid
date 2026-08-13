# Raid Entry Failure Handling Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 매칭 10초 실패와 예약 성공 뒤 맵 로드 10초 초과·travel 실패를 중복 없이 판정하고 로비의 기존 실패 UI로 복구한다.

**Architecture:** GameInstance 수명의 `URaidSessionSubsystem`이 load watchdog과 engine failure delegate를 소유한다. travel 시작 world와 endpoint를 snapshot하고, 새 world post-load·failure delegate·경과시간 중 먼저 도착한 사건이 pending 요청을 한 번만 완료한다.

**Tech Stack:** Unreal Engine 5.7 C++, `UGameInstanceSubsystem`, `FTSTicker`, engine travel/network failure delegates, Unreal Automation Tests.

## Global Constraints

- Dedicated Server 기준 server-authoritative 구조와 기존 예약·token·spawn 실패 경로를 유지한다.
- UMG, Blueprint, `.uasset`, `.umap`, VFX는 수정하지 않는다.
- 새 동작은 직접 관련 자동화의 RED → GREEN으로 검증한다.
- `ENTRY-06`, `ENTRY-10`과 실제 영향받은 문서 행만 갱신한다.
- 사용자 요청 없이 push, PR, Linear 상태 변경을 하지 않는다.

---

## File Structure

- `Source/DroneProto/Lobby/RaidSessionSubsystem.h`: load-watchdog 상태, lifecycle callback, 테스트 seam 선언.
- `Source/DroneProto/Lobby/RaidSessionSubsystem.cpp`: delegate 등록/해제, watchdog 시작·성공·실패·로비 복구 구현.
- `Source/DroneProto/Tests/DronePartInventoryTests.cpp`: 매칭 timeout 회귀와 load-watchdog RED/GREEN 계약.
- `docs/Audit/ImplementationMap_Current.md`: `ENTRY-06`, `ENTRY-10` 구현·검증 근거와 남은 PIE 경계 갱신.
- `docs/DEVLOG.md`: 검증된 구현 이력 append.
- `AGENTS.md`: 현재 상태와 다음 단계 갱신.

### Task 1: Load-watchdog 자동화 계약 추가

**Files:**
- Modify: `Source/DroneProto/Tests/DronePartInventoryTests.cpp:596`

**Interfaces:**
- Consumes: `RequestRaidEntry`, `SetSuppressTravelForTest`, `GetLastAssignmentResultForTest`.
- Produces: `IsRaidLoadWatchdogActiveForTest()`, `CompleteRaidLoadForTest()`, `ExpireRaidLoadWatchdogForTest()`, `NotifyRaidTravelFailureForTest()`, `GetRaidLoadFailureHandleCountForTest()`.

- [ ] **Step 1: 존재하지 않는 테스트 seam을 요구하는 실패 테스트 작성**

```cpp
bool IsRaidLoadWatchdogActiveForTest() const { return bRaidLoadWatchdogActive; }
int32 GetRaidLoadFailureHandleCountForTest() const { return RaidLoadFailureHandleCountForTest; }
void CompleteRaidLoadForTest();
void ExpireRaidLoadWatchdogForTest();
void NotifyRaidTravelFailureForTest();
```

`DroneProto.RaidEntry.Session.LoadWatchdogLifecycle`에서 성공 assignment가 watchdog을 시작하는지, 성공 완료 뒤 timeout이 무효인지, timeout과 travel failure가 중복 처리되지 않는지, 최종 reason이 `MapLoadFailed`인지 확인한다. 기존 `WaitRetryTimeoutCancel`은 `NoServerAvailable`과 retry 종료를 계속 확인한다.

- [ ] **Step 2: 직접 테스트를 실행해 RED 확인**

```powershell
& 'D:\Programs\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject' -unattended -nop4 -nosplash -NullRHI "-ExecCmds=Automation RunTests DroneProto.RaidEntry.Session; Quit" "-TestExit=Automation Test Queue Empty" -log
```

Expected: 새 테스트 seam이 아직 없어 compile 실패.

- [ ] **Step 3: 테스트 변경만 검토**

Run: `git diff --check -- Source/DroneProto/Tests/DronePartInventoryTests.cpp`

### Task 2: GameInstance 수명 load watchdog 구현

**Files:**
- Modify: `Source/DroneProto/Lobby/RaidSessionSubsystem.h:22-153`
- Modify: `Source/DroneProto/Lobby/RaidSessionSubsystem.cpp:45-555`
- Test: `Source/DroneProto/Tests/DronePartInventoryTests.cpp`

**Interfaces:**
- Consumes: `FCoreUObjectDelegates::PostLoadMapWithWorld`, `GEngine->OnTravelFailure()`, `GEngine->OnNetworkFailure()`, `FTSTicker`.
- Produces: `StartRaidLoadWatchdog`, `StopRaidLoadWatchdog`, `HandlePostLoadMap`, `HandleRaidLoadWatchdogTick`, `HandlePendingRaidLoadFailure`, `PresentPendingRaidLoadFailure`.

- [ ] **Step 1: subsystem lifecycle과 pending 상태 추가**

`Initialize`에서 post-load와 engine failure delegate를 등록하고 `Deinitialize`에서 delegate·ticker를 해제한다. 상태는 source world, endpoint, 시작 monotonic time, active/handled/pending-popup/return-request guard로 한정한다.

- [ ] **Step 2: travel 직전 watchdog 시작**

실제 world와 필요한 local PlayerController를 검증한 뒤 다음 순서로 실행한다.

```cpp
StartRaidLoadWatchdog(World, Result.Endpoint);
PC->ClientTravel(Result.Endpoint.TravelTarget, TRAVEL_Absolute);
```

level-name `OpenLevel`도 동일하게 시작한다. 자동화 travel suppression은 실제 호출만 생략하고 watchdog 시작은 유지한다.

- [ ] **Step 3: 성공·timeout·engine failure 단일 완료 구현**

새 GameInstance world의 post-load가 10초 이내면 성공으로 해제한다. post-load 시 이미 10초를 넘겼거나 ticker/delegate 실패가 먼저 도착하면 `HandlePendingRaidLoadFailure`가 active 상태를 먼저 해제하고 아래 결과를 한 번만 기록한다.

```cpp
FRaidAssignmentResult::Failed(
    ERaidEntryFailReason::MapLoadFailed,
    DebugReason,
    PendingRaidLoadEndpoint)
```

- [ ] **Step 4: 로비 복귀 후 native fallback 표시**

현재 map이 `LobbyMap`이면 즉시 `ShowLoadFailed()`를 호출한다. 다른 map이면 pending popup을 보존하고 `OpenLevel(LobbyMap)`을 한 번만 호출한다. `SetActiveLobbyWidget` 또는 LobbyMap post-load가 실제 widget 준비 시 pending popup을 소비한다.

- [ ] **Step 5: 직접 테스트 GREEN 확인**

Task 1의 `DroneProto.RaidEntry.Session` 명령을 재실행한다.

Expected: 새 lifecycle 테스트와 기존 wait/retry/cancel 테스트 전부 통과.

- [ ] **Step 6: 구현 커밋**

```powershell
git add -- Source/DroneProto/Lobby/RaidSessionSubsystem.h Source/DroneProto/Lobby/RaidSessionSubsystem.cpp Source/DroneProto/Tests/DronePartInventoryTests.cpp
git commit -m "fix: 레이드 입장 로드 실패 복구 추가"
```

### Task 3: 영향 검증과 canonical 문서 갱신

**Files:**
- Modify: `docs/Audit/ImplementationMap_Current.md:5,180,184`
- Modify: `docs/DEVLOG.md`
- Modify: `AGENTS.md`

**Interfaces:**
- Consumes: Task 2의 최종 함수명, 실제 테스트 수, 검증 커밋.
- Produces: 현행 작업 추적, `ENTRY-06`/`ENTRY-10` 근거, append-only DEVLOG 기록.

- [ ] **Step 1: UE 5.7 Editor Build 실행**

```powershell
& 'D:\Programs\UE_5.7\Engine\Build\BatchFiles\Build.bat' DroneProtoEditor Win64 Development -Project='D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject' -NoLiveCoding -WaitMutex
```

Expected: exit code 0.

- [ ] **Step 2: 전체 `DroneProto` 자동화 실행**

```powershell
& 'D:\Programs\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject' -unattended -nop4 -nosplash -NullRHI "-ExecCmds=Automation RunTests DroneProto; Quit" "-TestExit=Automation Test Queue Empty" -log
```

Expected: 발견된 전체 테스트 0 failures.

- [ ] **Step 3: 문서에 실제 결과 반영**

`ImplementationMap_Current.md`에는 자동화로 확인한 C++ 경로와 실제 PIE 미실행 경계를 함께 기록한다. `DEVLOG.md`는 새 날짜 항목을 append하고 `AGENTS.md` 현재 상태에 branch/검증/남은 PIE를 추가한다.

- [ ] **Step 4: 최종 정합성 검사**

```powershell
git diff --check
git status --short --branch
```

- [ ] **Step 5: 문서 커밋**

```powershell
git add -f -- AGENTS.md docs/DEVLOG.md docs/Audit/ImplementationMap_Current.md
git commit -m "docs: 레이드 입장 실패 검증 기록"
```

- [ ] **Step 6: 인계 상태 확인**

```powershell
git status --short --branch
git log -3 --oneline
```

Expected: 기능 브랜치 clean. push·merge·Linear 전환은 수행하지 않는다.
