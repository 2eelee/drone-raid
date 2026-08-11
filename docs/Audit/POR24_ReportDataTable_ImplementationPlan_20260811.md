# POR-24 DroneReport DataTable Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Bonus/Grade 전체 밸런스 수치를 원본 XLSX와 typed DataTable asset에서 원자 resolve해 서버 권한 DroneReport 계산과 표시명에 적용한다.

**Architecture:** `FDroneReportResolvedConfig`가 기존 report 공식의 모든 Bonus/Grade 수치를 값으로 보유하고, 신규 resolver가 Bonus/Settings/Grade 세 표를 전부 검증한 경우에만 config를 만든다. `ARaidPlayerController`는 기존 서버 report 생성 지점에서 최초 1회 resolve해 캐시하고, 클라이언트는 기존 Client RPC로 받은 `FDroneReportData`의 표시명만 렌더링한다.

**Tech Stack:** Unreal Engine 5.7 C++, Automation Tests, `UDataTable`, CSV, `.uasset`, `@oai/artifact-tool` XLSX 편집

## Global Constraints

- Dedicated Server 기준 server-authoritative 구조를 유지한다.
- 신규 RPC, replicated property, RepNotify, UMG layout, `.umap`, mesh/material/VFX/sound를 추가하거나 수정하지 않는다.
- 세 표 중 하나라도 잘못되면 부분 혼합 없이 canonical fallback 전체를 사용한다.
- production code보다 직접 관련 실패 테스트를 먼저 작성하고 RED 원인을 확인한다.
- Linear `POR-24`는 사용자 승인 전 `Done`으로 변경하지 않는다.

---

### Task 1: Resolved config와 원자 resolver

**Files:**
- Modify: `Source/DroneProto/Raid/DroneCombatTypes.h`
- Modify: `Source/DroneProto/Raid/DroneDataTableRows.h`
- Create: `Source/DroneProto/Raid/DroneReportDataTableResolver.h`
- Create: `Source/DroneProto/Raid/DroneReportDataTableResolver.cpp`
- Modify: `Source/DroneProto/Tests/DronePartInventoryTests.cpp`

**Interfaces:**
- Produces: `FDroneReportBonusRule`, `FDroneReportGradeRule`, `FDroneReportResolvedConfig`
- Produces: `EDroneReportDataFallbackReason`, `FDroneReportDataTableSet`
- Produces: `DroneReportData::TryResolve(const FDroneReportDataTableSet&, FDroneReportResolvedConfig&, EDroneReportDataFallbackReason&)`
- Produces: `FDroneReportRules::MakeCanonicalConfig()`

- [ ] **Step 1: 원자 resolve 실패를 잡는 테스트 두 개 작성**

`DronePartInventoryTests.cpp`에 transient `UDataTable` 세 개를 만드는 literal fixture를 추가하고 다음 테스트를 선언한다.

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDronePOR24ReportDataTableResolveTest,
    "DroneProto.POR24.ReportDataTable.ResolveCanonicalAndOverride",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDronePOR24ReportDataTableAtomicFallbackTest,
    "DroneProto.POR24.ReportDataTable.AtomicFallback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
