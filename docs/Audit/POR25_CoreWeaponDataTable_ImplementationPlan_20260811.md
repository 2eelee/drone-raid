# POR-25 Core/Weapon DataTable Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Core/Weapon의 전체 밸런스 수치를 원본 XLSX와 typed DataTable asset에서 원자 resolve해 기존 서버 권한 공격·이동·회복 계산에 적용한다.

**Architecture:** `FDroneCombatResolvedConfig`가 Core 3종과 Weapon 3종의 수치를 값으로 보유하고, 신규 resolver는 두 표를 모두 검증한 경우에만 완성된 config를 게시한다. `ADrone`은 최초 서버 계산에서 config를 한 번 resolve해 캐시하고, 기존 `FDroneCombatRules` 공식은 resolved config의 수치만 읽는다.

**Tech Stack:** Unreal Engine 5.7 C++, Automation Tests, `UDataTable`, UTF-8 CSV, `.uasset`, UE Python commandlet, `@oai/artifact-tool` XLSX 검증

## Global Constraints

- Dedicated Server 기준 server-authoritative 구조를 유지한다.
- 신규 RPC, replicated property, RepNotify, UMG layout, `.umap`, mesh/material/VFX/sound를 추가하거나 수정하지 않는다.
- Core/Weapon 두 표 중 하나라도 잘못되면 부분 혼합 없이 canonical fallback 전체를 사용한다.
- 기존 enum 식별, 공격·이동·회복 공식, authority guard와 로그 검색어를 유지한다.
- 원본 XLSX 값과 서식은 수정하지 않는다. Core/Weapon 시트를 읽고 렌더해 CSV/asset 동등성만 검증한다.
- production code보다 직접 관련 실패 테스트를 먼저 작성하고 예상한 RED 원인을 확인한다.
- Linear `POR-25`는 사용자 승인 전 `Done`으로 변경하지 않는다.

---

### Task 1: Resolved config와 Core/Weapon 원자 resolver

**Files:**
- Modify: `Source/DroneProto/Raid/DroneCombatTypes.h`
- Create: `Source/DroneProto/Raid/DroneCombatDataTableResolver.h`
- Create: `Source/DroneProto/Raid/DroneCombatDataTableResolver.cpp`
- Modify: `Source/DroneProto/Tests/DronePartInventoryTests.cpp`

**Interfaces:**
- Produces: `FDroneCoreRule`, `FDroneWeaponRule`, `FDroneCombatResolvedConfig`
- Produces: `FDroneCombatRules::MakeCanonicalConfig()`
- Produces: `EDroneCombatDataFallbackReason`, `FDroneCombatDataTableSet`
- Produces: `DroneCombatData::TryResolve(const FDroneCombatDataTableSet&, FDroneCombatResolvedConfig&, EDroneCombatDataFallbackReason&)`
- Produces: `DroneCombatData::ToString(EDroneCombatDataFallbackReason)`

- [x] **Step 1: resolver compile RED 테스트 작성**

`DronePartInventoryTests.cpp`에 `DroneCombatDataTableResolver.h`를 include하고 `FDroneCoreRow`/`FDroneWeaponRow` transient table fixture를 literal로 만든다. fixture는 `CORE_001`~`003`, `WEAPON_001`~`003` 전 필드를 설계 문서 값으로 넣는다.

```cpp
struct FDroneCombatTableFixture
{
    UDataTable* Core = nullptr;
    UDataTable* Weapon = nullptr;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDronePOR25CombatDataTableResolveTest,
    "DroneProto.POR25.CombatDataTable.ResolveCanonicalAndOverride",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDronePOR25CombatDataTableAtomicFallbackTest,
    "DroneProto.POR25.CombatDataTable.AtomicFallback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDronePOR25CombatDataTableReasonTest,
    "DroneProto.POR25.CombatDataTable.DeterministicReasons",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
```

첫 테스트는 Zenith `EffectValue01=0.05`, Pulse `BaseDamage=12` override가 resolved rule에 들어가는지 확인한다. 둘째 테스트는 Core가 유효한 상태에서 `WEAPON_003`을 제거하고 `TryResolve=false`, `MissingWeaponRow`를 확인하며 사전에 넣어 둔 `OutConfig` sentinel이 publish되지 않는지 확인한다. 셋째 테스트는 wrong row struct, invalid Core effect type, invalid Weapon ID, non-finite/negative 수치를 각각 고정 reason으로 확인한다.

