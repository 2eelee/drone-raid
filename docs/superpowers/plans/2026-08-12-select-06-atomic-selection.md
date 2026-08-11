# SELECT-06 Atomic Part Selection Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 부품 선택 교체를 재고와 3개 선택 슬롯이 함께 성공하거나 함께 유지되는 원자 요청으로 바꾸고, 서버 내부 오류 때 서버 권위 스냅샷으로 클라이언트를 복구한 뒤 고정 오류 팝업을 표시한다.

**Architecture:** `ADronePartInventory`가 이전 부품 반환과 새 부품 소비를 단일 preflight/commit으로 처리한다. `UDronePartReturnManager`가 이 재고 트랜잭션과 선택 슬롯 변경 및 성공 로그를 묶고, `ARaidPlayerController`는 요청 시작 시 3슬롯 스냅샷을 잡아 정상 거절과 서버 오류를 분리한다. 서버 오류는 전용 Reliable Client RPC로 3슬롯 전체를 덮어쓰며, `UDronePartSelectWidget`은 선택적 Blueprint 바인딩 또는 C++ 생성 fallback 팝업으로 정확한 안내 문구를 표시한다.

**Tech Stack:** Unreal Engine 5.7 C++, UE Automation Tests, server-authoritative RPC/replication, UMG C++ fallback layout.

## Global Constraints

- `docs/sources/현현_드론부품선택시스템_기획서.md`의 SELECT-06 문구를 기준으로 한다.
- `.uasset`, `.umap`, Blueprint/UMG 에셋은 수정하지 않는다.
- 기존 취소·Logout·사망·RaidEnd 반환 경로는 변경하지 않는다.
- 예상 가능한 품절은 기존 일반 선택 실패로 유지하고 팝업을 띄우지 않는다.
- 서버 내부 불일치나 원자 커밋 실패만 3슬롯 복구와 서버 오류 팝업으로 처리한다.
- 재고·반환·RPC 경계를 함께 바꾸므로 직접 테스트, UE Editor Build, 전체 `DroneProto` 회귀를 실행한다.

---

## Task 1: 현재 작업 추적과 재고 원자 교환 API

**Files:**

- Modify: `docs/Audit/ImplementationMap_Current.md`
- Modify: `Source/DroneProto/Tests/DronePartInventoryTests.cpp`
- Modify: `Source/DroneProto/Raid/DronePartInventory.h`
- Modify: `Source/DroneProto/Raid/DronePartInventory.cpp`

- [ ] **Step 1: 구현 지도 현재 작업 추적을 SELECT-06으로 전환**

`현재 작업 추적`에 아래 사실을 기록한다.

- branch: `codex/select-06-atomic-selection`
- 시작 HEAD: `9e5ec9e`
- current HEAD: 계획 커밋 HEAD
- 목적: SELECT-06 재고/선택 원자 커밋과 서버 오류 3슬롯 복구
- 예정 파일: Inventory, ReturnManager, RaidPlayerController, DronePartSelectWidget, 직접 테스트
- 영향 ID: `SELECT-06`, 성공 교체 로그 의미가 직접 영향받는 `REPLACE-01..04`
- 실행 경로: Client 요청 → Server RPC → ReturnManager → Inventory preflight/commit → 선택 갱신 또는 3슬롯 복구 RPC → Widget popup

- [ ] **Step 2: 재고 원자 교환 RED 테스트 추가**

`DronePartInventoryTests.cpp`에 `DroneProto.SELECT06.Inventory.AtomicExchange` 자동화 테스트를 추가한다. 최소 네 경우를 한 테스트에서 독립 fixture로 검증한다.

```cpp
FString FailureReason;
const EDronePartSelectionCommitResult Result = Inventory->TryCommitSelectionExchange(
    PreviousPartID,
    NewPartID,
    FailureReason);
```

검증 계약:

- 초기 선택: `NAME_None → CoreZenith`는 새 재고만 1 감소한다.
- 정상 교체: `CoreZenith → CoreBulwark`는 이전 재고 +1, 새 재고 -1을 한 번에 반영한다.
- 새 부품 품절: 결과 `OutOfStock`, 양쪽 stock 불변.
- 이전 부품 stock이 이미 max인 불일치: 결과 `ServerError`, 양쪽 stock 불변.

- [ ] **Step 3: RED 실행 및 새 API 부재 확인**

```powershell
& 'D:\Programs\UE_5.7\Engine\Build\BatchFiles\Build.bat' DroneProtoEditor Win64 Development -Project='D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject' -NoLiveCoding -WaitMutex
```

Expected: `TryCommitSelectionExchange`/결과 enum 부재로 컴파일 실패한다.

- [ ] **Step 4: 공유 결과 enum과 API 선언**

`DronePartInventory.h`에 C++ 내부 공유 결과를 선언한다.

```cpp
UENUM()
enum class EDronePartSelectionCommitResult : uint8
{
    Success,
    OutOfStock,
    ServerError
};
```

`ADronePartInventory` public API:

```cpp
EDronePartSelectionCommitResult TryCommitSelectionExchange(
    FName PreviousPartID,
    FName NewPartID,
    FString& OutFailureReason);
```

- [ ] **Step 5: 전 조건 확인 후 한 번만 commit**

`DronePartInventory.cpp` 구현 순서:

1. authority가 아니면 `ServerError`.
2. 새 부품 행이 없으면 `ServerError`, count가 0이면 `OutOfStock`.
3. 이전 ID가 있으면 이전 stock 행 존재와 `CurrentStock < MaxStock`을 확인한다. 위반 시 `ServerError`.
4. 모든 preflight 성공 후에만 이전 stock 증가와 새 stock 감소를 수행한다.
5. 두 값이 모두 바뀐 뒤 `OnInventoryChanged.Broadcast()`와 `ForceNetUpdate()`를 각각 한 번만 호출한다.
6. 실패 경로는 stock, delegate, replication dirty state를 건드리지 않는다.

- [ ] **Step 6: GREEN build와 직접 테스트**

```powershell
& 'D:\Programs\UE_5.7\Engine\Build\BatchFiles\Build.bat' DroneProtoEditor Win64 Development -Project='D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject' -NoLiveCoding -WaitMutex
& 'D:\Programs\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject' -unattended -nop4 -nosplash -NullRHI "-ExecCmds=Automation RunTests DroneProto.SELECT06.Inventory; Quit" "-TestExit=Automation Test Queue Empty" -log
```

Expected: build 성공, 1/1 통과.

- [ ] **Step 7: Task 1 커밋**

```powershell
git add -- 'docs/Audit/ImplementationMap_Current.md' 'Source/DroneProto/Tests/DronePartInventoryTests.cpp' 'Source/DroneProto/Raid/DronePartInventory.h' 'Source/DroneProto/Raid/DronePartInventory.cpp'
git commit -m "feat: 부품 재고 원자 교환 API 추가"
```

---

## Task 2: 요청 단위 선택 커밋과 서버 권위 3슬롯 복구

**Files:**

- Modify: `Source/DroneProto/Tests/DronePartInventoryTests.cpp`
- Modify: `Source/DroneProto/Raid/DronePartReturnManager.h`
- Modify: `Source/DroneProto/Raid/DronePartReturnManager.cpp`
- Modify: `Source/DroneProto/Raid/RaidPlayerController.h`
- Modify: `Source/DroneProto/Raid/RaidPlayerController.cpp`

- [ ] **Step 1: ReturnManager/Controller RED 테스트 추가**

추가 테스트:

- `DroneProto.SELECT06.Transaction.SuccessCommitsInventorySelectionAndLog`
- `DroneProto.SELECT06.Transaction.ServerErrorPreservesSnapshot`
- `DroneProto.SELECT06.Client.AuthoritativeSnapshotOverwrite`

성공 테스트는 교체 뒤 다음을 함께 확인한다.

- 이전 stock +1, 새 stock -1
- 지정 슬롯만 새 ID로 변경
- 다른 두 슬롯 불변
- `Replace` 성공 로그 정확히 1개

