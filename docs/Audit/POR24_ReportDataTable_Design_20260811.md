# POR-24 DroneReport Bonus/Grade DataTable 런타임 연결 설계

## 목표

`REPORT-DATA-01`의 `SpecMismatch`를 해소한다. 최신 원본 `docs/sources/현현_데이터테이블.xlsx`가 Bonus 점수·조건·상한과 Grade 임계값의 canonical source가 되고, 서버 권한 DroneReport 생성 경로가 그 값을 원자적으로 사용한다.

## 확정 범위

- 원본 XLSX의 `Bonus` 시트를 확장해 현재 코드에만 있는 2단계 점수와 특수 조건을 모두 표현한다.
- 원본 XLSX에 단일 전역 보너스 상한을 담는 `DroneReportSettings` 시트를 추가한다.
- `Grade` 시트는 기존 S/A/B/C 행과 값 구조를 유지한다.
- 세 시트에서 canonical CSV와 typed DataTable asset을 만든다.
- Bonus/Settings/Grade 중 하나라도 누락·중복·잘못된 타입·범위이면 세 표를 부분 혼합하지 않고 기존 검증 상수 전체로 fallback한다.
- Core/Weapon 및 중간 우선순위 DataTable, 기본 성과 점수식, UMG 배치, 맵, 신규 RPC/복제는 범위에서 제외한다.

## 원본 XLSX 스키마

### Bonus

기존 열 `Row Name`, `BonusID`, `BonusName`, `BonusDisplayName`, `BonusScore`, `MinCombatDuration`, `MinBossDamageRatio`, `MaxScore`를 유지한다. `BonusID`는 원본 숫자 ID `5001`~`5005`에 맞춰 C++도 `int32`로 정합화한다.

다음 열을 추가한다.

| 열 | 의미 |
|---|---|
| `SecondaryBonusScore` | 낮은 2단계 조건 점수. 없는 보너스는 0 |
| `SecondaryMinCombatDuration` | 2단계 최소 전투 시간 |
| `SecondaryMinBossDamageRatio` | 2단계 최소 보스 피해율 |
| `PrimaryMinDamagePerMinute` | High DPS 기본 점수 조건 |
| `SecondaryMinDamagePerMinute` | High DPS 2단계 조건 |
| `PrimaryMinMoveDistance` | Keep Moving 기본 점수 조건 |
| `SecondaryMinMovePerMinute` | Keep Moving 2단계 조건 |
| `PrimaryMinHealAmount` | High Recovery 기본 점수 조건 |
| `SecondaryMinHealAmount` | High Recovery 2단계 조건 |
| `LateJoinBossHPThresholdRatio` | Boss Slayer 막판 입장 판정 비율 |
| `MaxDamageTakenCount` | No Damage 허용 피격 횟수. 비적용 행은 -1 |
| `RequiresBossDefeated` | 보스 처치가 필요한지 |
| `RequiresAlive` | 리포트 시 생존이 필요한지 |

현재 기획 수치를 다음처럼 표현한다.

- Boss Slayer: 80점, 60초/3%; 막판 입장 40점, 30초/1%, 입장 HP 25% 이하; 보스 처치·생존 필요.
- High DPS: 70점, 분당 3%; 40점, 분당 2%; 양쪽 모두 30초/1.5% 필요.
- No Damage: 50점, 60초/2%, 피격 0회.
- Keep Moving: 50점, 60초/2%와 500m; 40점, 30초/1.5%와 분당 150m.
- High Recovery: 50점, 회복 40; 30점, 회복 25; 양쪽 모두 60초/1.5%와 생존 필요.

### DroneReportSettings

단일 행 `REPORT_SETTINGS`에 `BonusScoreCap=250`을 둔다. 전역 상한을 Bonus 행마다 중복 저장하지 않는다.

### Grade

기존 `GRADE_S/A/B/C`의 `Grade`, `MinScore`, `MaxScore`를 유지한다. 런타임 판정은 기존 동작처럼 내림차순 `MinScore`를 사용하고, `MaxScore`는 표 범위 검증에 사용한다. 부동소수점 `ReportScore`를 정수로 반올림하지 않는다.

## 런타임 구조