- [x] **Step 2: Editor Build로 compile RED 확인**

Run:

```powershell
& 'D:\Programs\UE_5.7\Engine\Build\BatchFiles\Build.bat' DroneProtoEditor Win64 Development -Project='D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject' -NoLiveCoding -WaitMutex
```

Expected: `DroneCombatDataTableResolver.h` 또는 신규 config symbol 부재 compile 실패. 오탈자나 include path 오류가 아니라 요구 API 부재가 원인인지 확인한다.

- [x] **Step 3: runtime config와 canonical config 구현**

`DroneCombatTypes.h`에 다음 값 타입을 추가한다.

```cpp
struct FDroneCoreRule
{
    EDroneCombatCoreType Type = EDroneCombatCoreType::None;
    float AttackModifier = 1.0f;
    float MoveSpeedModifier = 1.0f;
    FName EffectType = NAME_None;
    float EffectValue01 = 0.0f;
    float EffectValue02 = 0.0f;
    float EffectMaxValue = 0.0f;
};

struct FDroneWeaponRule
{
    EDroneCombatWeaponType Type = EDroneCombatWeaponType::None;
    float BaseDamage = 0.0f;
    FName SpecialEffectType = NAME_None;
    float SpecialValue01 = 0.0f;
    float SpecialValue02 = 0.0f;
    float SpecialMaxValue = 0.0f;
    int32 HitCount = 0;
};

struct FDroneCombatResolvedConfig
{
    TArray<FDroneCoreRule> CoreRules;
    TArray<FDroneWeaponRule> WeaponRules;
    const FDroneCoreRule* FindCoreRule(EDroneCombatCoreType Type) const;
    const FDroneWeaponRule* FindWeaponRule(EDroneCombatWeaponType Type) const;
};
```

`FDroneCombatRules::MakeCanonicalConfig()`는 XLSX와 현재 공식의 정확한 3+3 rules를 만든다. `None`은 rule을 추가하지 않고 기존 default 결과를 유지한다.

- [x] **Step 4: 원자 resolver 최소 구현**

`DroneCombatDataTableResolver.h`에 exact table set과 실패 reason을 선언한다.

```cpp
enum class EDroneCombatDataFallbackReason : uint8
{
    None,
    MissingCoreTable,
    MissingWeaponTable,
    InvalidCoreRowStruct,
    InvalidWeaponRowStruct,
    MissingCoreRow,
    MissingWeaponRow,
    UnexpectedCoreRow,
    UnexpectedWeaponRow,
    InvalidCoreIdentity,
    InvalidWeaponIdentity,
    InvalidCoreEffectType,
    InvalidWeaponEffectType,
    DuplicateCoreType,
    DuplicateWeaponType,
    InvalidCoreRange,
    InvalidWeaponRange
};

struct FDroneCombatDataTableSet
{
    const UDataTable* Core = nullptr;
    const UDataTable* Weapon = nullptr;
};
```

resolver는 `FDroneCoreRow::StaticStruct()`/`FDroneWeaponRow::StaticStruct()`를 요구하고, exact row name·count·numeric ID를 검증한 뒤 expected row table로 runtime enum과 effect type을 고정 매핑한다. local `Candidate`만 채우고 모든 검증이 끝난 뒤 `OutConfig = MoveTemp(Candidate)`로 publish한다.

- [x] **Step 5: POR25 resolver GREEN 확인**

Build 후 실행:

```powershell
& 'D:\Programs\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject' -unattended -nop4 -nosplash -NullRHI "-ExecCmds=Automation RunTests DroneProto.POR25.CombatDataTable; Quit" "-TestExit=Automation Test Queue Empty" -log
```

Expected: 현 단계 3/3 성공, 실패 0.

- [x] **Step 6: resolver 커밋**

```powershell
git add Source/DroneProto/Raid/DroneCombatTypes.h Source/DroneProto/Raid/DroneCombatDataTableResolver.h Source/DroneProto/Raid/DroneCombatDataTableResolver.cpp Source/DroneProto/Tests/DronePartInventoryTests.cpp
git diff --cached --check
git commit -m "feat: POR-25 전투 데이터 원자 resolver 추가"
```

### Task 2: 기존 Core/Weapon 계산을 resolved config 값으로 전환

**Files:**
- Modify: `Source/DroneProto/Raid/DroneCombatTypes.h`
- Modify: `Source/DroneProto/Tests/DronePartInventoryTests.cpp`