```

첫 테스트는 BossSlayer `BonusScore=81`, 표시명 `TABLE BOSS`, S `MinScore=900`을 literal로 넣고 resolver 출력이 그 값을 갖는지 확인한다. 둘째 테스트는 Bonus 표는 유효하지만 Grade 필수 행 하나가 빠진 fixture를 전달해 `TryResolve=false`, `MissingGradeRow`를 확인한다.

- [ ] **Step 2: Build RED 확인**

Run:

```powershell
& 'D:\Programs\UE_5.7\Engine\Build\BatchFiles\Build.bat' DroneProtoEditor Win64 Development -Project='D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject' -NoLiveCoding -WaitMutex
```

Expected: 신규 resolver/config symbol 부재로 compile 실패. 다른 기존 compile 오류이면 테스트를 수정하고 동일 RED를 다시 확인한다.

- [ ] **Step 3: row schema와 config 최소 구현**

`FDroneBonusRow::BonusID`를 원본과 같은 `int32`로 바꾸고 설계 문서의 13개 Bonus 조건 열을 추가한다. `FDroneReportSettingsRow`에는 `BonusScoreCap`, `FDroneGradeRow`에는 기존 필드를 유지한다.

`DroneCombatTypes.h`에 다음 값을 갖는 plain config struct를 추가한다.

```cpp
struct FDroneReportBonusRule
{
    EDroneReportBonusType Type = EDroneReportBonusType::BossSlayer;
    FText DisplayName;
    int32 PrimaryScore = 0;
    int32 SecondaryScore = 0;
    float PrimaryMinCombatDuration = 0.0f;
    float SecondaryMinCombatDuration = 0.0f;
    float PrimaryMinBossDamageRatio = 0.0f;
    float SecondaryMinBossDamageRatio = 0.0f;
    float PrimaryMinDamagePerMinute = 0.0f;
    float SecondaryMinDamagePerMinute = 0.0f;
    float PrimaryMinMoveDistance = 0.0f;
    float SecondaryMinMovePerMinute = 0.0f;
    float PrimaryMinHealAmount = 0.0f;
    float SecondaryMinHealAmount = 0.0f;
    float LateJoinBossHPThresholdRatio = 0.0f;
    int32 MaxDamageTakenCount = -1;
    int32 MaxScore = 0;
    bool bRequiresBossDefeated = false;
    bool bRequiresAlive = false;
};
```

`FDroneReportResolvedConfig`는 enum별 bonus rule, 내림차순 grade rule, `BonusScoreCap`을 보유하며 `FindBonusRule`을 제공한다. `MakeCanonicalConfig()`는 현행 상수와 동일한 literal 값을 만든다.

- [ ] **Step 4: resolver 최소 구현**

`TryResolve`는 다음 고정 순서로 검증한다.

1. 세 table 존재와 정확한 row struct
2. Bonus 5행, Settings 1행, Grade 4행의 exact row name set
3. BonusID 5001~5005와 BonusName→enum 매핑
4. 중복 bonus type/grade 금지
5. score/cap은 0 이상, ratio는 0~1, duration/metric은 0 이상
6. primary/secondary score가 각 MaxScore 이하
7. Grade min 내림차순과 각 min≤max

실패 시 `OutConfig`를 publish하지 않고 deterministic reason을 반환한다.

- [ ] **Step 5: Build와 직접 resolver GREEN 확인**

Build 후:

```powershell
& 'D:\Programs\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject' -unattended -nop4 -nosplash -NullRHI "-ExecCmds=Automation RunTests DroneProto.POR24.ReportDataTable; Quit" "-TestExit=Automation Test Queue Empty" -log
```

Expected: 2 tests found, 2 succeeded.

- [ ] **Step 6: 코드 커밋**

```powershell
git add Source/DroneProto/Raid/DroneCombatTypes.h Source/DroneProto/Raid/DroneDataTableRows.h Source/DroneProto/Raid/DroneReportDataTableResolver.h Source/DroneProto/Raid/DroneReportDataTableResolver.cpp Source/DroneProto/Tests/DronePartInventoryTests.cpp
git commit -m "feat: POR-24 리포트 데이터 원자 resolver 추가"
```

### Task 2: Config 기반 계산과 서버 표시 payload

**Files:**
- Modify: `Source/DroneProto/Raid/DroneCombatTypes.h`
- Modify: `Source/DroneProto/Raid/DroneReportWidget.h`
- Modify: `Source/DroneProto/Raid/DroneReportWidget.cpp`
- Modify: `Source/DroneProto/Tests/DronePartInventoryTests.cpp`

**Interfaces:**
- Consumes: `FDroneReportResolvedConfig`
- Produces: `FDroneReportRules::BuildReportData(const FDroneCombatRecord&, bool, const FDroneReportResolvedConfig&)`
- Produces: `FDroneReportData::AchievedBonusDisplayNames`

- [ ] **Step 1: 계산값과 표시 payload RED 테스트 작성**

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDronePOR24ReportDataTableServerPayloadTest,
    "DroneProto.POR24.ReportDataTable.ServerPayloadDisplayNames",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
```

BossSlayer 기본 점수를 81, 표시명을 `TABLE BOSS`, S 최소 점수를 900으로 바꾼 config와 literal combat record를 사용한다. `BonusScore=81`, `Grade=A`, `AchievedBonusDisplayNames[0]=TABLE BOSS`와 widget 표시 `TABLE BOSS`를 확인한다.

- [ ] **Step 2: 테스트 RED 확인**

Build 후 direct suite를 실행한다. Expected: config overload 또는 payload field 부재 compile RED. 이미 compile되면 기존 hardcoded 80/850/enum display 때문에 assertion RED여야 한다.

- [ ] **Step 3: report rule을 config 값으로 교체**

기본 성과 점수식은 그대로 둔다. Bonus 5종의 primary/secondary 조건과 점수, 개별 MaxScore, 전체 BonusScoreCap, Grade MinScore만 config에서 읽는다. 기존 2-argument `BuildReportData`와 `CalculateGrade(float)`는 canonical config를 호출하는 호환 overload로 유지한다.

`AddBonus`는 enum과 server-resolved `DisplayName`을 같은 index에 추가한다.

- [ ] **Step 4: widget을 payload 우선 표시로 변경**