1. `FDroneReportResolvedConfig`는 Bonus 5종 규칙, 보너스 총 상한, Grade 4종 범위를 가진다.
2. 신규 resolver가 세 `UDataTable`의 row struct, 필수 행, ID·이름 매핑, 중복, 수치 범위를 검사한다.
3. `ARaidPlayerController`는 기존 report 생성 서버 경로에서 resolver를 최초 1회 실행하고 결과를 캐시한다.
4. 성공 시 DataTable config, 실패 시 canonical fallback config 전체를 사용한다. 같은 report 계산 안에서 source를 섞지 않는다.
5. `FDroneReportRules::BuildReportData`는 resolved config를 받아 기존 기본 성과 점수식과 분기 순서를 유지하면서 Bonus/Grade 수치만 config에서 읽는다.
6. 서버는 달성 enum과 함께 `BonusDisplayName` 결과를 기존 `Client_ReceiveDroneReport`의 `FDroneReportData`에 넣는다.
7. `UDroneReportWidget`은 서버가 보낸 표시명을 사용한다. 이전/빈 데이터만 기존 enum 표시명 fallback을 사용한다.

새 RPC, replicated property, 병렬 계산 시스템은 추가하지 않는다.

## 에셋 및 소스 경로

- 원본: `docs/sources/현현_데이터테이블.xlsx`
- CSV: `Data/DroneReport/DroneReportBonus.csv`, `Data/DroneReport/DroneReportSettings.csv`, `Data/DroneReport/DroneReportGrade.csv`
- Asset: `/Game/Data/DroneReport/DT_DroneReportBonus`, `/Game/Data/DroneReport/DT_DroneReportSettings`, `/Game/Data/DroneReport/DT_DroneReportGrade`

DataTable asset은 C++ CDO hard reference로 cook reachability를 유지한다.

## 오류 처리와 로그

- 성공: `[DR_SUMMARY] DroneReportData Source=DataTable`
- 실패: `[DR_SUMMARY] DroneReportData Source=Fallback Reason=<deterministic reason>`
- fallback reason은 누락 table, 잘못된 row struct, 필수 행 누락/추가, ID·이름 불일치, 중복 bonus/grade, 잘못된 범위로 구분한다.
- 로그는 감사 수단이며 계산 권한은 서버 resolved config에 있다.

## 테스트 설계

RED에서 먼저 증명할 동작:

1. transient Bonus/Settings/Grade table의 변경 점수가 실제 report Bonus와 Grade 결과를 바꾼다.
2. Bonus 표시명이 서버 report payload를 통해 widget에 표시된다.
3. 세 표 중 하나가 잘못되면 유효한 두 표도 사용하지 않고 canonical 전체 fallback한다.
4. shipping asset 3개가 올바른 row struct와 canonical 값을 가진다.

GREEN 후 검증:

- 신규 `DroneProto.POR24.ReportDataTable` 직접 suite.
- 확인된 caller 범위인 `DroneProto.Q5.DataTable`, `DroneProto.D10.DroneReport`, `DroneProto.D11.DroneReport`.
- `.h/.cpp` 변경이므로 UE 5.7 Editor Build.
- GameMode, 공유 재고, 피해, 복제, 접속 생명주기를 변경하지 않으므로 전체 `DroneProto` 회귀는 기본적으로 불필요하다. 구현 중 기존 RPC payload 직렬화 경계가 직접 suite로 닫히지 않으면 그때 가장 작은 추가 suite부터 확대한다.
- DataTable/asset의 최종 runtime source 로그와 표시명은 자동화로 증명하며, UMG 배치 변경이 없으므로 신규 GUI PIE는 필수가 아니다.

## 완료 기록

- 실제 검증 뒤 `ImplementationMap_Current.md`의 현재 작업 추적과 `REPORT-DATA-01`만 갱신한다.
- 실제 명령·발견 테스트 수·결과를 `docs/DEVLOG.md`에 append한다.
- 최우선 다음 단계가 바뀌면 `AGENTS.md`를 같은 작업에서 갱신한다.
- Linear `POR-24`는 결과 요약을 남길 준비만 하고, 사용자 승인 전 `Done`으로 변경하지 않는다.