**Interfaces:**
- Consumes: `FDroneCombatResolvedConfig`, `FindCoreRule`, `FindWeaponRule`
- Produces: `CalculateWeaponDamage(const FDroneWeaponCalculationInput&, const FDroneCombatResolvedConfig&)`
- Produces: `CalculateCoreBonus(const FDroneCoreCalculationInput&, const FDroneCombatResolvedConfig&)`
- Produces: `CalculateDrainHeal(float, const FDroneCombatResolvedConfig&)`
- Keeps: 기존 overload 세 개는 `MakeCanonicalConfig()`를 사용해 호출 호환성을 유지

- [x] **Step 1: 계산 주입 RED 테스트 추가**

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDronePOR25CombatDataTableCalculationTest,
    "DroneProto.POR25.CombatDataTable.CalculationUsesResolvedValues",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
```

canonical config를 복사해 Pulse `BaseDamage=12`, third-hit damage `30`, Zenith step `0.05`, Drain heal ratio `0.20`, cap `7`로 바꾼다. 새 overload가 Pulse 1회 12, 3회 30, Zenith HP 50%에서 `1.25`, Drain 피해 50에서 heal 7을 반환해야 한다. Fracture와 Vector는 canonical config에서 기존 11/15 결과를 유지하는지 함께 확인한다.

- [x] **Step 2: Build RED 확인**

Task 1과 같은 Build 명령을 실행한다. Expected: config overload 부재 compile 실패. 이미 compile되면 기존 상수 8/18/0.02/0.12 때문에 assertion RED여야 한다.

- [x] **Step 3: config 기반 계산 최소 구현**

각 계산은 runtime type으로 rule을 찾고 없으면 기존 `None` 결과를 반환한다. 공식은 그대로 두고 다음 필드만 대입한다.

```cpp
// Pulse
Result.BaseDamage = Rule->BaseDamage;
const int32 TriggerCount = FMath::RoundToInt(Rule->SpecialValue01);
Result.WeaponDamage = Result.PulseAttackCount >= TriggerCount
    ? Rule->SpecialValue02
    : Result.BaseDamage;

// Zenith
const float Step = Rule->EffectValue02;
const float Bonus = FMath::FloorToFloat(Result.HPRatio / Step) * Rule->EffectValue01;
Result.CoreBonusAttackModifier = 1.0f + FMath::Min(Bonus, Rule->EffectMaxValue);

// Drain
return FMath::Min(FMath::Max(0.0f, DamageDealt) * DrainRule->EffectValue01, DrainRule->EffectValue02);
```

Booster 공격 보너스가 speed bonus의 절반인 공식, Vector 공격 후 거리 reset, Pulse 좌우 count 분리는 바꾸지 않는다.

- [x] **Step 4: 직접·기존 formula GREEN 확인**

POR25 direct suite는 4/4가 되어야 한다. 이어서 실행:

```powershell
& 'D:\Programs\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject' -unattended -nop4 -nosplash -NullRHI "-ExecCmds=Automation RunTests DroneProto.D9.DroneCombat; Quit" "-TestExit=Automation Test Queue Empty" -log
& 'D:\Programs\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject' -unattended -nop4 -nosplash -NullRHI "-ExecCmds=Automation RunTests DroneProto.D13.DroneCombat.SpecAlignment; Quit" "-TestExit=Automation Test Queue Empty" -log
```

Expected: POR25 4/4, D9 1/1, D13 1/1 성공.

- [x] **Step 5: 계산 커밋**

```powershell
git add Source/DroneProto/Raid/DroneCombatTypes.h Source/DroneProto/Tests/DronePartInventoryTests.cpp
git diff --cached --check
git commit -m "feat: POR-25 전투 계산을 데이터 설정에 연결"
```

### Task 3: `ADrone` 서버 resolve/cache 연결

**Files:**
- Modify: `Source/DroneProto/Drone.h`
- Modify: `Source/DroneProto/Drone.cpp`
- Modify: `Source/DroneProto/Tests/DronePartInventoryTests.cpp`

**Interfaces:**
- Consumes: `DroneCombatData::TryResolve`, config 기반 계산 overload
- Produces: `ADrone::ResolveDroneCombatConfigForServer()`
- Produces under `WITH_DEV_AUTOMATION_TESTS`: `SetDroneCombatDataTablesForTest(UDataTable*, UDataTable*)`, `GetDroneCombatConfigResolveCountForTest()`

- [x] **Step 1: 실제 server path RED 테스트 추가**

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDronePOR25CombatDataTableDroneCacheTest,
    "DroneProto.POR25.CombatDataTable.DroneUsesAndCachesResolvedSource",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
```

