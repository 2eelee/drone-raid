# POR-25 Core/Weapon DataTable 런타임 연결 설계

## 목표

최신 원본 `docs/sources/현현_데이터테이블.xlsx`의 `Core`, `Weapon` 시트를 기존 서버 권한 전투 계산의 단일 밸런스 데이터 소스로 연결한다. 기존 공격·이동·회복 계산식과 권한 구조는 유지하고, 하드코딩된 canonical 수치만 검증된 typed DataTable config로 대체한다.

## 확정 범위

- `Core` 3행과 `Weapon` 3행을 하나의 전투 config로 함께 검증한다.
- 둘 중 하나라도 누락·구조 오류·행 누락/추가·식별자 불일치·효과 타입 불일치·수치 범위 오류면 둘 다 기존 canonical 상수 전체로 원자 fallback한다.
- Core의 기본 공격/이속 배율과 특수효과 수치, Weapon의 기본 피해·특수효과 수치·HitCount를 연결한다.
- 원본 XLSX 값과 형식은 이미 현행 기획과 일치하므로 수정하지 않는다.
- 원본에서 canonical CSV와 typed DataTable asset을 만들고 `ADrone` CDO hard reference로 연결한다.
- UI 표시명, UMG, 맵, Blueprint 레이아웃, RPC, Replication, authority, 런타임 상태는 변경하지 않는다.
- `PartCount`와 중간 우선순위 DataTable 후보, POR-6 실서버 검증, `TELEGRAPH-01` VFX는 제외한다.

## 원본 계약

### Core

| Row | CoreID | CoreType | AttackModifier | MoveSpeedModifier | EffectType | EffectValue01 | EffectValue02 | EffectMaxValue |
|---|---:|---|---:|---:|---|---:|---:|---:|
| `CORE_001` | 1001 | Zenith | 1.00 | 1.00 | `HP_TO_ATTACK` | 0.02 | 0.10 | 0.20 |
| `CORE_002` | 1002 | Booster | 0.95 | 1.00 | `MOVE_TO_SPEED_ATTACK` | 0.03 | 20.0 | 0.30 |
| `CORE_003` | 1003 | Drain | 0.85 | 0.90 | `DAMAGE_TO_HEAL` | 0.12 | 3.0 | 0.0 |

### Weapon

| Row | WeaponID | WeaponType | BaseDamage | SpecialEffectType | SpecialValue01 | SpecialValue02 | SpecialMaxValue | HitCount |
|---|---:|---|---:|---|---:|---:|---:|---:|
| `WEAPON_001` | 2001 | Pulse | 8.0 | `THIRD_HIT_STRONG` | 3.0 | 18.0 | 0.0 | 1 |
| `WEAPON_002` | 2002 | Fracture | 5.0 | `FRACTURE_MULTI_HIT` | 3.0 | 2.0 | 0.0 | 4 |
| `WEAPON_003` | 2003 | Vector | 7.0 | `MOVE_DISTANCE_DAMAGE` | 5.0 | 1.0 | 8.0 | 1 |

`CoreName`과 `WeaponName`은 이번 런타임 계산 config에 넣지 않는다. 현행 부품 표시 메타데이터 경로를 유지해 UI 범위와 전투 밸런스 범위를 분리한다.

## 런타임 구조

1. `FDroneCombatResolvedConfig`가 Core 3종과 Weapon 3종의 런타임 규칙을 소유한다.
2. `FDroneCombatRules::MakeCanonicalConfig`가 현재 하드코딩 수치와 동일한 fallback config를 만든다.
3. 신규 `DroneCombatDataTableResolver.*`가 두 `UDataTable`의 row struct, 정확한 행 이름·개수, ID, 효과 타입, 유한값과 수치 범위를 함께 검증한다.
4. resolver는 완성된 candidate만 반환한다. 실패 중간값은 외부에 노출하지 않는다.
5. `ADrone`은 Core/Weapon DataTable CDO hard reference를 보유한다. 서버 인스턴스가 처음 계산 config를 요구할 때 두 표를 한 번 resolve하고 결과 또는 canonical fallback을 캐시한다.
6. `CalculateWeaponDamageForServer`, `CalculateCoreForServer`, `RefreshMoveSpeedForServer`, `ApplyDrainHealForServer`는 같은 cached config를 사용한다.
7. 순수 계산 테스트를 위해 `FDroneCombatRules::CalculateWeaponDamage`와 `CalculateCoreBonus`, `CalculateDrainHeal`은 resolved config를 명시적으로 받는 overload를 제공하고, 기존 호출 호환 경로는 canonical config를 사용한다.