오류 테스트는 선택 슬롯의 이전 부품 stock을 의도적으로 max로 두어 서버 상태 불일치를 만든 뒤 Server RPC를 호출하고 다음을 확인한다.

- Core/Left/Right 서버 선택 ID 전부 요청 전 snapshot 유지
- 모든 stock 불변
- Return log 추가 없음
- 전용 오류 복구 RPC 테스트 관측값이 snapshot 3개와 일치

Client 테스트는 로컬 선택 3개를 서로 다른 stale 값으로 만든 다음 client handler에 권위 snapshot을 넣어 세 값이 모두 교체되고 selected-changed/refresh/error delegate가 각 한 번 발생하는지 확인한다.

- [ ] **Step 2: RED 실행**

```powershell
& 'D:\Programs\UE_5.7\Engine\Build\BatchFiles\Build.bat' DroneProtoEditor Win64 Development -Project='D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject' -NoLiveCoding -WaitMutex
```

Expected: 새 manager API, client RPC, error delegate 부재로 컴파일 실패한다.

- [ ] **Step 3: ReturnManager 원자 선택 API 구현**

`UDronePartReturnManager` public API:

```cpp
EDronePartSelectionCommitResult TryCommitSelectedPartChange(
    ARaidPlayerController* PlayerController,
    EPartSlot Slot,
    FName NewPartID,
    FString& OutFailureReason);
```

구현 계약:

1. controller/inventory/authority/slot이 유효하지 않으면 `ServerError`.
2. 슬롯에서 `PreviousPartID`를 읽고 Inventory 원자 API를 호출한다.
3. `Success` 전에는 슬롯을 clear하거나 Return log를 쓰지 않는다.
4. 성공 후 슬롯을 `NewPartID`로 설정한다.
5. 실제 교체(`PreviousPartID != NAME_None`)일 때만 기존 포맷의 `Replace` 성공 로그를 한 번 남긴다.

- [ ] **Step 4: Controller 요청 흐름을 원자 API로 교체**

`Server_SelectDronePart_Implementation` 시작 시 권위 snapshot을 보존한다.

```cpp
const FName SnapshotCore = SelectedCorePartID;
const FName SnapshotLeft = SelectedLeftWeaponPartID;
const FName SnapshotRight = SelectedRightWeaponPartID;
```

기존 `ReturnSingleSelectedPart` 후 `TryConsumePart` 및 수동 재소비 보상 코드를 제거하고 `TryCommitSelectedPartChange` 한 번만 호출한다.

- `Success`: 기존 `Client_NotifyPartSelectionResult(true, ...)` 경로 유지.
- `OutOfStock`: 기존 일반 실패 결과와 선택 UI refresh만 사용.
- `ServerError`: 서버 3슬롯을 snapshot으로 명시적으로 복원하고 전용 RPC 전송.
- Inventory/ReturnManager 누락 및 저장된 선택과 재고의 불일치도 동일 ServerError 경로로 모은다.

- [ ] **Step 5: 전용 Reliable RPC와 delegate 구현**

`RaidPlayerController.h`에 추가:

```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPartSelectionServerError);

UPROPERTY(BlueprintAssignable, Category="Raid|PartSelect")
FOnPartSelectionServerError OnPartSelectionServerError;

UFUNCTION(Client, Reliable)
void Client_RestorePartSelectionAfterServerError(
    FName AuthoritativeCorePartID,
    FName AuthoritativeLeftWeaponPartID,
    FName AuthoritativeRightWeaponPartID);
```

Client implementation은 순서대로:

1. local Core/Left/Right를 인자로 완전히 덮어쓴다.
2. `OnSelectedPartsChanged.Broadcast()`.
3. `OnPartSelectUIRefreshRequested.Broadcast()`.
4. `OnPartSelectionServerError.Broadcast()`.

필요한 자동화 관측값은 기존 `WITH_DEV_AUTOMATION_TESTS` 패턴을 따라 최소 read-only getter/counter로 둔다.