기존 server drone/boss test context에 transient Core/Weapon fixture를 주입하고 Pulse base 12, Drain heal ratio 0.20/cap 7로 바꾼다. 실제 `ApplyLoadout`과 server attack 경로에서 boss HP delta가 table 값으로 계산되고, 같은 drone의 move-speed refresh와 Drain heal 뒤에도 resolve count가 1인지 확인한다. 잘못된 Weapon table을 다시 주입한 별도 drone은 canonical 전체 fallback을 사용하며 reason이 `MissingWeaponRow`인지 확인한다.

- [x] **Step 2: Build RED 확인**

Task 1 Build 명령을 실행한다. Expected: test setter/cache getter 또는 drone config method 부재 compile 실패.

- [x] **Step 3: `ADrone` property와 cache 추가**

`Drone.h` private 영역에 다음 상태를 추가한다.

```cpp
class UDataTable;

UPROPERTY(EditDefaultsOnly, Category = "Drone|Combat|Data", meta = (AllowPrivateAccess = "true"))
TObjectPtr<UDataTable> DroneCoreDataTable = nullptr;

UPROPERTY(EditDefaultsOnly, Category = "Drone|Combat|Data", meta = (AllowPrivateAccess = "true"))
TObjectPtr<UDataTable> DroneWeaponDataTable = nullptr;

FDroneCombatResolvedConfig CachedDroneCombatConfig;
bool bDroneCombatConfigResolved = false;
bool bDroneCombatConfigUsesDataTables = false;
FString DroneCombatConfigFallbackReason;
```

`ResolveDroneCombatConfigForServer()`는 처음 한 번만 `TryResolve`하고 실패 시 `MakeCanonicalConfig()` 전체를 저장한다. 성공/실패 `[DR_SUMMARY] DroneCombatData ...` 로그도 이 지점에서 한 번만 남긴다.

- [x] **Step 4: 기존 server 계산 호출을 같은 cache로 연결**

`CalculateWeaponDamageForServer`, `CalculateCoreForServer`, `RefreshMoveSpeedForServer`, `ApplyDrainHealForServer`가 모두 `ResolveDroneCombatConfigForServer()` 반환값을 config overload에 전달한다. `CalculateCoreForServer`의 `const`는 캐시를 갱신해야 하므로 제거한다. 공격·이동·회복 authority guard, runtime counters와 combat record 갱신은 그대로 둔다.

- [x] **Step 5: POR25·실제 전투 GREEN 확인**

Build 후 POR25 direct suite 5/5를 실행한다. 이어서 다음 prefix를 각각 실행한다.

```text
DroneProto.D7.Drone => 4/4
DroneProto.D9.Drone => 2/2
DroneProto.D13.DroneCombat.SpecAlignment => 1/1
DroneProto.D20.LogSemantics.NoDamageAttack => 1/1
```

- [x] **Step 6: 서버 연결 커밋**

```powershell
git add Source/DroneProto/Drone.h Source/DroneProto/Drone.cpp Source/DroneProto/Tests/DronePartInventoryTests.cpp
git diff --cached --check
git commit -m "feat: POR-25 드론 전투 데이터 캐시 연결"
```

### Task 4: canonical CSV, typed DataTable asset과 CDO hard reference

**Files:**
- Create: `Data/DroneCombat/DroneCore.csv`
- Create: `Data/DroneCombat/DroneWeapon.csv`
- Create: `Content/Data/DroneCombat/DT_DroneCore.uasset`
- Create: `Content/Data/DroneCombat/DT_DroneWeapon.uasset`
- Modify: `Source/DroneProto/Drone.cpp`
- Modify: `Source/DroneProto/Tests/DronePartInventoryTests.cpp`
- Read/verify only: `docs/sources/현현_데이터테이블.xlsx`

**Interfaces:**
- Produces: `/Game/Data/DroneCombat/DT_DroneCore`
- Produces: `/Game/Data/DroneCombat/DT_DroneWeapon`
- Produces: CDO properties `DroneCoreDataTable`, `DroneWeaponDataTable`