기존 enum 식별, part selection, loadout, 서버 공격 처리, 보스 피해 적용과 이동 authority guard는 그대로 재사용한다.

## 검증 및 fallback

검증 실패 이유는 table 누락, row struct 오류, 필수 행 누락, 예상 밖 행, ID/type/effect 불일치, 중복 runtime type, 잘못된 수치 범위로 구분한다.

- 모든 배율·피해·효과 수치는 finite이며 음수가 아니다.
- Core/Weapon ID와 row name은 정확한 canonical 쌍이어야 한다.
- 각 runtime type은 정확히 한 번만 나타나야 한다.
- Pulse의 trigger count와 Fracture의 추가 hit count, 모든 HitCount는 양의 정수 계약을 만족해야 한다.
- Booster distance step과 Vector distance step은 0보다 커야 한다.
- 비율·배율·상한은 원본 의미에 맞는 범위를 만족해야 한다.

성공 로그는 `[DR_SUMMARY] DroneCombatData Source=DataTable`, 실패 로그는 `[DR_SUMMARY] DroneCombatData Source=Fallback Reason=<reason>`으로 한 인스턴스당 한 번만 남긴다. 로그는 감사 수단이며 계산 권한은 cached config에 있다.

## 데이터와 에셋 경로

- 원본: `docs/sources/현현_데이터테이블.xlsx`
- CSV: `Data/DroneCombat/DroneCore.csv`, `Data/DroneCombat/DroneWeapon.csv`
- Asset: `/Game/Data/DroneCombat/DT_DroneCore`, `/Game/Data/DroneCombat/DT_DroneWeapon`

asset은 C++ CDO hard reference로 cook reachability를 유지한다.

## 테스트 설계

RED에서 먼저 증명할 동작:

1. transient Core/Weapon table의 변경 수치가 실제 Core/Weapon 계산 결과를 바꾼다.
2. 둘 중 하나가 잘못되면 유효한 다른 표도 사용하지 않고 canonical 전체 fallback한다.
3. 필수 행·식별자·효과 타입·범위 오류가 deterministic reason으로 구분된다.
4. 한 `ADrone` 인스턴스가 같은 cached config를 공격·이속·Drain 회복에 공유한다.
5. shipping asset 두 개가 올바른 row struct와 canonical 전 필드 값을 가진다.

GREEN 검증:

- 신규 `DroneProto.POR25.CombatDataTable` 직접 suite.
- 기존 Core/Weapon 계산과 실제 공격 경로 직접 suite.
- UE 5.7 Editor Build.
- 공유 피해·이속 계산 경로를 건드리므로 전체 `DroneProto` 회귀.
- XLSX/CSV/typed asset canonical 동등성 및 Unreal CSV import 0 Problems.
- UMG·맵·Blueprint를 변경하지 않으므로 신규 GUI PIE는 필수가 아니다. 시각 체감은 기존 수치와 동일해 별도 완료 근거로 사용하지 않는다.

## 완료 기록

- 실제 검증 후 `ImplementationMap_Current.md`의 현재 작업 추적과 Core/Weapon 관련 행만 갱신한다.
- 실행한 명령, 실제 발견 테스트 수와 결과를 `docs/DEVLOG.md`에 append한다.
- 다음 단계나 현재 상태가 바뀌면 `AGENTS.md`를 같은 작업에서 갱신한다.
- Linear `POR-25`에는 완료 요약만 준비하고, 사용자 승인 전 `Done`으로 변경하지 않는다.