- [ ] **Step 6: GREEN build와 Transaction/Client 테스트**

```powershell
& 'D:\Programs\UE_5.7\Engine\Build\BatchFiles\Build.bat' DroneProtoEditor Win64 Development -Project='D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject' -NoLiveCoding -WaitMutex
& 'D:\Programs\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject' -unattended -nop4 -nosplash -NullRHI "-ExecCmds=Automation RunTests DroneProto.SELECT06.Transaction+DroneProto.SELECT06.Client; Quit" "-TestExit=Automation Test Queue Empty" -log
```

Expected: 신규 Transaction/Client 테스트 전부 통과.

- [ ] **Step 7: Task 2 커밋**

```powershell
git add -- 'Source/DroneProto/Tests/DronePartInventoryTests.cpp' 'Source/DroneProto/Raid/DronePartReturnManager.h' 'Source/DroneProto/Raid/DronePartReturnManager.cpp' 'Source/DroneProto/Raid/RaidPlayerController.h' 'Source/DroneProto/Raid/RaidPlayerController.cpp'
git commit -m "feat: 선택 요청을 원자 트랜잭션으로 전환"
```

---

## Task 3: SELECT-06 서버 오류 팝업과 C++ fallback

**Files:**

- Modify: `Source/DroneProto/Tests/DronePartInventoryTests.cpp`
- Modify: `Source/DroneProto/Raid/DronePartSelectWidget.h`
- Modify: `Source/DroneProto/Raid/DronePartSelectWidget.cpp`

- [ ] **Step 1: 팝업 RED 테스트 추가**

`DroneProto.SELECT06.UI.ServerErrorPopupFallback`을 추가하고 현재 WBP를 load/initialize한 뒤 `ApplyPlanningLayout`을 호출한다.

검증:

- optional Blueprint binding이 없는 현재 에셋에서 C++ fallback panel/text가 생성된다.
- 오류 handler 호출 시 정확히 `일시적인 오류가 발생했습니다. 다시 시도해주세요.`가 표시된다.
- popup은 중앙 배치되고 표시 중 hit test를 가로채지 않는다.
- 다음 선택/취소 시도 또는 정상 성공 handler 뒤에는 숨겨진다.

- [ ] **Step 2: RED 실행**

```powershell
& 'D:\Programs\UE_5.7\Engine\Build\BatchFiles\Build.bat' DroneProtoEditor Win64 Development -Project='D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject' -NoLiveCoding -WaitMutex
```

Expected: popup binding/handler 부재로 컴파일 실패한다.

- [ ] **Step 3: Widget 계약과 delegate 생명주기 연결**

`DronePartSelectWidget.h`에 optional binding을 추가한다.

```cpp
UPROPERTY(meta=(BindWidgetOptional))
TObjectPtr<UBorder> ServerErrorPopupPanel;

UPROPERTY(meta=(BindWidgetOptional))
TObjectPtr<UTextBlock> ServerErrorPopupText;
```

`NativeConstruct`에서 controller의 `OnPartSelectionServerError`를 bind하고, `NativeDestruct`에서 제거한다. 공개 BlueprintCallable UI handler는 테스트와 향후 WBP 재사용이 가능하게 한다.

```cpp
UFUNCTION(BlueprintCallable, Category="Part Select")
void ShowPartSelectionServerError();
```

- [ ] **Step 4: C++ fallback 생성과 표시/숨김 구현**

`ApplyPlanningLayout`에서 optional binding이 없으면 root canvas에 `Planning_ServerErrorPopup` Border와 `Planning_ServerErrorPopupText`를 생성한다.

- 중앙 anchor/alignment, 다른 선택 패널보다 높은 ZOrder.
- 초기 `Collapsed`.
- 표시 문구는 코드 상수 하나로 고정.
- 표시 visibility는 `Not Hit-Testable (Self & All Children)`에 대응하는 `ESlateVisibility::HitTestInvisible`.
- `SelectFocusedPart`, `CancelFocusedPart`, 정상 선택 결과에서 숨긴다.
- `NativeDestruct`에서도 숨기고 delegate를 해제한다.