- [x] **Step 1: asset RED 테스트 두 개 추가**

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDronePOR25CombatDataTableAssetContractTest,
    "DroneProto.POR25.CombatDataTable.AssetPathsAndHardReferences",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDronePOR25CombatDataTableAssetCanonicalEqualityTest,
    "DroneProto.POR25.CombatDataTable.AssetCanonicalEquality",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
```

첫 테스트는 두 fixed path load, row struct, `ADrone` CDO reflection property, `Drone.cpp` hard reference 문자열, production runtime CSV read 부재를 확인한다. 둘째 테스트는 shipping tables를 resolver에 전달해 canonical config와 Core/Weapon 모든 필드가 동등한지 확인한다.

- [x] **Step 2: direct suite asset RED 확인**

Build와 POR25 suite를 실행한다. Expected: 기존 5개는 성공하고 신규 두 테스트가 asset load null로 실패해 총 5/7 성공, 2/7 실패.

- [x] **Step 3: XLSX read-only 검증과 canonical CSV 생성**

번들 `@oai/artifact-tool`로 `Core!A1:J4`, `Weapon!A1:J4` 값·computed style을 inspect하고 두 시트를 render/view한다. XLSX는 export하거나 덮어쓰지 않는다.

CSV는 UTF-8로 다음 literal content를 생성한다.

```csv
---,CoreID,AttackModifier,MoveSpeedModifier,EffectType,EffectValue01,EffectValue02,EffectMaxValue
CORE_001,1001,1,1,HP_TO_ATTACK,0.02,0.1,0.2
CORE_002,1002,0.95,1,MOVE_TO_SPEED_ATTACK,0.03,20,0.3
CORE_003,1003,0.85,0.9,DAMAGE_TO_HEAL,0.12,3,0
```

```csv
---,WeaponID,BaseDamage,SpecialEffectType,SpecialValue01,SpecialValue02,SpecialMaxValue,HitCount
WEAPON_001,2001,8,THIRD_HIT_STRONG,3,18,0,1
WEAPON_002,2002,5,FRACTURE_MULTI_HIT,3,2,0,4
WEAPON_003,2003,7,MOVE_DISTANCE_DAMAGE,5,1,8,1
```

- [x] **Step 4: UE Python commandlet로 typed asset 생성**

conversation temp의 단일 Python script에서 다음 helper를 사용한다. 저장 대상은 프로젝트 `Content/Data/DroneCombat`이다.

```python
import unreal

def import_table(asset_name, row_struct_path, csv_path):
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.DataTableFactory()
    factory.set_editor_property("struct", unreal.load_object(None, row_struct_path))
    table = asset_tools.create_asset(asset_name, "/Game/Data/DroneCombat", unreal.DataTable, factory)
    if not unreal.DataTableFunctionLibrary.fill_data_table_from_csv_file(table, csv_path):
        raise RuntimeError(f"CSV import failed: {csv_path}")
    if not unreal.EditorAssetLibrary.save_loaded_asset(table, False):
        raise RuntimeError(f"Asset save failed: {asset_name}")

import_table("DT_DroneCore", "/Script/DroneProto.DroneCoreRow", r"D:\Documents\Unreal Projects\DroneProto\Data\DroneCombat\DroneCore.csv")
import_table("DT_DroneWeapon", "/Script/DroneProto.DroneWeaponRow", r"D:\Documents\Unreal Projects\DroneProto\Data\DroneCombat\DroneWeapon.csv")
```

Run:

```powershell
& 'D:\Programs\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject' -unattended -nop4 -nosplash -NullRHI -run=pythonscript "-script=C:\Users\Public\Documents\ESTsoft\CreatorTemp\droneproto-core-weapon-019fefd1\create_por25_assets.py" -log
```

Expected: 두 CSV import `0 Problems`, 두 asset save 성공. Editor process 종료 후 파일 존재를 확인한다.

- [x] **Step 5: CDO hard reference 연결**

`ADrone::ADrone()`에 fixed object finder를 추가한다.

```cpp
#include "Engine/DataTable.h"
#include "UObject/ConstructorHelpers.h"

static ConstructorHelpers::FObjectFinder<UDataTable> CoreTableFinder(
    TEXT("/Game/Data/DroneCombat/DT_DroneCore.DT_DroneCore"));
static ConstructorHelpers::FObjectFinder<UDataTable> WeaponTableFinder(
    TEXT("/Game/Data/DroneCombat/DT_DroneWeapon.DT_DroneWeapon"));