`RefreshReport`는 `AchievedBonusDisplayNames.Num() == AchievedBonusList.Num()`이면 payload를 join하고, 아니면 기존 enum 표시명 fallback을 사용한다. 빈 목록은 계속 빈 `FText`를 반환한다.

- [ ] **Step 5: direct suite GREEN**

Expected: `DroneProto.POR24.ReportDataTable` 3/3 성공. 이어서 기존 `DroneProto.D10.DroneReport` 2/2와 `DroneProto.D11.DroneReport` 2/2 성공.

- [ ] **Step 6: 코드 커밋**

```powershell
git add Source/DroneProto/Raid/DroneCombatTypes.h Source/DroneProto/Raid/DroneReportWidget.h Source/DroneProto/Raid/DroneReportWidget.cpp Source/DroneProto/Tests/DronePartInventoryTests.cpp
git commit -m "feat: POR-24 리포트 계산을 데이터 설정에 연결"
```

### Task 3: PlayerController 서버 resolve 연결

**Files:**
- Modify: `Source/DroneProto/Raid/RaidPlayerController.h`
- Modify: `Source/DroneProto/Raid/RaidPlayerController.cpp`
- Modify: `Source/DroneProto/Tests/DronePartInventoryTests.cpp`

**Interfaces:**
- Consumes: `DroneReportData::TryResolve`
- Produces: `ARaidPlayerController::ResolveDroneReportConfigForServer()`

- [ ] **Step 1: production 연결 RED 테스트 추가**

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDronePOR24ReportDataTableControllerSourceTest,
    "DroneProto.POR24.ReportDataTable.ControllerUsesResolvedSource",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