- [ ] **Step 5: GREEN build와 UI 직접 테스트**

```powershell
& 'D:\Programs\UE_5.7\Engine\Build\BatchFiles\Build.bat' DroneProtoEditor Win64 Development -Project='D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject' -NoLiveCoding -WaitMutex
& 'D:\Programs\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject' -unattended -nop4 -nosplash -NullRHI "-ExecCmds=Automation RunTests DroneProto.SELECT06.UI; Quit" "-TestExit=Automation Test Queue Empty" -log
```

Expected: 신규 UI 테스트 전부 통과.

- [ ] **Step 6: Task 3 커밋**

```powershell
git add -- 'Source/DroneProto/Tests/DronePartInventoryTests.cpp' 'Source/DroneProto/Raid/DronePartSelectWidget.h' 'Source/DroneProto/Raid/DronePartSelectWidget.cpp'
git commit -m "feat: 선택 서버 오류 복구 팝업 추가"
```

---

## Task 4: 회귀 검증과 canonical 문서 마감

**Files:**

- Modify: `docs/Audit/ImplementationMap_Current.md`
- Modify: `docs/DEVLOG.md`
- Modify: `AGENTS.md`

- [ ] **Step 1: SELECT-06와 인접 직접 회귀 실행**

```powershell
& 'D:\Programs\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject' -unattended -nop4 -nosplash -NullRHI "-ExecCmds=Automation RunTests DroneProto.SELECT06+DroneProto.D5.DronePartReturnManager+DroneProto.D5.DronePartSelectUI; Quit" "-TestExit=Automation Test Queue Empty" -log
```

Expected: SELECT-06와 기존 D5 직접 회귀 전부 통과.

- [ ] **Step 2: UE Editor Build와 전체 회귀 실행**

```powershell
& 'D:\Programs\UE_5.7\Engine\Build\BatchFiles\Build.bat' DroneProtoEditor Win64 Development -Project='D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject' -NoLiveCoding -WaitMutex
& 'D:\Programs\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject' -unattended -nop4 -nosplash -NullRHI "-ExecCmds=Automation RunTests DroneProto; Quit" "-TestExit=Automation Test Queue Empty" -log
```

Expected: build 성공, 기존 141개와 신규 SELECT-06 테스트 전부 통과. 실제 발견 개수는 로그에서 기록한다.

- [ ] **Step 3: 문서 상태를 실제 검증 결과로만 갱신**

- `ImplementationMap_Current.md`: `SELECT-06` 및 실제 영향이 확인된 `REPLACE-01..04`만 갱신하고 현재 작업 추적에 최종 HEAD/테스트 수/GUI PIE 미실행을 기록한다.
- `docs/DEVLOG.md`: 원자 API, 3슬롯 복구 RPC, popup fallback, RED→GREEN 및 전체 회귀 결과를 append한다.
- `AGENTS.md`: SELECT-06 완료 상태와 실제 검증 수를 추가하고, 오래된 `main/origin main caad02c` 문장을 현재 기준선으로 바로잡는다.
- GUI PIE는 실행하지 않았으면 `미실행`으로 유지하며 Blueprint 연결 완료로 과장하지 않는다.

- [ ] **Step 4: diff hygiene 확인**

```powershell
git diff --check
git status --short --branch
git diff --stat
```

범위 밖 파일, `.uasset`, `.umap` 변경이 없어야 한다.

- [ ] **Step 5: 문서 마감 커밋**

```powershell
git add -- 'docs/Audit/ImplementationMap_Current.md' 'docs/DEVLOG.md' 'AGENTS.md'
git commit -m "docs: SELECT-06 구현 및 검증 결과 기록"
```

- [ ] **Step 6: 최종 증거 재확인**

```powershell
git status --short --branch
git log -4 --oneline
git diff main...HEAD --check
git diff --stat main...HEAD
```

Expected: 작업 트리 clean, SELECT-06 범위 커밋만 존재. Push/merge는 수행하지 않고 사용자에게 인계한다.