DroneCoreDataTable = CoreTableFinder.Object;
DroneWeaponDataTable = WeaponTableFinder.Object;
```

runtime에서 `FillDataTableFromCSV`나 CSV 파일 읽기는 추가하지 않는다.

- [x] **Step 6: shipping asset GREEN 확인**

Build 후 POR25 direct suite를 실행한다. Expected: 7/7 성공, 두 asset row struct·CDO reference·canonical 전 필드 동등성 성공.

- [x] **Step 7: data/asset 커밋**

```powershell
git add Data/DroneCombat Content/Data/DroneCombat Source/DroneProto/Drone.cpp Source/DroneProto/Tests/DronePartInventoryTests.cpp
git diff --cached --check
git commit -m "feat: POR-25 코어 무기 데이터테이블 에셋 추가"
```

### Task 5: 최종 검증과 canonical 문서

**Files:**
- Modify local canonical: `docs/Audit/ImplementationMap_Current.md`
- Modify local canonical: `docs/DEVLOG.md`
- Modify: `AGENTS.md`
- Track explicitly: `docs/Audit/POR25_CoreWeaponDataTable_ImplementationPlan_20260811.md`

**Interfaces:**
- Consumes: 실제 커밋 hash, 자동화 발견 수/성공 수, Build와 asset 결과
- Produces: POR-25 Linear-ready 완료 요약; 상태는 `In Progress` 유지

- [x] **Step 1: 직접·인접 suite 재실행**

순서대로 실행하고 실제 발견 수와 성공 수를 기록한다.

```text
DroneProto.POR25.CombatDataTable => expected 7/7
DroneProto.D7.Drone => expected 4/4
DroneProto.D9.Drone => expected 2/2
DroneProto.D13.DroneCombat.SpecAlignment => expected 1/1
DroneProto.D20.LogSemantics.NoDamageAttack => expected 1/1
```

- [x] **Step 2: UE 5.7 Editor Build**

Task 1의 canonical Build 명령을 실행한다. Expected: exit 0.

- [x] **Step 3: 전체 DroneProto 회귀**

Core/Weapon은 공유 피해·이속·회복 계산을 바꾸므로 전체 회귀를 실행한다.

```powershell
& 'D:\Programs\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject' -unattended -nop4 -nosplash -NullRHI "-ExecCmds=Automation RunTests DroneProto; Quit" "-TestExit=Automation Test Queue Empty" -log
```

Expected: 기존 132개 + 신규 7개를 포함한 실제 발견 수 전부 성공, 실패 0. 숫자가 다르면 로그의 실제 발견 수를 canonical 기록에 사용한다.

- [x] **Step 4: XLSX/CSV/asset 최종 동등성 확인**

artifact inspection으로 XLSX Core/Weapon 3+3 rows를 다시 읽고 CSV literal과 대조한다. POR25 `AssetCanonicalEquality` 결과와 `[DR_SUMMARY] DroneCombatData Source=DataTable` 로그를 함께 확인한다. 예상치 못한 fallback 로그는 0건이어야 한다.

- [x] **Step 5: canonical 문서 갱신**

`ImplementationMap_Current.md`의 현재 작업 추적과 `CORE-01`~`CORE-05`, `WEAPON-01`~`WEAPON-05`만 실제 함수·테스트·커밋으로 갱신한다. `DEVLOG.md`에는 scope, RED 원인, GREEN commands/counts, CSV/assets, fallback, GUI PIE 미실행을 append한다. `AGENTS.md`는 POR-25 current state와 다음 DataTable 후보를 실제 결과에 맞춰 갱신한다.

- [x] **Step 6: 정적 검사와 문서 커밋**

```powershell
git diff --check
git status --short --branch
git add AGENTS.md
git add -f docs/Audit/POR25_CoreWeaponDataTable_ImplementationPlan_20260811.md
git diff --cached --check
git commit -m "docs: POR-25 전투 데이터테이블 검증 기록"
```

`ImplementationMap_Current.md`와 `DEVLOG.md`는 `docs/*` ignore 정책에 따라 로컬 canonical 문서로 유지하고, Git 정책을 바꾸지 않는다.

- [x] **Step 7: Linear-ready 요약 준비**

POR-25에 변경 파일, 직접/인접/전체 테스트 수, Build, asset import와 canonical 동등성, GUI PIE 미실행 이유, 남은 경계를 적을 summary를 준비한다. 상태는 `In Progress`로 유지한다.