```

기존 drone/controller test context에 transient 세 table을 reflection으로 주입한다. `NoDamage`의 최소 시간·피해율을 0, 점수를 77, 표시명을 `TABLE NO DAMAGE`로 둔 뒤 살아 있는 드론의 RaidTimeLimit report를 한 번 생성한다. 테스트는 `BonusScore=77`과 custom 표시명이 `LastDroneReportData.AchievedBonusDisplayNames`에 남는 실제 결과를 확인한다. 두 번째 report 요청은 기존 duplicate guard로 거부되어 최초 config/result가 유지돼야 한다.

- [ ] **Step 2: controller RED 확인**

Build와 direct suite를 실행한다. Expected: table property 부재 또는 기존 `BuildReportData(record, defeated)` 호출 때문에 RED.

- [ ] **Step 3: controller 최소 구현**

`BonusDataTable`, `DroneReportSettingsDataTable`, `GradeDataTable`을 `EditDefaultsOnly`로 추가하고, config/ready/source/reason을 private cache한다. report 생성 서버 경로에서 최초 1회만 resolve하고 다음 호출은 snapshot을 재사용한다. 성공·fallback 로그를 player controller당 1회만 남긴다.

기존 `Client_ReceiveDroneReport`, duplicate manager, return flow는 수정하지 않는다.

- [ ] **Step 4: direct suite GREEN**

Expected: `DroneProto.POR24.ReportDataTable` 4/4, D10 2/2, D11 2/2.

- [ ] **Step 5: 코드 커밋**

```powershell
git add Source/DroneProto/Raid/RaidPlayerController.h Source/DroneProto/Raid/RaidPlayerController.cpp Source/DroneProto/Tests/DronePartInventoryTests.cpp
git commit -m "feat: POR-24 서버 리포트에 데이터 설정 적용"
```

### Task 4: canonical XLSX, CSV, shipping assets

**Files:**
- Modify: `docs/sources/현현_데이터테이블.xlsx`
- Create: `Data/DroneReport/DroneReportBonus.csv`
- Create: `Data/DroneReport/DroneReportSettings.csv`
- Create: `Data/DroneReport/DroneReportGrade.csv`
- Create: `Content/Data/DroneReport/DT_DroneReportBonus.uasset`
- Create: `Content/Data/DroneReport/DT_DroneReportSettings.uasset`
- Create: `Content/Data/DroneReport/DT_DroneReportGrade.uasset`
- Modify: `Source/DroneProto/Raid/RaidPlayerController.cpp`
- Modify: `Source/DroneProto/Tests/DronePartInventoryTests.cpp`

**Interfaces:**
- Produces: `/Game/Data/DroneReport/DT_DroneReportBonus`
- Produces: `/Game/Data/DroneReport/DT_DroneReportSettings`
- Produces: `/Game/Data/DroneReport/DT_DroneReportGrade`

- [ ] **Step 1: shipping asset RED 테스트 추가**

`ControllerUsesResolvedSource` 또는 별도 assertion block에서 세 asset path가 아직 null임을 정상 RED로 확인할 수 있도록 `StaticLoadObject`와 row struct/value 검증을 추가한다. CDO의 세 property가 정확한 asset을 hard reference하는지도 확인한다.

- [ ] **Step 2: direct suite RED 확인**

Expected: `Shipping assets load` assertion이 세 asset 부재로 실패하며 process crash나 timeout은 없어야 한다.

- [ ] **Step 3: XLSX를 보존 편집**

번들 `@oai/artifact-tool`로 편집 전 `Bonus`와 `Grade`를 render/view한다. `Bonus` 오른쪽에 설계 열을 추가하고 기존 header/data style을 복사하며 `DroneReportSettings` 시트를 같은 header 스타일로 추가한다. 기존 20개 sheet의 값·formula·style은 변경하지 않는다.

편집 후 다음을 검사한다.

```text
Bonus!A1:U6 = 5 canonical bonus rows and all special-condition values
DroneReportSettings!A1:B2 = REPORT_SETTINGS / 250
Grade!A1:D5 = existing canonical rows unchanged
```

수식 오류 scan과 두 시트 render/view를 통과한 export를 원본 경로와 conversation output 경로에 저장한다.

- [ ] **Step 4: canonical CSV 생성**

XLSX 표시값과 동일한 UTF-8 CSV 3개를 만든다. header는 C++ UPROPERTY 이름과 정확히 일치시키고 `Row Name`을 첫 열로 둔다.

- [ ] **Step 5: typed DataTable asset 생성**

UE 5.7 commandlet Python에서 `AssetImportTask`/CSV DataTable import를 사용해 세 CSV를 각 row struct로 import하고 저장한다. import 뒤 Editor를 재시작해 asset을 다시 load한다.

- [ ] **Step 6: CDO hard reference 연결**

`ARaidPlayerController` constructor의 `ConstructorHelpers::FObjectFinder<UDataTable>`로 세 fixed path를 연결한다. runtime CSV 읽기는 추가하지 않는다.

- [ ] **Step 7: shipping asset GREEN 확인**

Build 후 `DroneProto.POR24.ReportDataTable` 4/4를 실행한다. Expected: asset load, row struct, canonical 전 필드, CDO hard reference가 모두 성공한다.

- [ ] **Step 8: data/asset 커밋**

```powershell
git add Data/DroneReport Content/Data/DroneReport Source/DroneProto/Raid/RaidPlayerController.cpp Source/DroneProto/Tests/DronePartInventoryTests.cpp
git add -f docs/sources/현현_데이터테이블.xlsx
git commit -m "feat: POR-24 리포트 데이터테이블 에셋 추가"
```

### Task 5: 최종 검증과 canonical 문서

**Files:**
- Modify: `docs/Audit/ImplementationMap_Current.md`
- Modify: `docs/DEVLOG.md`
- Modify: `AGENTS.md`

- [ ] **Step 1: 직접·영향 suite 실행**

순서대로 실행하고 각 발견 수와 성공 수를 기록한다.

```text
DroneProto.POR24.ReportDataTable => 4/4
DroneProto.Q5.DataTable => 3/3
DroneProto.D10.DroneReport => 2/2
DroneProto.D11.DroneReport => 2/2
```

- [ ] **Step 2: UE 5.7 Editor Build**

Run the canonical `Build.bat` command. Expected: exit 0.

- [ ] **Step 3: 확대 필요성 판정**

`git diff`로 GameMode, 공유 재고, 피해, replicated property, RPC signature, 접속 생명주기 변경이 없는지 확인한다. 모두 없으면 전체 `DroneProto`는 `not required`로 기록한다. 하나라도 생겼거나 direct suite가 payload 직렬화를 닫지 못하면 가장 작은 관련 suite를 먼저 실행하고 필요할 때만 전체 회귀로 확대한다.

- [ ] **Step 4: 문서 갱신**

`ImplementationMap_Current.md`의 현재 작업 추적과 `REPORT-DATA-01`만 실제 commit/test 결과로 갱신한다. `DEVLOG.md`에 POR-24 범위, RED/GREEN, commands/counts, XLSX/CSV/assets, fallback, 미실행 PIE를 append한다. 최우선 다음 작업은 `Core/Weapon`으로 이동시키고 `AGENTS.md` 현재 상태를 같은 내용으로 갱신한다.

- [ ] **Step 5: 정적 검사와 문서 커밋**

```powershell
git diff --check
git status --short --branch
git add -f docs/Audit/ImplementationMap_Current.md docs/DEVLOG.md AGENTS.md docs/Audit/POR24_ReportDataTable_ImplementationPlan_20260811.md
git diff --cached --check
git commit -m "docs: POR-24 리포트 데이터테이블 검증 기록"
```

- [ ] **Step 6: Linear-ready 요약 준비**

POR-24에 변경 파일, 테스트 수, Build, 전체 회귀/PIE 미실행 이유, 남은 경계를 적을 summary를 준비한다. 상태는 `In Progress`로 유지한다.
