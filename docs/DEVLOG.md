# DEVLOG — DroneProto

서버 권한 기반 드론 조립 PvE MMORPG 프로토타입 개발 기록.

---
## 2026-07-08 — 기획서 전문 감사 + 문서 체계 재배치 + 작업큐 생성 (branch docs/spec-audit-20260708)

### 목적
docs/sources 기획서 원문 전체를 코드 대비 감사하고, 에이전트가 항상 참조 가능한 문서 체계로 재배치. 누락표 기준 다음 작업 큐 생성.

### 읽은 기획서 (전문 판독, 11종)
플로우/코어루프, 클래스GameManager(수정중), 레이드입장, 드론부품, 드론부품선택, 드론부품환원, 드론 이동/회피, 드론 타겟팅, 보스, DroneReport (docx 10) + 현현_데이터테이블.xlsx 16시트. **보스 패턴/전투조작/튜토리얼 상세 기획서는 원문 부재** — 해당 영역은 SpecDecisionNeeded 유지.

### 생성/갱신/이동 문서
- 신규: `docs/Audit/ImplementationGap_CurrentSpec_20260708.md` (누락표, 이전 감사 대체), `docs/Audit/NextWorkQueue_CurrentSpec_20260708.md` (작업 큐 + 상위 5개 Codex 프롬프트), `docs/AI/DRONERAID_DOC_MAP_20260708.md` (문서맵)
- 이동: 감사/계획 문서 6종 → `docs/Audit/` (ImplementationAudit 3종, DRONERAID_FABLE_AUDIT_20260707, Fable_Final5HourPlan_20260707, D4_D5_SHARED_INVENTORY_TEST)
- 갱신: `AGENTS.md` 상단에 "문서맵 (작업 전 필독)" 섹션 추가
- 삭제 없음. 기획서 원문 유지.

### 누락표 요약 (Complete 9 / Partial 4 / SpecMismatch 2 / 기획부재 1)
- **Critical**: 보스 MaxHP 1000 vs 기획 60,000 (RaidBoss.h:116). push 미실행(로컬 유일본).
- **High**: 코어 기본 배율 no-op — 데이터테이블 AttackModifier(Booster 0.95/Drain 0.85)·MoveSpeedModifier(Drain 0.9) 미적용, CoreAttackModifier 항상 1.0.
- **Medium**: RaidState Drafting 미사용(Waiting→Battle 직행), BossState enum(Spawn/Battle/Dead/Clear) 부재+입장 차단 게이트 불명시, 맵로드/스폰 실패 ReturnToLobby TODO 미구현, 레이드 잔여시간 클라 미복제.
- **일치 확인**: 부품 수량 5/6/5/11/10/11, 무기/코어 계산식 전수치, Dodge 4수치, 경계 50m/보스 800cm, 카메라 4수치, Report 점수식/보너스/등급/late join 보정 — 전부 기획서·데이터테이블과 일치.
- **문서가 낡음(코드 아님)**: CLAUDE.md 보류 목록의 이동 동기화 TODO(해소됨), Booster/Drain/Vector 효과(구현 완료), IsSlotEnabled dead(호출 체인 존재).

### 다음 작업 큐 Top 5 (상세·Codex 프롬프트는 NextWorkQueue 문서)
0. (오너 수동) push + 잔여 PIE 검증(보스 패턴 실피격) — 코드 작업 선행 게이트
1. 보스 MaxHP 60,000 정합 (+테스트 기대값)
2. 코어 기본 배율 적용 (0.95/0.85/이속 0.9)
3. RaidState Drafting 정식화
4. BossState enum + 신규 입장 차단 게이트
5. DataTable 전환 1단계 (스키마+CSV+fallback)

### gitignore 처리
- `.gitignore` 선택 (`.git/info/exclude` 아님 — 기획서 바이너리 커밋 방지는 저장소 전체 규칙이어야 하고 규칙 자체가 버전 관리되어야 함). 기존 `docs/*` + `!docs/DEVLOG.md` 규칙 유지, `!docs/D4_D5_SHARED_INVENTORY_TEST.md` 예외 제거.
- `git rm --cached docs/D4_D5_SHARED_INVENTORY_TEST.md` (index에서만 제거, 파일은 docs/Audit/로 이동 보존).
- tracked 유지: README.md(필수), AGENTS.md(에이전트 규칙, 팀 공유), docs/DEVLOG.md(canonical 기록, 팀 공유). 나머지 docs/ 문서 전부 로컬 전용(ignore).

### 커밋
- pending commit (이 항목 작성 시점). 커밋 후 해시는 git log 참조.
- 검증: 빌드/자동화 재실행 없음(문서 전용 변경, C++/Config/에셋/테스트 무변경).

---
## 2026-07-08 — Post-commit 2 Client PIE 로그 확인: Target Marker Visible, BossStunChanged true/false, StunMultiplier=1.50, BossDead 후 BossPattern Stopped 및 Target Clear 확인. 실제 BossAttackHit/DroneDamaged는 OutOfRange만 발생해 별도 미검증.

---
## 2026-07-08 — POR-16~19 통합 커밋 봉인 (6f5bfd2)

### 작업

- 미커밋 13파일(+1522/−75)을 커밋 전 안전검산했다. 내용물은 4계열 혼재: POR-16 타겟팅 / POR-17 이동 클램프(보스 최소접근 500→800cm) / POR-18 방어 3종 / POR-19 보스 패턴·스턴.
- 기존 "2분할 커밋" 계획(07-07 계획 ①방어3종+Destroyed / ②패턴+스턴)은 **폐기**. `ARaidBoss::EndPlay/Destroyed`(POR-18)가 `StopBossPatternForServer`(POR-19)를 같은 hunk에서 호출해 ①만으로는 컴파일 불가. hunk 수동 편집으로 억지 분리하면 64/64가 검증한 코드와 다른 중간 커밋이 생기므로 배제.
- 검증된 스냅샷 그대로 1커밋 `6f5bfd2`로 봉인. 커밋 메시지에 분할 불가 사유와 SpecDecisionNeeded placeholder를 기록. 워킹 트리 클린.

### 검증

- 커밋 전 검산은 git diff/log 기반 정적 분석(이 세션에서 빌드/자동화 재실행 없음). 64/64(EXIT CODE 0) 근거는 2026-07-07 세션 기록(`docs/Fable_Final5HourPlan_20260707.md` §7).
- 구 테스트와 신 코드의 교차 회귀 후보 3건을 정적으로 배제: BossDamage 로그 포맷 변경(테스트는 값 검사만), `ResetDroneReportForTest`(구 테스트 미사용), 500cm 기대 테스트(변경이 같은 커밋에 동승).

### 남은 것

- **push 미실행** — 원격 미반영, 로컬 유일본 상태.
- **2 Client PIE 수동 검증 미실행** — 클라 OnRep 계열(타겟 마커 표시/숨김, 스턴 visual)은 자동화 미커버. 검색어 목록은 `Fable_Final5HourPlan_20260707.md` §7 참조.
- **README 최신화 필요** — D5 기준으로 낡음, POR-19까지 반영해야 함.

---
## 2026-07-06 — POR-14/POR-15 전투 가시화 및 로그 의미 정리

### 작업

- 전투 상황을 PIE에서 보기 쉽게 CombatVisual C++ 훅을 추가했다.
- 공격/보스 피격/드론 피격/텔레그래프 표시용 visual-only 이벤트와 DR_SUMMARY 로그를 정리했다.
- 0 damage 공격은 Attack NoDamage로 분리하고, 실제 HP 변화가 있는 경우만 BossDamage로 남기도록 로그 의미를 정리했다.
- 이동 계열 반복 로그를 줄이고, PIE 확인용 체크리스트 로그를 추가했다.
- AGENTS.md에 Codex/Linear 보고 규칙과 DroneRaid 작업 제한/명명 규칙을 추가했다.

### 검증

- Build.bat DroneProtoEditor Win64 Development -Project="D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject" -NoLiveCoding -WaitMutex 성공.
- Automation RunTests DroneProto.D20.LogSemantics.NoDamageAttack 성공.
- Automation RunTests DroneProto.D19.CombatVisual.Hooks 성공.
- Automation RunTests DroneProto.D13.DroneCombat.SpecAlignment 성공.
- Automation RunTests DroneProto.D16.Drone 성공.
- Automation RunTests DroneProto.D17.Drone 성공.
- Automation RunTests DroneProto.D18.Drone 성공.
- Automation RunTests DroneProto.D15.RaidBoss.TelegraphedAreaAttack 성공.
- git diff --check 성공.
- Linear POR-15에 검증 결과 코멘트 작성.

### PIE 검색어

- [DR_SUMMARY] D20PIEChecklist CombatVisual
- [DR_SUMMARY] Attack NoDamage:
- [DR_SUMMARY] BossDamageIgnored: Reason=NoDamage
- [DR_SUMMARY] CombatVisual Attack:
- [DR_SUMMARY] CombatVisual BossDamaged:
- [DR_SUMMARY] Dodge VisualHidden:
- [DR_SUMMARY] Dodge VisualShown:
- [DR_SUMMARY] CombatVisual TelegraphStart:
- [DR_SUMMARY] CombatVisual TelegraphEnd:
- [DR_SUMMARY] CombatVisual DroneDamaged
- [DR_SUMMARY] CombatVisual DroneDamageIgnored:

### 남은 확인

- Dodge visual, Telegraph visual, DroneDamaged/DamageIgnored visual은 최신 PIE에서 수동 확인 필요.

---
## 2026-07-03 - D19 이동거리 효과 감사 / Dodge 체감 후속 보강

### 문제

- Vector Cannon/Booster Core 이동거리 기반 효과와 DroneReport 반영이 기획 수치와 맞는지 재확인이 필요했다.
- PIE에서 `Move Accepted`가 반복 출력되어 수동 검증 로그가 과도하게 많았다.
- Dodge가 0.25초 상태 유지에도 시작 즉시 End 위치로 이동해 순간이동처럼 보였다.
- Dodge 무적 구간을 플레이어가 인지할 수 있도록 사라짐/재등장 visual hook이 필요했다.

### 수정

- Vector Cannon 5m당 +1, 최대 +8 및 Booster Core 누적 이동거리 상한을 자동화 테스트로 고정했다.
- CombatRecord/DroneReport가 서버 기준 이동거리, 보스 피해, 피격 횟수를 복사하는지 검증을 추가했다.
- `Move Accepted` 로그를 이동 시작, 의미 있는 축 변경, 1초 요약 기준으로 제한했다.
- Dodge 시작 시 즉시 End로 이동하던 `SetActorLocation(FinalLocation)` 흐름을 제거하고, 서버 Tick에서 Start/Target을 0.25초 동안 보간한다.
- Dodge 보간 중 실제 `Distance2D`만 Vector/Booster/CombatRecord 이동거리로 누적한다.
- `InvincibleDuration=0.15s` 구간에 visual-only multicast와 `BP_OnDodgeInvincibleVisualChanged` 훅을 추가했다.
- C++ 기본 visual은 visible `UPrimitiveComponent`만 숨기고, 우리가 숨긴 컴포넌트만 `InvincibleEnd`에 복구한다.
- dedicated server에서는 visual 적용을 건너뛰고, 서버 gameplay 판정과 `bIsInvincible` 복제 정책은 바꾸지 않았다.

### 검증

- `Build.bat DroneProtoEditor Win64 Development -Project="D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject" -NoLiveCoding -WaitMutex` 성공.
- `Automation RunTests DroneProto.D13.DroneCombat.SpecAlignment` 성공.
- `Automation RunTests DroneProto.D9.Drone.VectorBoosterCombatRecord` 성공, 1 test.
- `Automation RunTests DroneProto.D16.Drone` 성공, 4 tests.
- `Automation RunTests DroneProto.D17.Drone` 성공, 3 tests.
- `Automation RunTests DroneProto.D18.Drone` 성공, 6 tests.
- `git diff --check` 공백 오류 없음. Windows LF/CRLF 경고만 확인.

### PIE 검색어

- `[DR_SUMMARY] Move Accepted:`
- `[DR_SUMMARY] WeaponCalc`
- `[DR_SUMMARY] CoreCalc`
- `[DR_SUMMARY] AttackCalc`
- `[DR_SUMMARY] CombatRecord`
- `[DR_SUMMARY] Dodge Started:`
- `[DR_SUMMARY] Dodge VisualHidden:`
- `[DR_SUMMARY] Dodge InvincibleEnd`
- `[DR_SUMMARY] Dodge VisualShown:`
- `[DR_SUMMARY] Dodge End:`

### SpecDecisionNeeded

- 현재 Dodge visual은 C++ 기본 visibility toggle이다. 투명 머티리얼, fade, VFX는 추후 Blueprint/에셋 범위에서 붙인다.

---
## 2026-07-01 - D18.5 Fixed Boss-Facing Camera 화면 기준 입력 변환

### 문제

Fixed Boss-Facing Quarter View Camera와 RaidBoss 시각화/타겟 판정은 정상화되었지만, PIE 수동 확인에서 조작감 문제가 남았다.

- 카메라는 보스를 바라보며 플레이어 위치에 따라 360도 회전한다.
- 기존 이동/회피 입력은 월드 기준 축 또는 Controller yaw 기준 재해석에 묶여 있어, 카메라가 돌 때 방향키 체감이 계속 바뀌었다.
- 사용자는 "월드 기준 방향은 일정하지만 카메라가 돌아서 헷갈린다"고 확인했다.
- 이동/회피 서버 권한 구조는 유지하면서, 로컬 클라이언트의 입력 요청만 화면/카메라 기준으로 변환해야 했다.

### 수정

- `ADrone::ConvertScreenInputToWorldMoveDirection()`을 추가해 RawAxis를 현재 카메라 forward/right 기준 XY 평면 world direction으로 변환했다.
- 입력 기준은 `RawAxis.X = 화면 오른쪽/왼쪽`, `RawAxis.Y = 화면 위/아래`로 고정하고, 대각선은 normalize한다.
- `ADrone::Move()`는 로컬 입력 RawAxis를 `ConvertLocalScreenInputToWorldMoveDirection()`으로 변환한 뒤 기존 `Server_SetMoveInput` 경로로 전달한다.
- `ADrone::Dodge()`는 마지막 이동 입력에서 변환/캐시된 world direction을 사용해 기존 `Server_RequestDodge` 경로로 전달한다.
- 서버는 카메라 값을 신뢰하거나 복제하지 않고, 클라이언트가 요청한 world XY 벡터를 다시 clamp/normalize해서 최종 이동/회피 판정에만 사용한다.
- 서버 이동/회피 적용부에서 `ControlRotation` 기반 재회전을 제거해, RPC로 들어온 world XY 요청 벡터가 한 번 더 회전하지 않도록 했다.
- `[DR_SUMMARY] MoveInputConverted`와 `[DR_SUMMARY] DodgeInputConverted` 로그를 추가했다.
- 입력 변환 로그는 RawAxis 변경 시 즉시 출력하고, 같은 키 유지 중 카메라 yaw 변화로 생기는 반복 로그는 1초 throttle로 제한했다.
- `Move BossMinClamp` 반복 로그도 같은 플레이어/상황에서 1초 throttle로 줄였다.
- D16/D17 테스트에 서버 move/dodge 입력이 Controller yaw와 독립적인 world XY 요청 벡터로 처리되는 회귀 테스트를 추가했다.
- D18 테스트에 카메라 yaw 0/90/180/-90 기준 화면 입력 변환 순수 함수 테스트를 추가했다.

### 검증

- `Build.bat DroneProtoEditor Win64 Development -Project="D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject" -NoLiveCoding -WaitMutex` 성공.
- `Automation RunTests DroneProto.D16.Drone` 성공, 3 tests found, 3 Success, exit code 0.
- `Automation RunTests DroneProto.D17.Drone` 성공, 3 tests found, 3 Success, exit code 0.
- `Automation RunTests DroneProto.D18.Drone` 성공, 6 tests found, 6 Success, exit code 0.
- PIE 수동 검증에서 보스가 보이는 상태로 카메라는 보스를 계속 바라보고, 이동/회피 입력 체감은 화면 기준으로 통과 확인했다.

### PIE 검색어

- `[DR_SUMMARY] MoveInputConverted`
- `[DR_SUMMARY] DodgeInputConverted`
- `[DR_SUMMARY] Camera BossFacing`
- `[DR_SUMMARY] Move Accepted`
- `[DR_SUMMARY] Dodge Accepted`
- `[DR_SUMMARY] Move BossMinClamp`

### SpecDecisionNeeded

- 현재 C++ helper의 입력 규약은 `RawAxis.X = 화면 좌우`, `RawAxis.Y = 화면 상하`다. 추후 Enhanced Input asset의 Vector2D 축 매핑을 바꾸면 이 규약과 함께 확인해야 한다.
- 화면공간 타겟 위치 보정은 여전히 C++ hook만 있고, 실제 UMG/uasset/umap/카메라 솔버 작업은 별도 범위로 남겨둔다.

---
## 2026-07-01 — D15.5/D16/D17/D18 보스 텔레그래프, 이동/회피, 카메라/보스 시각화 정리

### 문제

보스 레이드 전투 수동 검증을 위해 서버 권한 이동, 회피, 보스 공격 예고, 고정 쿼터뷰 카메라가 이어져야 했지만, 각 기능의 C++ 경로와 검증 로그가 아직 한 묶음으로 정리되지 않았다.

- 보스 공격은 즉시 범위 판정만 있었고, 예고 표시 후 지연 판정되는 프로토타입 경로가 필요했다.
- 신규 이동 기획의 기본 이동, 원형 경계, 보스 최소 접근 거리, 이동거리 누적이 서버 권한으로 고정되어야 했다.
- `C + 방향키` 회피는 방향 입력 순간의 축, 6m 이동, 0.25초 상태 유지, 0.15초 무적, 종료 후 1.2초 쿨타임을 서버에서 확정해야 했다.
- Fixed Boss-Facing Quarter View Camera는 로컬 클라 표시 전용이어야 하며, 보이지 않는 C++ `RaidBoss`를 바라보며 카메라가 도는 문제가 있었다.
- PIE 수동 검증은 전체 로그를 읽지 않고 `[DR_SUMMARY]` 검색어로 판정할 수 있어야 했다.

### 수정

- `ARaidBossAttackTelegraph`를 추가해 서버가 보스 공격 예고 actor를 생성하고, delay 만료 뒤 기존 `PerformDebugAreaAttackForServer()` 경로로 판정을 실행하게 했다.
- `ARaidPlayerController::DebugTriggerBossTelegraphAttack()`과 debug-only server RPC를 추가해 PIE에서 보스 예고 공격을 수동 트리거할 수 있게 했다.
- `ADrone` 서버 Tick에서 X/Y 평면 이동만 적용하고, 입력 축 normalize, Z 고정, 50m 원형 경계 clamp, 보스 중심 5m push-out, 이동거리 Distance2D 누적을 처리하도록 보강했다.
- 이동 가능 조건은 서버에서 `InBattle + Alive + Battle RaidState + Report/Loading/Dodge 아님`으로 검증하고, `Move Accepted`, `BoundaryClamp`, `BossMinClamp`, `DistanceReset` 요약 로그를 정리했다.
- `ServerRequestDodge` / `RequestDodgeForServer` 경로에서 회피 가능 조건, 방향 normalize, 경계/보스 최소거리 보정, 실제 이동거리 누적, 무적 타이머, 회피 종료 타이머, 종료 후 쿨타임을 서버 권한으로 처리했다.
- 회피 중 일반 이동과 공격은 각각 `Move Ignored: Reason=Dodging`, `Attack Ignored: Reason=Dodging`으로 거부한다.
- `ADrone`에 로컬 전용 `USpringArmComponent` / `UCameraComponent` 기반 Fixed Boss-Facing Quarter View Camera를 추가했다.
- 카메라는 `GameState` 공식 `RaidBoss`를 우선 사용하고, 공식 보스가 없을 때만 fallback search를 사용한다.
- 카메라용 BossValid는 non-null 포인터가 아니라 `공식 RaidBoss + hidden 아님 + 위치 정상 + VisualReady` 기준으로 강화했다.
- `ARaidBoss` C++ 기본 생성자에 `PrototypeVisualMesh`와 `PrototypeVisualLabel`을 추가해 uasset 수정 없이 원점 보스가 보이도록 했다. Collision은 `NoCollision`으로 유지한다.
- 카메라 fallback yaw는 최초 적용 시점 yaw로 고정해 보스가 없거나 보이지 않을 때 플레이어 이동만으로 카메라가 계속 회전하지 않게 했다.
- 카메라/이동 로그는 상태 변경 또는 throttle 기준으로 제한하고, 보스 타겟 진단에는 `Source`, `BossName`, `BossClass`, `Location`, `MeshVisible`, `VisualReady`, `Reason`을 남긴다.

### 검증

- TDD red 확인:
  - D15 telegraph 테스트에서 `ARaidBossAttackTelegraph`, `StartDebugTelegraphedAreaAttackForServer`, debug trigger RPC 누락 컴파일 실패를 먼저 확인했다.
  - D16 이동 테스트에서 normalize, Z lock, 50m boundary, boss min clamp, 상태별 이동 차단, Distance2D 누적 기대값을 먼저 추가했다.
  - D17 회피 테스트에서 NoDirection/Cooldown/AlreadyDodging/Attacking/Dead/Selecting/Report 차단, 타이머, 무적, 공격 차단, 실제 거리 누적 기대값을 먼저 추가했다.
  - D18 카메라 테스트에서 boss-facing 수학, local-only gate, target source 우선순위, fallback yaw 고정, log throttle, `RaidBoss` visual ready 기대값을 먼저 추가했다.
- `Build.bat DroneProtoEditor Win64 Development -Project="D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject" -NoLiveCoding -WaitMutex` 성공.
- `Automation RunTests DroneProto.D15` 성공, 2 tests found, 2 Success, exit code 0.
- `Automation RunTests DroneProto.D16.Drone` 성공, 3 tests found, 3 Success, exit code 0.
- `Automation RunTests DroneProto.D17.Drone` 성공, 3 tests found, 3 Success, exit code 0.
- `Automation RunTests DroneProto.D18.Drone` 성공, 5 tests found, 5 Success, exit code 0.
- Automation 실행은 현재 로컬 DDC 권한 제약 때문에 `-DDC-ForceMemoryCache`를 붙여 진행했다.

### PIE 검색어

- `[DR_SUMMARY] BossTelegraphStart`
- `[DR_SUMMARY] BossTelegraphSpawned`
- `[DR_SUMMARY] BossAttackExecute Reason=TelegraphExpired`
- `[DR_SUMMARY] BossTelegraphExpired`
- `[DR_SUMMARY] BossTelegraphDebugTrigger`
- `[DR_SUMMARY] Move Accepted:`
- `[DR_SUMMARY] Move Ignored: Reason=`
- `[DR_SUMMARY] Move BoundaryClamp:`
- `[DR_SUMMARY] Move BossMinClamp:`
- `[DR_SUMMARY] Move DistanceReset:`
- `[DR_SUMMARY] Dodge Accepted:`
- `[DR_SUMMARY] Dodge Ignored: Reason=`
- `[DR_SUMMARY] Dodge InvincibleStart`
- `[DR_SUMMARY] Dodge InvincibleEnd`
- `[DR_SUMMARY] Dodge End:`
- `[DR_SUMMARY] Attack Ignored: Reason=Dodging`
- `[DR_SUMMARY] Camera Applied:`
- `[DR_SUMMARY] Camera BossTarget:`
- `[DR_SUMMARY] Camera BossFacing:`
- `[DR_SUMMARY] Camera Fallback:`
- `[DR_SUMMARY] Camera RotationInputDisabled`
- `[DR_SUMMARY] Boss VisualReady:`
- `[DR_SUMMARY] Boss VisualWarning:`

### PIE 판정

- Ready 전 선택 화면에서는 이동/회피가 ignored 로그로 남아야 한다.
- Ready 후 방향키 입력은 서버 기준 `Move Accepted`로 남고, 대각선 이동이 직선보다 빨라지면 안 된다.
- 50m 경계와 보스 중심 5m 안쪽 진입은 각각 boundary/boss clamp 로그로 보정되어야 한다.
- `C`만 누르면 회피하지 않고 `NoDirection`이 남아야 하며, `C + 방향키`는 6m 회피와 실제 이동거리 누적을 남겨야 한다.
- 회피 직후 0.15초 동안만 피해가 무시되고, 이후 0.10초 구간에는 피격 가능해야 한다.
- 회피 중 공격은 `Attack Ignored: Reason=Dodging`으로 거부되어야 한다.
- 보스가 보이는 상태에서만 `Camera BossFacing: BossValid=True`가 남고, 보스가 없거나 보이지 않으면 `Camera Fallback`으로 빠져야 한다.
- C++ `RaidBoss`만 스폰된 상태에서도 `Boss VisualReady`와 `Camera BossTarget ... VisualReady=True`가 보여야 한다.

### SpecDecisionNeeded

- 실제 보스 모델/애니메이션/VFX/UMG HP UI는 아직 에셋 작업 범위로 남긴다.
- 보스 패턴 스케줄러, 텔레그래프 디자인, 공격 쿨다운, 난이도별 패턴 테이블은 별도 기획 결정이 필요하다.
- 카메라 화면공간 보정은 C++ hook만 마련했고, 실제 화면 배치 튜닝은 에디터/플레이 테스트 기준으로 별도 진행한다.

---
## 2026-06-30 — POR-6 ServerState / raid instance availability 분리

### 문제

RaidEntry local prototype은 A/B/C 후보, 16인 정원, wait/retry/timeout/cancel 흐름까지 갖췄지만, raid instance 가용성 판단에 쓰는 값이 기존 `FRaidServerCandidate`의 `CurrentPlayers`, `MaxPlayers`, `bIsOnline`, `bAcceptsPlayers` mirror 필드에 섞여 있었다.

- `RaidState`는 TestMap 내부의 `Waiting / Drafting / Battle / End` 진행 상태인데, pre-travel server availability와 이름/책임이 섞일 위험이 있었다.
- `RaidGameState::CurrentPlayers`는 이미 raid instance에 들어간 뒤의 gameplay state이므로, Lobby/RaidEntry 단계의 capacity source로 쓰면 안 됐다.
- 현재 구현은 실제 backend가 아니라 local assignment prototype이므로, 나중에 backend/server-authoritative availability provider로 바꿔 끼울 수 있는 모델과 평가 함수 경계가 필요했다.
- 기존 RaidEntry 동작인 A -> B -> C 우선순위, `MaxPlayers=16`, `TestMap`, 1초 retry, 10초 timeout, cancel, duplicate request guard는 유지해야 했다.

### 수정

- Lobby 전용 `ERaidServerState`를 추가했다: `Unknown / Online / Offline / Full / Unavailable / Loading / Error`.
- `FRaidServerAvailability`를 추가해 `SlotId`, `CurrentPlayers`, `MaxPlayers`, `ServerState`, `bAcceptsPlayers`, `DebugReason`을 endpoint와 분리해 표현하게 했다.
- `FRaidServerCandidate`는 `Endpoint`와 `Availability`를 함께 들고, 기존 `CurrentPlayers / MaxPlayers / bIsOnline / bAcceptsPlayers` 필드는 호환 mirror로 유지했다.
- `FRaidAssignmentResult`에도 선택된 후보의 `Availability`를 보존해 `RaidAssignmentResult` 로그와 테스트에서 `ServerState`를 확인할 수 있게 했다.
- `ULocalAssignment` 안에서 후보 목록 생성, availability 정규화/평가, A -> B -> C 우선순위 선택, travel 가능 판정을 helper로 분리했다.
- success 조건을 `ServerState == Online`, `bAcceptsPlayers == true`, `CurrentPlayers < MaxPlayers`, valid `TravelTarget`으로 고정했다.
- `Full`, `Offline`, `Unavailable`, `Error`, `Loading`, `Unknown` 후보는 assignment 선택에서 skip한다.
- all full, all offline/unavailable/error 상태는 기존 no-server popup 흐름과 맞춰 `Waiting` + `NoServerAvailable`로 유지했다.
- full slot은 UI request가 waiting 상태로 진입할 수 있도록 `IsSlotEnabled()`에서는 허용하고, offline/unavailable/error는 직접 entry disabled로 남겼다.
- invalid `TravelTarget`은 기존 정책대로 `MapLoadFailed`로 실패 처리한다.
- `[DR_SUMMARY] RaidServerState` 로그를 추가하고, `RaidAssignmentResult` 로그에 `ServerState`와 `AuthorityModel=LocalPrototype`을 남겼다.
- `RaidGameMode`, `RaidGameState`, gameplay RPC, replicated 변수, 전투/부품/반환/Report/Dodge/BossAttack 경로는 수정하지 않았다.

### 검증

- TDD red 확인: `ERaidServerState`, `FRaidServerAvailability`, `FRaidServerCandidate::Availability`, `FRaidAssignmentResult::Availability`를 기대하는 RaidEntry 테스트를 먼저 추가한 뒤 컴파일 실패를 확인했다.
- 회귀 red 확인: full slot이 disabled가 되어 waiting 진입이 막히는 `IsSlotEnabled` 회귀 테스트를 추가하고, `full slot remains enabled so request can enter waiting` 실패를 확인한 뒤 수정했다.
- `Build.bat DroneProtoEditor Win64 Development -Project="D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject" -NoLiveCoding -WaitMutex` 성공.
- `Automation RunTests DroneProto.RaidEntry` 성공, 5 tests found, 5 Success, exit code 0.
- `Automation RunTests DroneProto` 성공, 40 tests found, exit code 0.
- `git diff --check` 성공. LF/CRLF warning 외 whitespace error 없음.
- 전체 자동화 로그에는 UE 초기화 단계의 `LogAutomationTest: Error: Condition failed` 4줄이 남을 수 있지만, automation queue는 `TEST COMPLETE. EXIT CODE: 0`으로 종료됐다.
- Linear `POR-6`에 branch와 검증 결과를 코멘트로 남겼다.

### PIE 검색어

- `[DR_SUMMARY] RaidEntryRequest`
- `[DR_SUMMARY] RaidServerState`
- `[DR_SUMMARY] RaidAssignmentResult`
- `[DR_SUMMARY] RaidEntryWait`
- `[DR_SUMMARY] RaidEntryRetry`
- `[DR_SUMMARY] RaidEntryFail`
- `[DR_SUMMARY] RaidEntryTravel`
- `[DR_SUMMARY] RaidEntryCancel`
- `[DR_SUMMARY] LobbyUIState State=Loading`
- `[DR_SUMMARY] LobbyUIState State=Waiting`
- `[DR_SUMMARY] LobbyUIState State=Main`

### PIE 판정

- 기본 성공 경로에서는 `RaidEntryRequest` 뒤 `RaidServerState Slot=A State=Online`, `RaidAssignmentResult Result=Success Slot=A ServerState=Online AuthorityModel=LocalPrototype`, `LobbyUIState State=Loading`, `RaidEntryTravel Slot=A Target=TestMap`이 이어져야 한다.
- A가 full이면 `RaidServerState Slot=A State=Full` 뒤 B가 online일 때 `RaidAssignmentResult Result=Success Slot=B ServerState=Online`이 남아야 한다.
- A offline, B unavailable, C online이면 A/B는 skip되고 C가 선택되어야 한다.
- A/B/C가 모두 full이면 `RaidEntryWait`와 `LobbyUIState State=Waiting`이 남고, 즉시 `RaidEntryTravel`이 남으면 안 된다.
- all offline/unavailable/error도 현재 no-server popup 흐름과 맞춰 waiting/fail 경로로 이어져야 하며, success travel이 발생하면 안 된다.
- cancel 후에는 `RaidEntryCancel`과 `LobbyUIState State=Main`이 남고, TestMap 내부 `RaidState`는 바뀌면 안 된다.

### SpecDecisionNeeded

- 실제 backend/server-authoritative availability provider의 소유 위치, 갱신 주기, 실패 원인 분류는 아직 미정이다.
- `ERaidServerState::Loading`과 `Unknown`을 UI에서 waiting으로 보여줄지 no-server 계열로 보여줄지는 최종 UX 결정이 필요하다.
- B/C의 실제 dedicated/listen endpoint와 운영 방식은 local prototype 밖의 결정이다.

---
## 2026-06-30 — Drone Report 후 메인 로비 복귀

### 문제

LobbyMap에서 레이드에 입장해 TestMap 전투와 DroneReport 표시까지 이어지는 경로는 연결됐지만, Report 화면에서 사용자가 직접 메인 로비로 돌아가는 C++ 부모 위젯 경로가 없었다.

- `UDroneReportWidget`은 Report 데이터 표시와 텍스트 갱신을 담당하고 있었지만, `WBP_DroneReport` 안의 로비 복귀 버튼을 받을 `BindWidgetOptional` 경로가 없었다.
- Report 후 로비 복귀는 gameplay state를 다시 정리하는 기능이 아니라, 이미 RaidEnd/Death/Logout 경로에서 정리된 뒤 local client가 LobbyMap으로 나가는 UX여야 했다.
- 버튼 연타로 travel 요청이 중복 발생하지 않도록 C++ guard가 필요했다.
- Dedicated server나 non-local controller에서 UI travel이 실행되면 안 됐다.
- 이 작업은 Report 점수/등급/보너스 공식, RaidEnd/Death/Logout 반환, RaidEntry 정책, 전투/부품/Dodge/BossAttack 로직을 바꾸면 안 됐다.

### 수정

- `UDroneReportWidget` C++ 부모에 `ReturnToLobbyButton`을 `BindWidgetOptional`로 추가했다.
- `NativeConstruct()`에서 `ReturnToLobbyButton`이 있으면 `AddUniqueDynamic`으로 클릭 이벤트를 바인딩하고, 없으면 `[DR_SUMMARY] ReportReturnToLobbyButtonMissing` 로그를 남기도록 했다.
- 버튼 클릭은 `RequestReturnToLobby()`로 모아 처리하고, 첫 클릭 이후 `bReturnToLobbyRequested` guard와 버튼 비활성화로 중복 travel 요청을 막았다.
- owning local player UI에서만 LobbyMap으로 `OpenLevel`을 호출하며, target은 `LobbyMap`으로 고정했다.
- invalid world, dedicated server, non-local controller에서는 travel하지 않고 ignored 로그를 남기도록 했다.
- 자동화 테스트에서는 실제 map travel을 수행하지 않도록 `WITH_DEV_AUTOMATION_TESTS` 전용 suppress hook과 request count를 추가했다.
- `WBP_DroneReport` 에디터 자산에는 `ReturnToLobbyButton` 배치/이름 지정 변경을 별도 자산 변경으로 반영했다.

### 검증

- TDD red 확인: `DroneProto.D11.DroneReport.ReturnToLobbyGuard` 테스트를 먼저 추가한 뒤, `SetSuppressReturnToLobbyTravelForTest`, `RequestReturnToLobby`, `GetReturnToLobbyTravelRequestCountForTest` 누락 컴파일 에러를 확인했다.
- `Build.bat DroneProtoEditor Win64 Development -Project="D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject" -NoLiveCoding -WaitMutex` 성공.
- `Automation RunTests DroneProto` 성공, 40 tests found, all Success, exit code 0.
- 신규 테스트 `DroneProto.D11.DroneReport.ReturnToLobbyGuard` 성공.
- source-marker 테스트에 `ReturnToLobbyButton`, `ReportReturnToLobbyClicked`, `ReportReturnToLobbyTravel Target=LobbyMap`, `ReportReturnToLobbyIgnored Reason=AlreadyRequested`, `ReportReturnToLobbyButtonMissing` 확인을 추가했다.
- `git diff --check -- Source/DroneProto/Raid/DroneReportWidget.h Source/DroneProto/Raid/DroneReportWidget.cpp Source/DroneProto/Tests/DronePartInventoryTests.cpp` 성공. LF/CRLF warning 외 whitespace error 없음.
- 전체 자동화 로그에는 UE 초기화 단계의 `LogAutomationTest: Error: Condition failed` 4줄이 남지만, automation queue는 모두 `Result={Success}`와 `TEST COMPLETE. EXIT CODE: 0`으로 종료됐다.

### PIE 검색어

- `[DR_SUMMARY] ReportWidgetShown`
- `[DR_SUMMARY] ReportReturnToLobbyButtonMissing`
- `[DR_SUMMARY] ReportReturnToLobbyClicked`
- `[DR_SUMMARY] ReportReturnToLobbyTravel Target=LobbyMap`
- `[DR_SUMMARY] ReportReturnToLobbyIgnored Reason=AlreadyRequested`
- `[DR_SUMMARY] LobbyWidgetShown`

### PIE 판정

- Report 화면에서 `ReturnToLobbyButton`을 누르면 `ReportReturnToLobbyClicked` 다음 `ReportReturnToLobbyTravel Target=LobbyMap`이 떠야 한다.
- travel 이후 LobbyMap에서 `LobbyWidgetShown`이 다시 떠야 한다.
- 버튼을 연타해도 travel 로그가 2회 반복되면 안 되며, 두 번째 요청은 `ReportReturnToLobbyIgnored Reason=AlreadyRequested`로 남아야 한다.
- `ReportReturnToLobbyButtonMissing`이 뜨면 `WBP_DroneReport` 안에 `ReturnToLobbyButton` 이름의 버튼이 없거나 parent binding이 맞지 않는 상태다.

### SpecDecisionNeeded

- Report 화면의 최종 버튼 문구, 배치, 애니메이션, 검정 화면 전환 연출은 UMG/에디터 작업으로 남긴다.
- 실제 서비스 구조에서 Report 확인 후 로비 복귀가 클라이언트 단독 map travel인지, backend session leave/party state 정리와 함께 가야 하는지는 별도 서버/세션 설계 결정이 필요하다.
- LobbyMap 복귀 후 파티 유지, 재매칭, 결과 보상 수령 상태를 어떻게 보존할지는 아직 기획 결정 대상이다.

---
## 2026-06-29 — 레이드 로비 UI 표시와 상태 전환 검증

### 문제

레이드 입장 A/B/C 배정 로직은 구현됐지만, 로비 UI 쪽에서는 성공 경로와 예외 경로를 PIE에서 분리해 확인하기 어려웠다.

- LobbyMap에서 `WBP_RaidLobby` 표시와 버튼 클릭은 확인됐지만, C++ 부모 위젯이 `Main / Waiting / NoServer / Loading` 상태를 직접 관리해야 했다.
- A/B/C가 모두 full인 경우의 대기 팝업, timeout 이후 서버 부족 팝업, 취소/확인 버튼 흐름은 에디터에서 강제로 검증할 진입점이 부족했다.
- 성공 시 이동 대상은 실제 테스트/레이드 맵인 `TestMap`이어야 했다.
- Loading 상태에서 레이드 참가 버튼이 중복 호출되면 travel 요청이 2회 발생할 수 있는 위험이 있었다.
- 이 작업은 로비 UI 검증용 C++ hook이며, gameplay RPC, replication, 전투/부품/Report/Dodge/BossAttack 경로를 바꾸면 안 됐다.

### 수정

- `ALobbyPlayerController`가 local controller에서 `LobbyWidgetClass`를 생성하고 viewport에 추가하는 경로에 `[DR_SUMMARY] LobbyPCBeginPlay`, `LobbyWidgetCreateAttempt`, `LobbyWidgetShown` 로그를 추가했다.
- `URaidLobbyWidget` C++ 부모에 `MainLobbyPanel`, `WaitingPopupPanel`, `NoServerPopupPanel`, `LoadingPanel`, `RaidJoinButton`, `CancelMatchmakingButton`, `NoServerConfirmButton`을 `BindWidgetOptional`로 추가했다.
- `ERaidLobbyUIState`와 `ShowMainLobby()`, `ShowWaitingPopup()`, `ShowNoServerPopup()`, `ShowLoading()`을 추가해 한 번에 한 상태만 보이도록 했다.
- `RaidJoinButton`, `CancelMatchmakingButton`, `NoServerConfirmButton`은 `NativeConstruct()`에서 `AddUniqueDynamic`으로 바인딩해 중복 바인딩을 피했다.
- `URaidSessionSubsystem`이 active lobby widget을 보관하고, 배정 결과에 따라 `Success -> Loading`, `Waiting -> Waiting`, `Failed -> NoServer`, `Canceled -> Main` 상태를 호출하게 했다.
- `CancelMatchmaking()`은 retry timer를 중단하고 `RaidEntryCancel` 로그를 남긴 뒤 Main으로 돌아가며, 별도 travel을 실행하지 않는다.
- `ULocalAssignment` 기본 A/B/C 후보의 `TravelTarget`을 `TestMap`으로 맞췄다.
- 성공 결과의 `FailReason`은 `None`으로 정리해 `Success` 로그에 `NoServerAvailable`이 같이 보이지 않게 했다.
- PIE 검증용 local UI hook인 `ShowDebugWaitingPopup()`, `ShowDebugNoServerPopup()`, `ResetDebugMainLobby()`을 추가했다.
- debug hook 로그에는 `Scope=LocalUIOnly GameplayAuthority=Unaffected`를 남겨 서버 권한/게임플레이 상태를 바꾸지 않는 검증용 진입점임을 명시했다.
- Loading/Waiting 중 `RequestEntry()`가 다시 들어오면 `[DR_SUMMARY] RaidLobbyRequestEntryIgnored Reason=EntryInFlight`로 무시해 중복 travel 요청을 막았다.

### 검증

- TDD red 확인: 새 `DroneProto.RaidEntry.LobbyWidget.*` 테스트를 먼저 추가한 뒤, `GetCurrentLobbyUIState`, `ShowDebugWaitingPopup`, `ConfirmNoServerFromLobby`, `GetTravelRequestCountForTest` 누락 컴파일 에러를 확인했다.
- `Build.bat DroneProtoEditor Win64 Development -Project="D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject" -NoLiveCoding -WaitMutex` 성공.
- `Automation RunTests DroneProto.RaidEntry` 성공, 5 tests found, 5 Success, exit code 0.
- `Automation RunTests DroneProto` 성공, 39 tests found, all Success, exit code 0.
- `git diff --check -- Source/DroneProto/Lobby/RaidLobbyWidget.h Source/DroneProto/Lobby/RaidLobbyWidget.cpp Source/DroneProto/Lobby/RaidSessionSubsystem.h Source/DroneProto/Lobby/RaidSessionSubsystem.cpp Source/DroneProto/Tests/DronePartInventoryTests.cpp` 성공. LF/CRLF warning 외 whitespace error 없음.
- 전체 자동화 로그에는 기존 negative-path 테스트의 warning 로그와 UE 초기화 단계의 `LogAutomationTest: Error: Condition failed` 4줄이 남지만, automation queue는 모두 `Result={Success}`와 `TEST COMPLETE. EXIT CODE: 0`으로 종료됐다.

### PIE 검색어

- `[DR_SUMMARY] LobbyPCBeginPlay`
- `[DR_SUMMARY] LobbyWidgetCreateAttempt`
- `[DR_SUMMARY] LobbyWidgetShown`
- `[DR_SUMMARY] RaidLobbyWidgetNativeConstruct`
- `[DR_SUMMARY] RaidLobbyRequestEntryClicked`
- `[DR_SUMMARY] RaidEntryRequest`
- `[DR_SUMMARY] RaidAssignmentResult`
- `[DR_SUMMARY] RaidEntryTravel`
- `[DR_SUMMARY] LobbyUIState State=Main`
- `[DR_SUMMARY] LobbyUIState State=Waiting`
- `[DR_SUMMARY] LobbyUIState State=NoServer`
- `[DR_SUMMARY] LobbyUIState State=Loading`
- `[DR_SUMMARY] LobbyUIDebugHook`
- `[DR_SUMMARY] RaidEntryCancel`
- `[DR_SUMMARY] RaidLobbyRequestEntryIgnored`

### PIE 판정

- LobbyMap 시작 직후 `LobbyWidgetShown`, `RaidLobbyWidgetNativeConstruct`, `LobbyUIState State=Main`이 남아야 한다.
- 레이드 참가 버튼 성공 경로에서는 `RaidAssignmentResult Result=Success Slot=A`, `LobbyUIState State=Loading`, `RaidEntryTravel Slot=A Target=TestMap`이 이어져야 한다.
- `ShowDebugWaitingPopup()` 호출 시 `LobbyUIDebugHook Action=ShowWaitingPopup` 뒤 `LobbyUIState State=Waiting`이 남아야 한다.
- Waiting 상태에서 취소 버튼을 누르면 `RaidEntryCancel` 뒤 `LobbyUIState State=Main`이 남아야 한다.
- `ShowDebugNoServerPopup()` 호출 시 `LobbyUIDebugHook Action=ShowNoServerPopup` 뒤 `LobbyUIState State=NoServer`가 남아야 한다.
- NoServer 확인 버튼을 누르면 `LobbyUIState State=Main`으로 돌아와야 한다.
- Loading 상태에서 레이드 참가 요청이 중복으로 들어오면 `RaidLobbyRequestEntryIgnored Reason=EntryInFlight`가 남고 `RaidEntryTravel`이 추가로 반복되지 않아야 한다.

### SpecDecisionNeeded

- 실제 제품 구조의 server availability, `ServerState`, raid instance 선택은 여전히 backend/권한 layer 결정이 필요하다.
- Waiting / NoServer / Loading 패널의 최종 디자인, 문구 배치, 애니메이션, 검정 화면 연출은 UMG/에디터 작업으로 남긴다.
- 별도 load failed 전용 패널을 둘지, 현재 NoServer 계열 표시로 묶을지는 UI 기획 결정이 필요하다.
- A/B/C의 실제 endpoint와 dedicated/listen 운용 방식은 local prototype 밖의 결정이다.

---
## 2026-06-29 — 레이드 입장 A/B/C 배정 및 대기 재탐색

### 문제

기존 로비 입장 흐름은 `URaidLobbyWidget::RequestEntry()`에서 `URaidSessionSubsystem::RequestRaidEntry()`로 들어온 뒤 `ULocalAssignment::ResolveServer()`가 항상 A 서버와 `Lvl_ThirdPerson`만 반환했다.

- 기존 감사 보고서의 B. 레이드 입장 시스템에는 A/B/C 3개 후보, A -> B -> C 우선순위, 서버별 16인 정원, 만석 시 10초 대기, 1초 재탐색, timeout 실패 처리가 명시되어 있었다.
- `RaidGameState::CurrentPlayers`는 이미 raid instance에 들어간 뒤의 내부 상태라 pre-travel 배정 판단에 섞으면 안 됐다.
- 현재 단계는 실제 backend가 아니라 local assignment prototype이므로, 권한 한계를 과장하지 않고 자동화 가능한 모델로 닫아야 했다.
- Success가 아닌 Waiting / Failed / Canceled 상태에서는 `OpenLevel` 또는 `ClientTravel`이 실행되면 안 됐다.

### 수정

- `FRaidServerCandidate`를 추가해 `FServerEndpoint`, `CurrentPlayers`, `MaxPlayers`, `bIsOnline`, `bAcceptsPlayers`를 표현하게 했다.
- `FRaidAssignmentResult`와 `ERaidAssignmentResultType`을 추가해 `Success / Waiting / Failed / Canceled` 결과와 실패 사유, 선택 slot, debug reason을 보존하게 했다.
- `URaidAssignmentBase::ResolveRaidAssignment()`를 추가하고, 기존 `ResolveServer()`는 성공 endpoint만 반환하는 호환 wrapper로 유지했다.
- `ULocalAssignment` 기본 후보를 A/B/C 3개로 만들고, 모두 `Lvl_ThirdPerson`, `MaxPlayers=16`으로 초기화했다.
- 배정은 A -> B -> C 순서로 검사하며 offline, unavailable, full 후보를 skip한다.
- 첫 available 후보의 `TravelTarget`이 비어 있으면 `MapLoadFailed`로 실패 처리한다.
- 모든 후보가 full/unavailable이면 즉시 travel하지 않고 `Waiting` 상태로 전환한다.
- `URaidSessionSubsystem`에 10초 timeout / 1초 retry timer를 추가했다.
- retry 중 후보가 열리면 즉시 `Success`로 전환해 기존 `OpenLevel` / `ClientTravel` 경로를 실행한다.
- timeout이면 `NoServerAvailable` 실패로 처리하고 travel하지 않는다.
- `CancelMatchmaking()`은 retry timer를 중단하고 기존 LobbyMap 복귀 경로를 유지한다.
- 새 gameplay RPC, 새 replicated 변수, `RaidGameState::CurrentPlayers` 기반 capacity 판단, 전투/부품/반환/Report/Dodge/BossAttack 변경은 추가하지 않았다.
- `[DR_SUMMARY] RaidEntryRequest`, `RaidAssignmentResult`, `RaidEntryTravel`, `RaidEntryWait`, `RaidEntryRetry`, `RaidEntryFail`, `RaidEntryCancel` 로그를 추가했다.
- 로그에는 `AuthorityModel=LocalPrototype`을 남겨 현재 구조가 실제 server-authoritative backend가 아니라 테스트 가능한 local model임을 명시했다.

### 검증

- `Build.bat DroneProtoEditor Win64 Development -Project="D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject" -NoLiveCoding -WaitMutex` 성공.
- `Automation RunTests DroneProto.RaidEntry` 성공, 3 tests performed, warnings 0.
- `Automation RunTests DroneProto.D15` 성공, 1 test performed, warnings 0.
- `Automation RunTests DroneProto.D14` 성공, 2 tests performed, warnings 0.
- `Automation RunTests DroneProto.D7` 성공, 4 tests performed, warnings 0.
- `Automation RunTests DroneProto` 성공, 37 tests performed, failed 0. 기존 negative-path warning 로그 때문에 `succeededWithWarnings=5`로 분류된다.
- `git diff --check` 성공. LF/CRLF warning 외 whitespace error 없음.

### PIE 검색어

- `[DR_SUMMARY] RaidEntryRequest`
- `[DR_SUMMARY] RaidAssignmentResult`
- `[DR_SUMMARY] RaidEntryTravel`
- `[DR_SUMMARY] RaidEntryWait`
- `[DR_SUMMARY] RaidEntryRetry`
- `[DR_SUMMARY] RaidEntryFail`
- `[DR_SUMMARY] RaidEntryCancel`
- `[Server] PostLogin: CurrentPlayers =`
- `[DR_SUMMARY] Spawn PC=`

### PIE 판정

- 기본 로비 버튼 요청 시 `RaidEntryRequest` 뒤 `RaidAssignmentResult Result=Success Slot=A`와 `RaidEntryTravel Slot=A`가 남아야 한다.
- A가 full이면 `Slot=B`, A/B가 full이면 `Slot=C`가 선택되어야 한다.
- A/B/C가 모두 full이면 `RaidEntryWait`가 남고 `RaidEntryTravel`이 없어야 한다.
- 대기 중 후보가 열리면 `RaidEntryRetry` 뒤 `RaidAssignmentResult Result=Success`와 `RaidEntryTravel`이 이어져야 한다.
- 10초 timeout이면 `RaidEntryFail FailReason=NoServerAvailable`이 남고 travel하지 않아야 한다.
- cancel이면 `RaidEntryCancel`이 남고 retry가 중단되어야 한다.

### SpecDecisionNeeded

- 실제 제품 구조에서 server availability, `CurrentPlayers`, `ServerState`, raid instance 선택을 확정할 backend/권한 layer가 필요하다.
- B/C의 실제 dedicated/listen endpoint 또는 최종 travel target은 아직 미정이다.
- `ServerState`의 최종 소유 위치와 갱신 주체는 아직 local assignment prototype 밖의 결정이다.
- UMG 팝업 widget class 지정, 대기/실패/로드 실패 화면 배치, 로비 UX polish는 에디터/UMG 작업으로 남긴다.

---
## 2026-06-28 — POR-12 D15 Boss Attack / Drone Hit 최소 수직 슬라이스

### 문제

보스가 서버 권한으로 단순 공격 판정을 만들고, 판정 시점에 범위 안에 있는 Drone에게 피해를 주는 최소 전투 루프가 필요했다.

- Dodge는 이미 서버에서 최종 위치를 바꾸므로, 공격 판정은 클라이언트 입력이 아니라 서버 위치를 기준으로 해야 했다.
- 클라이언트가 Boss attack 결과를 직접 확정하면 안 된다.
- Drone HP, Death, DroneReport, 부품 반환 경로는 이미 있으므로 새 경로를 만들지 않고 재사용해야 했다.
- RaidEnd 또는 BossDead 이후 Boss attack은 무시되어야 했다.

### 수정

- `ARaidBoss::PerformDebugAreaAttackForServer(FVector AttackCenter, float RadiusCm, int32 DamageAmount)`를 추가했다.
- 함수는 서버 권한에서만 판정하며, `RaidState == End`, BossDead, invalid params, no world 상태는 `[DR_SUMMARY] BossAttackIgnored`로 무시한다.
- 판정은 `AttackCenter`와 Drone 서버 위치 사이의 XY 2D 거리로 계산한다.
- 살아 있고, possess가 맞고, `PlayerSelectionState == InBattle`인 Drone만 피해 대상이 된다.
- 범위 밖, Dead, NotInBattle, invalid controller 대상은 `[DR_SUMMARY] BossAttackMiss`로 남기고 피해를 주지 않는다.
- 범위 안의 대상은 `[DR_SUMMARY] BossAttackHit`를 남기고 기존 `ADrone::ApplyDamageForServer(..., "BossAttack")` 경로로 피해를 적용한다.
- 기존 `ApplyDamageForServer`가 HP 감소, `DamageTakenCount`, `DroneDeath`, `ReportCreated`, `ReturnAfterReport`, 부품 반환을 이어서 처리한다.
- 새 client-to-server Boss attack RPC, 새 replicated 변수, UMG/VFX/animation/uasset 변경은 추가하지 않았다.

### 검증

- TDD red 확인: D15 테스트를 먼저 추가한 뒤 `PerformDebugAreaAttackForServer` 누락 컴파일 에러를 확인했다.
- `Build.bat DroneProtoEditor Win64 Development -Project="D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject" -NoLiveCoding -WaitMutex` 성공.
- `Automation RunTests DroneProto.D15` 성공, 1 test performed.
- `Automation RunTests DroneProto.D14` 성공, 2 tests performed.
- `Automation RunTests DroneProto.D7` 성공, 4 tests performed.
- `Automation RunTests DroneProto` 성공, 34 tests performed.
- `git diff --check` 성공. LF/CRLF warning 외 whitespace error 없음.

### PIE 검색어

- `[DR_SUMMARY] BossAttack`
- `[DR_SUMMARY] BossAttackHit`
- `[DR_SUMMARY] BossAttackMiss`
- `[DR_SUMMARY] BossAttackIgnored`
- `[DR_SUMMARY] DroneDamage`
- `[DR_SUMMARY] DroneDeath`
- `[DR_SUMMARY] ReportCreated`
- `[DR_SUMMARY] ReturnAfterReport`

### PIE 판정

- 범위 안의 InBattle Drone은 `BossAttackHit` 뒤에 `DroneDamage`가 남아야 한다.
- lethal damage이면 `DroneDeath`, `ReportCreated`, `ReturnAfterReport`가 이어져야 한다.
- Dodge 뒤 판정 범위 밖에 있으면 `BossAttackMiss Reason=OutOfRange`가 남고 `DroneDamage`가 없어야 한다.
- RaidEnd 이후에는 `BossAttackIgnored Reason=RaidEnd`가 남고 `BossAttackHit`가 없어야 한다.
- BossDead 이후에는 `BossAttackIgnored Reason=BossDead`가 남고 Drone HP가 변하지 않아야 한다.

### SpecDecisionNeeded

- 없음. 실제 보스 패턴, 텔레그래프, 쿨다운, 수동 exec 트리거는 D15 최소 수직 슬라이스 범위 밖으로 남긴다.

---
## 2026-06-28 — D14/D14.5 서버 권한 Dodge 및 PIE 로그 진단

### 문제

기획 플로우의 `C + 방향키 = 회피` 입력이 서버 권한 전투 액션으로 연결되어야 했다.

- 기존 Dodge 요청 경로는 실제 서버 이동 적용까지 이어지지 않았다.
- Enhanced Input asset은 C++에서 직접 생성/수정하지 않고, 코드 쪽 연결 슬롯과 null guard만 준비해야 했다.
- PIE 2 Players 검증 중 `ServerMoveApplied`와 `OwnerMoveCorrected` 로그의 `PC`/`Pawn`/`ViewTarget`이 서로 다른 Pawn처럼 보여 실제 보정 대상 확인이 필요했다.

### 수정

- `ADrone::RequestDodge(FVector2D)`와 `Server_RequestDodge(FVector2D)` 경로를 통해 클라이언트는 요청만 보내고 서버가 최종 위치를 확정하도록 했다.
- `RequestDodgeForServer(FVector2D)`에서 서버 권한, Controller/Pawn 소유권, RaidEnd, Dead, InBattle, zero direction, cooldown을 검증한다.
- Dodge 성공 시 서버 ActorLocation을 변경하고 기존 `OwnerMoveSync` / movement replication 경로로 owner 클라 위치 보정을 유지한다.
- Dodge 이동거리는 `DodgeDistanceCm = 300.0f`, 최소 서버 쿨다운은 `DodgeCooldownSeconds = 0.20f`로 두었다.
- Dodge 이동은 Vector/Booster/report 이동거리 누적에서 제외하고 `[DR_SUMMARY] MoveDistanceIgnored Reason=Dodge`로 남긴다.
- `DodgeAction` C++ 입력 슬롯과 move-axis cache를 추가했다. asset 지정은 에디터 작업으로 남기고, 미지정 상태에서는 null guard로 crash 없이 동작한다.
- 방향 없이 C만 누른 경우 클라이언트에서 요청하지 않고 `[DR_SUMMARY] DodgeInputIgnored ... Reason=ZeroDirection`으로 무시한다.
- `OwnerMoveCorrected` 로그에 실제 보정 대상인 `CorrectedActor`, `bPawnMatchesCorrectedActor`, `LocalRole`, `RemoteRole`을 추가했다.
- `ServerMoveApplied`, `ServerDodgeApplied`, `OwnerMoveSyncSent` 로그에 `Drone`, `Pawn`, `bPawnMatchesDrone`을 추가해 서버 이동 대상과 PC 소유 Pawn 일치 여부를 바로 확인할 수 있게 했다.

### 검증

- `Build.bat DroneProtoEditor Win64 Development -Project="D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject" -NoLiveCoding -WaitMutex` 성공.
- `Automation RunTests DroneProto.D14` 성공, 2 tests performed.
- `Automation RunTests DroneProto.D7` 성공, 4 tests performed.
- `Automation RunTests DroneProto` 성공, 33 tests performed.
- PIE 수동 검증에서 `Dodge Result=Success`, `ServerDodgeApplied`, `MoveDistanceIgnored Reason=Dodge`, `DodgeInputIgnored Reason=ZeroDirection`, `Dodge Ignored Reason=Cooldown` 확인.

### PIE 검색어

- `[DR_SUMMARY] Dodge`
- `[DR_SUMMARY] Dodge Ignored`
- `[DR_SUMMARY] DodgeInputIgnored`
- `[DR_SUMMARY] ServerDodgeApplied`
- `[DR_SUMMARY] ServerMoveApplied`
- `[DR_SUMMARY] OwnerMoveSyncSent`
- `[DR_SUMMARY] OwnerMoveCorrected`
- `[DR_SUMMARY] ReplicatedLocation`
- `[DR_SUMMARY] MoveDistanceIgnored`
- `[DR_SUMMARY] Attack Accepted`
- `[DR_SUMMARY] Attack Ignored`

### PIE 판정

- `ServerMoveApplied` / `ServerDodgeApplied` / `OwnerMoveSyncSent`에서 `bPawnMatchesDrone=true`면 서버가 PC 소유 Pawn만 이동 적용한 것이다.
- `OwnerMoveCorrected`에서 `CorrectedActor`와 `Pawn`이 같고 `bPawnMatchesCorrectedActor=true`면 owning client가 자기 Pawn만 보정한 것이다.
- `ReplicatedLocation ... ViewTargetResult=SimProxyObserved`는 SimProxy 관찰 로그이며 owner correction과 구분해서 봐야 한다.
- 현재 코드 기준으로 다른 플레이어 Pawn을 실제 보정하는 증거는 없고, 기존 혼동은 로그 표시와 PIE 멀티 컨텍스트 로그 interleaving에 가까웠다.

### SpecDecisionNeeded

- Dodge cooldown duration has no numeric design value. 현재는 비정상 연타 이동을 막기 위한 최소 서버 쿨다운 0.20초를 사용한다.
- Dodge displacement move-distance scoring policy has no final design value. 현재는 Vector/Booster/report 이동거리 누적에서 제외한다.

---

## 2026-06-28 — POR-8 D13 전투 계산식/부품 효과 명세 정렬

### 문제

구현 감사에서 Drone Core / Weapon 계산식과 최신 기획서 사이에 확인이 필요한 지점이 남아 있었다.

- Fracture Burst는 총 피해 11은 맞지만 `HitCount=4`가 총 타수인지 추가 타수인지 로그만으로 헷갈릴 수 있었다.
- Drain Core의 "기본 공격력/이속 낮음" 문구는 있었지만 적용할 수 있는 정확한 수치가 없었다.
- Zenith / Booster / Drain / Pulse / Fracture / Vector 경계값을 한 번에 고정하는 자동화 테스트가 부족했다.

### 수정

- `FDroneWeaponCalculationResult`에 `AdditionalHitCount`를 추가했다.
- Fracture Burst는 기존 `HitCount=4`를 유지하면서 `AdditionalHitCount=3`, `BaseDamage=5`, `BonusDamage=6`, `WeaponDamage=11`로 의미를 분리했다.
- `[DR_SUMMARY] WeaponCalc` 로그에 `AdditionalHitCount`를 추가했다.
- Drain Core의 기본 공격/이속 페널티는 정확한 수치가 없어 임의 구현하지 않았다.
- `DroneProto.D13.DroneCombat.SpecAlignment` 테스트로 Core/Weapon 경계값과 Drain 실제 피해량 기준 회복을 검증했다.

### 검증

- `git diff --check` 성공. LF/CRLF warning 외 whitespace error 없음.
- `Build.bat DroneProtoEditor Win64 Development -Project="D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject" -NoLiveCoding -WaitMutex` 성공.
- `Automation RunTests DroneProto.D13.DroneCombat.SpecAlignment` 성공, 1 test performed.
- `Automation RunTests DroneProto.D7` 성공, 4 tests performed.
- `Automation RunTests DroneProto` 성공, 31 tests performed.

### PIE 검색어

- `[DR_SUMMARY] WeaponCalc`
- `[DR_SUMMARY] CoreCalc`
- `[DR_SUMMARY] AttackCalc`
- `[DR_SUMMARY] DrainHeal`
- `[DR_SUMMARY] BossDamage`
- `[DR_SUMMARY] MoveDistanceReset`
- `[DR_SUMMARY] Attack Accepted`
- `[DR_SUMMARY] Attack Ignored`

### SpecDecisionNeeded

- Drain base attack/move penalty has no numeric value.

---

## 2026-06-28 — POR-9 RaidTimeLimit 실제 종료 트리거 연결

### 문제

DroneReport 발생 조건에는 드론 사망, 보스 처치, 레이드 제한 시간 종료가 포함되어 있지만, 실제 RaidTimeLimit 타이머 만료가 서버 RaidEnd로 이어지는 경로가 없었다.

### 수정

- `ARaidGameMode`에 서버 권한 RaidTimeLimit 타이머를 추가했다.
- `RaidState`가 `Battle`로 진입한 뒤 `StartRaidTimeLimitTimerForServer()`가 제한 시간 타이머를 1회 시작한다.
- 타이머 만료 시 `Reason=RaidTimeLimit`으로 기존 `ReturnAllEquippedPartsForRaidEnd` 경로를 호출한다.
- BossDefeated 또는 기존 RaidEnd가 먼저 발생하면 RaidTimeLimit 타이머를 정리하고, End 상태에서 재진입하면 무시한다.
- 생존 `InBattle` 플레이어는 DroneReport를 받은 뒤 equipped 부품이 반환되고, `Selecting` 플레이어는 기존 정책대로 selected 부품만 반환된다.
- Death Report를 이미 받은 플레이어는 RaidTimeLimit 종료에서 Report를 중복 생성하지 않는다.

### 검증

- `git diff --check` 성공. LF/CRLF warning 외 whitespace error 없음.
- `Build.bat DroneProtoEditor Win64 Development -Project="D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject" -NoLiveCoding -WaitMutex` 성공.
- `Automation RunTests DroneProto` 성공, 31 tests performed.
- Full automation 안에서 `DroneProto.D12.RaidGameMode.RaidTimeLimitEndsRaid`와 `DroneProto.D12.RaidGameMode.RaidTimeLimitDoesNotDuplicateBossDefeated` 성공을 확인했다.

### PIE 검색어

- `[DR_SUMMARY] Ready`
- `[DR_SUMMARY] RaidTimerStart`
- `[DR_SUMMARY] RaidTimerExpired`
- `[DR_SUMMARY] RaidEnd Reason=RaidTimeLimit`
- `[DR_SUMMARY] ReportCreated`
- `[DR_SUMMARY] RaidEndReturn`
- `[DR_SUMMARY] ReturnSkipped`
- `[DR_SUMMARY] RaidEndStateCleaned`

### 보류

- POR-9의 `DroneReportManager` 분리와 PlayerID/key 기반 report 중복 방지는 이번 범위에서 제외했다. 현재 구현은 기존 PlayerController bool 중복 방지 경로를 유지한다.

---

## 2026-06-27 — POR-5/POR-7 RaidEnd 반환 및 cleanup 안정화

### 문제

구현 감사에서 드론부품 환원 경로의 두 가지 리스크가 남아 있었다.

- `RaidEnd`가 `InBattle` 플레이어의 equipped 부품 중심으로 처리되어, 아직 Ready 전 `Selecting` 상태에서 selected 부품을 들고 있던 플레이어의 부품 반환 근거가 부족했다.
- Death / Logout / RaidEnd 조합에서 PlayerController의 selected/equipped 슬롯은 비워지지만, 같은 Pawn의 `ADrone` 내부 loadout FName까지 항상 비워지는지 테스트 근거가 약했다.

### 수정

- POR-5: `ARaidGameMode::ReturnAllEquippedPartsForRaidEnd`가 `Selecting` 플레이어도 순회해 selected Core/LeftWeapon/RightWeapon을 반환하도록 보강했다.
- POR-7: `ADrone::ClearEquippedLoadoutForServer(FName Reason)`를 추가해 서버 cleanup 경로에서 Drone 내부 equipped loadout과 runtime 공격 값을 명시적으로 비우도록 했다.
- Death, Logout, RaidEnd 경로에서 반환 후 PlayerController slot과 Drone internal loadout이 모두 `None`/empty 상태가 되도록 정리했다.
- 중복 반환 방지는 별도 반환 로그 구조가 아니라 기존 slot clear 기준을 유지했다.
- 선택 교체 순서와 클라이언트 UI/Replication 구조는 변경하지 않았다.

### 검증

- Build는 직전 작업에서 성공했고, Linear 검증 반영 단계에서는 사용자가 "빌드는 했으므로 그 다음부터"라고 지시해 재실행하지 않았다.
- `git diff --check` 성공. LF/CRLF warning 외 whitespace error 없음.
- Targeted automation:
  - `Automation RunTests DroneProto.D6.RaidGameMode.LogoutClearsDroneLoadout` 성공.
  - `Automation RunTests DroneProto.D6.RaidGameMode.RaidEndReturnClearsInBattlePlayers` 성공.
  - `Automation RunTests DroneProto.D6.Drone.DeathReturnClearsEquippedSlots` 성공.
- Full automation:
  - `Automation RunTests DroneProto` 성공, 28 tests performed.
- Linear:
  - POR-7에 검증 코멘트 반영 후 `Done` 처리.
  - POR-5 동작 유지 확인은 POR-7 검증 코멘트에 함께 기록.

### PIE 검색어

- `[DR_SUMMARY] DroneDeath`
- `[DR_SUMMARY] DeathReturn`
- `[DR_SUMMARY] RaidEnd`
- `[DR_SUMMARY] RaidEndReturn`
- `[DR_SUMMARY] ReturnSkipped`
- `[DR_SUMMARY] Ready`
- `[DR_SUMMARY] Attack Ignored`

### 보류

- 이번 반영은 자동화 테스트 기준 검증이다. 2 Client PIE에서 수동으로 Death 후 Logout, RaidEnd 후 Logout, Selecting RaidEnd 반환 로그를 한 번 더 확인하면 수동 검증까지 닫을 수 있다.

---

## 2026-06-27 — RaidEnd 이후 공격/이동 차단 및 상태 정리

### 문제

2 Client PIE에서 `BossDeath -> RaidEnd -> DroneReport -> 부품 반환`까지 1차 루프는 동작했지만, RaidEnd 이후에도 입력 처리 일부가 살아 있었다.

- Boss HP가 이미 `0.00`인데도 `Attack Accepted`, `BossDamage` 로그가 추가로 남음.
- RaidEnd 이후 이동 입력에서 `MoveDistanceIgnored Reason=RaidEnd`는 찍히지만 서버 위치 이동은 적용됨.
- RaidEnd/Return 이후 UI refresh에 `State=InBattle` 또는 이전 Core/Left/Right 선택값이 stale하게 남는 경우가 있음.

### 수정

- `ADrone::HandleAttackBossForServer` 초입에서 `RaidState == End`를 검사해 공격을 서버에서 즉시 무시.
- Boss가 이미 defeated 상태이면 공격 계산, Pulse/Vector 카운터, BossDamage, Attack Accepted 경로에 들어가지 않게 차단.
- `ARaidBoss::ApplyDamageForServer`는 이미 죽은 Boss에게 추가 데미지를 적용하지 않고 `[DR_SUMMARY] BossDamageIgnored: Reason=BossDead`를 남김.
- `ADrone::ApplyMoveInputForServer`와 서버 Tick 이동 적용 경로에서 RaidEnd를 모두 검사해 RaidEnd 이후 서버 위치 변경 자체를 막음.
- `ARaidGameMode::ReturnAllEquippedPartsForRaidEnd`는 RaidEnd를 1회만 처리하고 중복 호출은 `[DR_SUMMARY] RaidEndSkipped Reason=AlreadyEnded`로 종료.
- RaidEnd 반환 후 `ARaidPlayerController::FinalizeRaidEndForServer`와 owning client RPC에서 Equipped/Selected 캐시를 `None`으로 비우고 UI refresh를 요청.
- 부품 반환은 기존 `ReturnEquippedPartsForServer -> DronePartReturnManager` 단일 경로를 유지하고, 공격/이동 코드에 재고 계산을 새로 만들지 않음.

### 검증

- `Build.bat DroneProtoEditor Win64 Development -Project="D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject" -NoLiveCoding -WaitMutex` 성공.
- `Automation RunTests DroneProto.D6.RaidGameMode.RaidEndReturnClearsInBattlePlayers` 성공.
- 확인 로그:
  - `[DR_SUMMARY] RaidEnd Reason=Automation PlayerCount=1`
  - `[DR_SUMMARY] RaidEndStateCleaned ... State=Locked Core=None Left=None Right=None`
  - `[DR_SUMMARY] Attack Ignored: Reason=RaidEnd ...`
  - `[DR_SUMMARY] MoveInput ... Result=Ignored Reason=RaidEnd`
  - `[DR_SUMMARY] RaidEndSkipped Reason=AlreadyEnded RequestedReason=AutomationRetry`

### PIE 검색어

- `[DR_SUMMARY] Attack Ignored: Reason=RaidEnd`
- `[DR_SUMMARY] Attack Ignored: Reason=BossDead`
- `[DR_SUMMARY] BossDamageIgnored: Reason=BossDead`
- `[DR_SUMMARY] ServerMoveIgnored PC=`
- `[DR_SUMMARY] RaidEndSkipped Reason=AlreadyEnded`
- `[DR_SUMMARY] RaidEndStateCleaned Player=`
- `[DR_SUMMARY] RaidEndClientRefresh Player=`

### 보류

- 전체 `DroneProto` 자동화 재실행은 앱 사용량 제한으로 추가 확인하지 못했다. 직전 빌드는 성공했고, RaidEnd 핵심 자동화는 통과했다.

---

## 2026-06-27 — Vector/Booster 전투 계산과 DroneReport 기반

### 구현

- `FDroneCombatRules`를 추가해 무기/코어 계산과 리포트 점수/등급 계산을 검증 가능한 helper로 분리.
- Vector Cannon은 서버 이동거리 snapshot 기준으로 5m당 보너스 피해를 계산하고, 1회 공격 입력에서 좌/우 Vector 계산을 모두 끝낸 뒤 Vector 누적값만 reset.
- Booster Core는 Booster 전용 누적 이동거리를 사용해 공격 보너스와 이동속도 보너스 계산 기반을 연결.
- Pulse/Fracture/Zenith/Drain 계산도 같은 helper 기반으로 정리해 좌/우 무기 합산이 중복/누락되지 않게 유지.
- `FDroneCombatRecord`로 생존 시간, Boss damage, 이동거리, 회복량, 피격 횟수를 서버에서 누적.
- Boss 사망 또는 RaidEnd 시 서버에서 `FDroneReportData`를 생성하고 owning client RPC로 전달.
- `UDroneReportWidget` C++ glue를 추가해 Blueprint 위젯에서 생존 시간, Boss 피해, 이동거리, 회복량, 보너스, 등급 텍스트를 읽을 수 있게 함.
- Boss HP/MaxHP 복제와 BossDamage/BossDeath summary 로그를 정리.

### 검증

- `Build.bat DroneProtoEditor Win64 Development -Project="D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject" -NoLiveCoding -WaitMutex` 성공.
- 자동화 테스트 추가/확장:
  - `DroneProto.D9.DroneCombat.Formulas`
  - `DroneProto.D9.Drone.VectorBoosterCombatRecord`
  - `DroneProto.D10.DroneReport.Formulas`
  - `DroneProto.D10.DroneReport.DuplicateGeneration`
  - `DroneProto.D11.DroneReport.WidgetTextHelpers`
  - `DroneProto.D5.ManualSummaryLogs.SourceMarkers`
- PIE 2 Clients 기준 `ReportCreated`, `ReportWidgetShown`, `RaidEndReturn`, `ReturnSkipped AlreadyEmpty` 흐름을 로그로 확인.

### PIE 검색어

- `[DR_SUMMARY] WeaponCalc Player=`
- `[DR_SUMMARY] CoreCalc Player=`
- `[DR_SUMMARY] Attack Accepted: Player=`
- `[DR_SUMMARY] BossDamage: OldHP=`
- `[DR_SUMMARY] BossDeath:`
- `[DR_SUMMARY] CombatRecord Player=`
- `[DR_SUMMARY] ReportCreated Player=`
- `[DR_SUMMARY] ReportWidgetShown Player=`
- `[DR_SUMMARY] ReturnAfterReport Player=`

### 범위 제외

- ContributionManager, DataTable 전환, 보스 패턴, VFX는 아직 미구현.
- Report 위젯 C++ 바인딩과 에셋 연결만 진행했고, UMG 레이아웃 polish는 별도 범위로 남김.

---
## 2026-06-23 — [디버깅] 드론 이동 입력 방향 오류 수정

### 문제

전투 진입 후 드론의 실제 이동 방향이 입력 의도와 90도 어긋났다.

- `A` / 왼쪽 입력: 뒤로 이동
- `W` / 앞 입력: 오른쪽 이동
- `D` / 오른쪽 입력: 앞으로 이동
- `S` / 뒤 입력: 왼쪽 이동

### 원인

C++ 서버 이동 처리 코드(`ADrone::ApplyMoveInputForServer`)는 `Axis.X=좌우`, `Axis.Y=전후` 기준으로 정상 동작하고 있었다.

실제 원인은 Enhanced Input Mapping Context의 `IA_Move` WASD 축 매핑 오류였다. `W/S`는 Y축 전후 입력으로, `A/D`는 X축 좌우 입력으로 들어와야 하지만 두 축이 서로 뒤바뀐 상태였다.

### 수정

`IA_Move` 매핑을 다음 기준으로 정리했다.

- `W`: Y축 `+1`
- `S`: Y축 `-1`
- `D`: X축 `+1`
- `A`: X축 `-1`

`ADrone::ApplyMoveInputForServer`는 수정하지 않았다. RPC, Replicate/OnRep, 이동거리 누적 로직, 전투 상태 조건도 그대로 유지했다.

### 검증

PIE 멀티 테스트에서 전투 진입 후 이동 방향을 확인했다.

- `W`: 앞으로 이동
- `S`: 뒤로 이동
- `A`: 왼쪽 이동
- `D`: 오른쪽 이동

서버/클라이언트 화면에서 이동 방향이 동일하게 보이는 것을 확인했다.

---

## 2026-06-23 — D8 이동 입력/이동거리 누적 검증 및 PIE 포커스 이슈 정리

### 검증 목적

Vector Cannon / Booster Core 구현 전에, 이동거리 기반 효과의 원천 데이터가 클라이언트 위치값이 아니라 서버 `ActorLocation` 기준으로만 계산되는지 재확인했다.

서버는 클라이언트가 보낸 이동 축 입력만 받고, 실제 이동 적용과 `VectorAccumulatedMoveDistanceMeters` / `BoosterAccumulatedMoveDistanceMeters` 누적은 서버 Tick 경로에서 처리하는 D8 구조를 유지했다.

### 확인 내용

별도 클라이언트 창에서 이동 입력 시 서버 화면에서는 움직이지만 클라이언트 본인 화면에서는 움직임이 보이지 않는 문제가 있었다. 이를 확인하는 과정에서 owning client에 한해 표시용 로컬 이동을 복구하는 임시 패치를 적용했다. 이 로컬 이동은 화면 체감용이며 이동거리 누적값은 변경하지 않는다.

이후 서버 플레이어가 전투에 진입하면 클라이언트 입력이 서버 플레이어에게 들어가는 것처럼 보이는 현상이 있었다. 처음에는 이동 대상 꼬임, 소유 Pawn 불일치, 입력 버퍼 잔류 문제로 의심했으나, 입력 소스 로그를 추가해 확인한 결과 실제 원인은 Listen Server PIE 환경의 포커스 전환이었다.

서버 플레이어가 AutoReady로 전투에 진입하면서 에디터/Listen Server 뷰포트가 active input target이 되었고, 그 상태에서 키보드 입력이 서버 플레이어에게 들어갔다. 클라이언트 창을 다시 클릭하면 클라이언트 Pawn이 정상적으로 이동했다. 따라서 코드상 이동 대상 꼬임보다는 Listen Server PIE 테스트 환경의 입력 포커스 문제로 판단했다.

### 현재 결론

- 클라이언트 Ready 후 클라이언트 창에 포커스가 있을 때 클라이언트 Pawn 이동은 정상.
- 서버에서 해당 클라이언트 Pawn의 `MoveDistance` 누적도 정상.
- 이동거리 누적은 서버 `HasAuthority()` 경로에서만 처리.
- 서버 플레이어 전투 시작 후 입력이 서버 Pawn으로 들어간 현상은 Listen Server 뷰포트 포커스 전환 때문.
- Dedicated Server 구조에서는 서버에 플레이어 입력 뷰포트가 없으므로 동일 문제가 재현되지 않을 가능성이 높다.

### 후속 작업

- 임시로 추가한 `MoveLocalSource`, `MoveLocalPredict`, `MoveServerBefore`, `MoveServerAfter` 로그 정리.
- Dedicated Server + Clients 2개 환경에서 이동거리 누적 재검증.
- 검증 후 Vector Cannon / Booster Core 이동거리 기반 효과 구현.
- Vector/Booster 구현 전까지 이동 시스템은 불필요하게 추가 수정하지 않기.

---

## 2026-06-21 — D8 서버 권한 이동 입력 + 이동거리 누적 기반

### 구현

- Vector Cannon/Booster Core 실제 보너스 계산은 제외하고, 두 효과가 사용할 서버 권한 이동거리 원천 데이터만 먼저 구축.
- `ADrone`에 Tick을 켜고 `SetReplicates(true)`, `SetReplicateMovement(true)`를 명시해 서버 이동 결과가 클라이언트로 복제되도록 정리.
- `ADrone::Move`는 클라이언트 위치값을 보내지 않고 이동 축 입력만 `Server_SetMoveInput`으로 전달.
- 서버는 `ApplyMoveInputForServer`에서 입력 벡터를 길이 1 이하로 clamp하고, Pawn 없음/ADrone 아님/Possess 불일치/Dead/NotInBattle/ZeroAxis 상황을 거부.
- Listen Server 로컬 플레이어도 동일한 서버 적용 경로를 타도록 authority 분기에서 직접 `ApplyMoveInputForServer`를 호출.
- 서버 Tick에서 ActorLocation delta를 cm가 아닌 m 기준으로 계산해 `VectorAccumulatedMoveDistanceMeters`, `BoosterAccumulatedMoveDistanceMeters`에 각각 누적.
- 이동거리 누적은 `PlayerSelectionState == InBattle`, Dead 아님, 정상 possess, RaidEnd 아님 조건에서만 허용.
- Spawn/Possess/Loadout/Death/RaidEnd 시 전체 이동거리 누적값과 baseline을 reset.
- FirstSample은 baseline만 잡고 누적하지 않으며, `DeltaSeconds > 0.25`, 20m 초과 hard delta, MaxSpeed 기반 예상치보다 큰 delta는 Teleport/TooLargeDelta로 무시.
- 향후 Vector 공격 후 사용할 `ResetVectorMoveDistanceForServer` helper를 추가하되, 이번 단계에서는 Vector 피해 보너스에 연결하지 않음.
- `WEAPON_003`은 이동거리 보너스를 아직 구현하지 않고 base 7 damage만 반환하도록 유지해 Vector/Booster 효과 구현 범위를 넘지 않게 함.
- 신규 DR_SUMMARY:
  - `[DR_SUMMARY] MoveInput ...`
  - `[DR_SUMMARY] MoveAudit ...`
  - `[DR_SUMMARY] MoveDistance ...`
  - `[DR_SUMMARY] MoveDistanceIgnored ...`
  - `[DR_SUMMARY] MoveDistanceReset ...`

### 검증

- RED: 신규 D8 자동화 테스트가 `ApplyMoveInputForServerForTest`, `UpdateMoveDistanceForServerForTest`, 이동거리 getter 미구현으로 컴파일 실패하는 것을 먼저 확인.
- `Build.bat DroneProtoEditor Win64 Development -Project="D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject" -NoLiveCoding -WaitMutex` 성공.
- `Automation RunTests DroneProto.D8` 신규 3개 성공.
- `Automation RunTests DroneProto` 전체 22개 성공.
- 신규 테스트:
  - `DroneProto.D8.Drone.ServerMoveInputAuthority`
  - `DroneProto.D8.Drone.MoveDistanceAccumulation`
  - `DroneProto.D8.Drone.MoveDistanceReset`

### PIE 검색어

- `[DR_SUMMARY] MoveInput PC=`
- `[DR_SUMMARY] MoveAudit PC=`
- `[DR_SUMMARY] MoveDistance PC=`
- `[DR_SUMMARY] MoveDistanceIgnored PC=`
- `[DR_SUMMARY] MoveDistanceReset PC=`

### 범위 제외 / 다음 단계

- Vector Cannon 이동거리 피해 보너스와 Booster Core 이동거리/속도/공격 보너스는 아직 미구현.
- Vector+Vector 동시 장착 시 한쪽 계산 후 reset되는 문제를 피하려면 다음 단계에서 공격 입력 1회 기준 이동거리 snapshot을 먼저 잡고 좌/우 무기 계산 후 Vector 누적을 reset해야 함.
- Booster는 실제 속도 변경까지 한 번에 넣기보다, D8 누적값 기반 공격 보너스를 먼저 서버 계산에 연결하고 MovementComponent 속도 변경/클라 체감은 별도 검증하는 편이 안전.
- 2 Clients PIE에서 클라이언트 창 이동이 서버 로그의 `MoveInput`, `MoveDistance`로 반영되는지 수동 확인 필요.
- UMG 에셋/배치/디자인, 보스 패턴, VFX, ContributionManager, DroneReport, DataTable 전환은 변경하지 않음.

---

## 2026-06-21 — D7 이동거리 비기반 전투 부품 효과 구현

### 구현

- 기존 D5 `RequestAttackBoss -> Server_RequestAttackBoss -> HandleAttackBossForServer` 공격 입력 경로를 유지하고, 서버에서 좌/우 무기 피해와 코어 배율을 최종 피해로 계산.
- `Pulse Laser(WEAPON_001)`는 슬롯별 독립 카운터를 사용해 1/2타 8, 3타 18 강공격 후 reset 처리.
- Pulse 카운터는 새 Loadout 적용, Death, RaidEnd에서 초기화. RaidEnd는 ReturnManager 반환 경로를 유지한 채 eligible Drone의 전투 런타임 상태만 reset.
- `Fracture Burst(WEAPON_002)`는 `5 + 3 * 2 = 11`, `HitCount=4`로 유지하고, `WEAPON_002 + WEAPON_002 + CORE_002` 회귀값 `Damage=20.90`을 보존.
- `Zenith Core(CORE_001)`는 `CurrentHP / MaxHP`를 0~1로 clamp한 뒤 10% 단위 `0.02` 보너스를 적용하고 최대 `1.20` modifier로 제한.
- `Drain Core(CORE_003)`는 AttackModifier `0.85`, MoveSpeedModifier `0.9` 기준을 유지하고, 실제 Boss HP 감소량의 `12%`를 서버에서 Drone HP로 회복. 1회 Z 입력당 최대 3, MaxHealth 초과 회복 금지.
- Drain 회복의 소수 값을 잃지 않도록 `ADrone::Health` 내부 저장 타입을 float로 전환하되, 기존 `GetHealth()` 정수 표시 API는 유지.
- Dead 상태에서는 기존 Attack/Move/Dodge/Heal 차단을 유지하고, Drain 회복도 Dead에서는 발생하지 않음.
- 신규 DR_SUMMARY:
  - `[DR_SUMMARY] PulseAttack ...`
  - `[DR_SUMMARY] FractureAttack ...`
  - `[DR_SUMMARY] ZenithBonus ...`
  - `[DR_SUMMARY] DrainHeal ...`

### 검증

- RED: 신규 D7 자동화 테스트가 `GetPulseAttackCountForTest`, `GetHealthValueForTest` 미구현으로 컴파일 실패하는 것을 먼저 확인.
- `Build.bat DroneProtoEditor Win64 Development -Project="D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject" -NoLiveCoding -WaitMutex` 성공.
- `Automation RunTests DroneProto` 전체 19개 성공.
- 신규 테스트:
  - `DroneProto.D7.Drone.PulseLaserCombat`
  - `DroneProto.D7.Drone.FractureBurstCombat`
  - `DroneProto.D7.Drone.ZenithCoreCombat`
  - `DroneProto.D7.Drone.DrainCoreCombat`
- 브랜치 `codex-drone-combat-effects-d7`에 내역별 커밋:
  - `b28ecb2 feat: implement drone combat effects`
  - `8ebafa6 test: cover drone combat effects`

### PIE 검색어

- `[DR_SUMMARY] PulseAttack PC=`
- `[DR_SUMMARY] FractureAttack PC=`
- `[DR_SUMMARY] ZenithBonus PC=`
- `[DR_SUMMARY] DrainHeal PC=`
- `[DR_SUMMARY] Attack PC=`
- `[DR_SUMMARY] DeadInputIgnored PC=`

### 범위 제외 / 다음 단계

- Vector Cannon 이동거리 피해 보너스와 Booster Core 이동거리/속도 보너스는 이번 범위에서 구현하지 않음.
- UMG 에셋/배치/디자인, 보스 패턴, VFX, ContributionManager, DroneReport, DataTable 전환은 변경하지 않음.
- Vector/Booster 구현 전 서버 권한 이동거리 누적 기준, FloatingPawnMovement 서버 동기화 방식, 이동거리 reset 타이밍을 먼저 결정해야 함.

---

## 2026-06-21 — D6 드론 사망/레이드 종료 부품 환원 + Dead Ready 차단

### 구현

- `ADrone`에 서버 권한 HP/Dead 상태를 명확히 두고 `Health`, `MaxHealth`, `bIsDead`를 Replicated/OnRep 경로로 정리.
- HP 감소/회복/사망 판정은 서버 함수에서만 처리하고, Dead 상태에서는 Attack/Move/Dodge/Heal을 무시하도록 차단.
- HP가 0 이하가 되면 `HandleDeath`가 1회만 실행되고, `DronePartReturnManager`를 통해 Equipped Core/LeftWeapon/RightWeapon만 DeathReturn 처리.
- DeathReturn 성공/skip 후 Equipped 슬롯을 비워 Logout/Disconnect 재진입 시 중복 반환되지 않도록 정리.
- `ARaidGameMode::ReturnAllEquippedPartsForRaidEnd(Reason)`를 추가해 InBattle/Locked 플레이어의 EquippedParts만 RaidEndReturn 처리.
- RaidEndReturn 후 Equipped 슬롯을 비워 RaidEnd 재호출/Logout에서도 MaxCount 초과 반환이 일어나지 않게 유지.
- 빈 슬롯 반환은 실패성 에러가 아니라 `[DR_SUMMARY] ReturnSkipped ... Reason=AlreadyEmpty` no-op 로그로 남김.
- `D6KillDrone`, `D6RaidEndReturn` 테스트용 경로를 추가하되, 결과창/UMG/DroneReport 전환은 구현하지 않음.
- D6-1 수동 PIE 회귀: Dead Pawn이 Ready/AutoReady로 `Selecting->InBattle` 되는 상태 버그를 `ProcessReadyForRaidForServer` 공통 경로에서 차단.
- Dead Ready/AutoReady 무시 시 재고, SelectedParts, EquippedParts를 건드리지 않고 아래 summary 로그만 남김.
  - `[DR_SUMMARY] ReadyIgnored PC=... Reason=DeadPawn SelectionState=...`
  - `[DR_SUMMARY] AutoReady PC=... Result=Ignored Reason=DeadPawn SelectionState=...`

### 검증

- `Build.bat DroneProtoEditor Win64 Development -NoLiveCoding` 성공.
- 신규/확장 자동화 포함 `Automation RunTests DroneProto` 전체 15개 성공.
- 추가된 핵심 테스트:
  - `DroneProto.D6.Drone.DeathReturnClearsEquippedSlots`
  - `DroneProto.D6.RaidGameMode.RaidEndReturnClearsInBattlePlayers`
  - `DroneProto.D6.RaidPlayerController.DeadPawnReadyAndAutoReadyIgnored`
- D6-1 테스트는 RED 단계에서 Dead Pawn이 Ready/AutoReady 성공으로 InBattle이 되는 실패를 확인한 뒤, guard 추가 후 GREEN으로 전환.

### PIE 검색어

- `[DR_SUMMARY] D6KillDrone RequestPC=`
- `[DR_SUMMARY] DroneDamage PC=`
- `[DR_SUMMARY] DroneDeath PC=`
- `[DR_SUMMARY] DeathReturn PC=`
- `[DR_SUMMARY] RaidEnd Reason=`
- `[DR_SUMMARY] RaidEndReturn PC=`
- `[DR_SUMMARY] ReadyIgnored PC=`
- `Result=Ignored Reason=DeadPawn`
- `[DR_SUMMARY] DeadInputIgnored PC=`

### 범위 제외

- Pulse/Drain/Vector/Booster 세부 효과, 보스 패턴, VFX, ContributionManager, DroneReport, DataTable 전환, UMG 에셋 생성/배치는 D6/D6-1 범위에서 제외.
- D5의 선택/Ready/AutoReady/PlayerSelectionState 구조는 유지하고, Dead Pawn 차단만 Ready 공통 경로에 추가.

---

## 2026-06-21 — D5 드론 부품 선택/AutoReady/전투 Vertical Slice 완료 정리

### 완료 기준

- TestMap PIE 2 Players 기준으로 두 `PlayerController`가 모두 `BP_Drone_C` 계열 Pawn을 possess하고 `[DR_SUMMARY] Spawn ... IsADrone=true`를 출력하는 것을 확인.
- 개인별 선택 타이머가 `[DR_SUMMARY] SelectTimerStart ... Duration=15.00`으로 시작하며, UI 표시 `TimeLeft`가 15초를 초과하지 않도록 보정.
- `PlayerSelectionState`를 `Selecting` / `InBattle`로 분리해 전역 `RaidState=Battle`이어도 아직 Selecting인 플레이어는 부품 선택을 계속할 수 있게 유지.
- 수동 Ready와 15초 AutoReady 모두 같은 서버 검증/장착 경로를 사용하며, 성공 시 Selected 슬롯을 Equipped 슬롯으로 복사하고 Selected 슬롯은 `NAME_None`으로 비운다.
- 아무 부품 없음, 일부 슬롯만 선택한 상태, 코어/무기 조합 모두 전투 참가 가능.
- Selecting 상태에서 Z 공격 입력이 들어와도 서버에서 `[DR_SUMMARY] AttackIgnored ... Reason=NotInBattle`만 출력하고 Boss HP는 변경하지 않음.
- InBattle 상태에서는 Z 입력 1회에 좌/우 무기를 동시에 계산해 `ARaidBoss` HP를 감소시키는 최소 전투 루프를 확인.
- 접속 종료/반환 경로는 Selecting이면 SelectedParts, InBattle이면 EquippedParts를 반환하며, 반환 성공 후 슬롯을 비워 중복 반환을 방지.
- UI 연결용 C++ 부모 위젯에 `BindWidgetOptional` 텍스트 훅과 BlueprintCallable/Pure getter를 추가했고, TimerText는 서버 기준 남은 시간을 클라이언트 UI 타이머로 주기 갱신한다.
- 수동 검증용 `[DR_SUMMARY]` 로그를 Spawn/Select/Cancel/Ready/AutoReady/Attack/AttackIgnored/Return/UIRefresh 흐름에 정리.

### 수동 PIE 확인

- 부품 선택 성공: `Core=CORE_002`, `Left=WEAPON_002`, `Right=WEAPON_002`.
- 수동 Ready 성공: `SelectTimerStop Reason=ManualReady`, `Ready Result=Success`, `SelectionState=Selecting->InBattle`, `AttackPower=22`.
- 기본 드론 플레이어 AutoReady 성공: `AutoReady Result=Success`, `SelectionState=Selecting->InBattle`, `Core=None Left=None Right=None AttackPower=0`.
- 무기 장착 드론 공격 성공: `Damage=20.90`, Boss HP `979.10 -> 958.20 -> 937.30` 감소 확인.
- InBattle 상태 UIRefresh는 SelectedParts가 아니라 EquippedParts 기준으로 `Core/Left/Right`를 표시.

### 자동화/빌드 검증

- `Build.bat DroneProtoEditor Win64 Development -NoLiveCoding` 성공.
- `Automation RunTests DroneProto` 전체 12개 성공.

### D5 미완료/다음 단계

- PIE 종료 시 EquippedParts 반환 summary 최종 확인은 남김. 기본 드론은 반환할 부품이 없으므로 Return 로그가 없어도 정상이고, 장착 플레이어는 Core/LeftWeapon/RightWeapon 반환 후 Count가 Max를 넘지 않아야 한다.
- UMG 배치/디자인/아이콘 polish, 보이는 보스 Actor/HP UI, 전체 드론 사망 플로우 검증, 레이드 종료 시 전체 플레이어 EquippedParts 반환은 D6 이후 작업으로 분리.
- Booster Core 이동거리 보너스, Drain Core 흡혈, Vector Cannon 이동거리 피해 보너스, Pulse Laser 3타 강공격 수동 검증, ContributionManager, DroneReport, DataTable 전환, 보스 패턴/VFX는 D5 범위에서 제외.

---

## 2026-06-20 — 부품 선택 UI + 공유 재고 멀티 검증 + Ready 전투 진입 연결 (D4/D5)

### 구현

- `WBP_DronePartSelect`는 에디터에서 배치하고, C++ 부모 `UDronePartSelectWidget`이 버튼/키 입력/Refresh 흐름을 담당하도록 정리.
- UI는 재고를 직접 수정하지 않고 `ARaidPlayerController`의 요청 함수만 호출한다.
  - `RequestSelectPartFromUI(EPartSlot Slot, FName PartID)`
  - `RequestCancelPartFromUI(EPartSlot Slot)`
  - `RequestReadyForRaidFromUI()`
- `ADronePartInventory`는 서버 공유 재고 Actor로 유지하고, `ARaidGameState::DronePartInventory`를 Replicated 포인터로 클라이언트에 전달.
- 클라이언트는 `OnRep_DronePartInventory` / `OnRep_PartStocks` 이후 UI Refresh 이벤트를 받아 남은 수량을 다시 표시한다.
- `ADronePartReturnManager`를 통해 선택 취소/교체/Logout/RaidEnd/Death 반환 경로를 분리하고, 반환 성공 후 슬롯을 `NAME_None`으로 비워 중복 반환을 막는다.
- 테스트용 자동 장착은 제거하고, 장착/스탯 적용은 `RequestReadyForRaidFromUI` 이후 선택값 기준으로만 진행하도록 변경.

### Ready → Battle 흐름

- `RequestReadyForRaidFromUI` → `Server_RequestReadyForRaid`.
- 서버에서 Core/LeftWeapon/RightWeapon 선택값과 PartID 타입을 검증한다.
- 현재 Pawn이 `ADrone`인지 확인한 뒤 선택한 PartID를 `ADrone::ApplyLoadout(Core, Left, Right)`로 전달한다.
- `ApplyLoadout`은 서버 권한에서만 실행되며, PartID별 스탯을 부품 객체로 만들고 `RecalculateStats()`로 MaxHealth/AttackPower/Health를 계산한다.
- 성공 시 `RaidPlayerController`의 Selected 슬롯을 Equipped 슬롯으로 이동하고 Selected 슬롯은 비운다.
- `ARaidGameState::SetRaidStateForServer(ERaidState::Battle)`로 전투 상태를 시작한다.
- 클라이언트는 `Client_NotifyRaidReadyResult` 성공 응답을 받으면 부품 선택 UI를 닫고 `InputMode GameOnly`로 복구한다.

### 검증

- PIE 2 Clients에서 `CurrentPlayers=2` 확인.
- Client에서 `OnRep_DronePartInventory` 수신 확인.
- `ADronePartInventory`가 `bReplicates=true`, `bAlwaysRelevant=true`로 복제되는 것 확인.
- `OnRep_PartStocks`로 양쪽 클라이언트 UI 수량 갱신 확인.
- 한쪽이 부품 선택/취소/교체 시 다른 쪽 UI에도 공유 재고 변경이 반영됨을 확인.
- `RequestReadyForRaid` 로그에서 플레이어별 Core/Left/Right 선택값 출력 확인.
- UnrealBuildTool `DroneProtoEditor Win64 Development` 빌드 성공.
- 자동화 테스트 `Automation RunTests DroneProto.D` 성공.
  - `DroneProto.D4.DronePartInventory.StockConsumeReturn`
  - `DroneProto.D5.Drone.ApplySelectedLoadout`
  - `DroneProto.D5.DronePartReturnManager.ReturnAndReplace`
  - `DroneProto.D5.DronePartSelectUI.BlueprintGlue`

### 다음

- PIE 2 Clients에서 Ready 버튼 클릭 후 실제 창 기준으로 UI 닫힘, InputMode GameOnly, `RaidState=Battle` 전환을 재확인.
- Battle 상태 이후 보스/전투 스폰 플로우와 드론 사망 UI는 다음 단계에서 연결.
- FloatingPawnMovement 이동 동기화 방식은 별도 결정 필요.

---

## 2026-06-17 — 공유 부품 재고 + 서버 선택 RPC (D4)

### 구현

- `ADronePartInventory` (`AInfo`) 추가 — 위치/메시 없는 서버 공유 재고 Actor, `bReplicates = true`, 이동 복제 없음.
- `FDronePartStock` — `PartID`, `PartType`, `CurrentCount`, `MaxCount`. D4 기본 데이터는 Core 3종 각 2개, Weapon 3종 각 3개.
- `EDronePartType` 추가 — 기존 D3 `EPartSlot`은 그대로 재사용하고 중복 `EDronePartSlot`은 만들지 않음.
- `PartStocks`는 `ReplicatedUsing=OnRep_PartStocks`; 클라 OnRep에서 `[Client] Part stock updated: CORE_ZENITH 1/2` 형식 로그와 `OnPartStocksChanged` 이벤트 자리 제공.
- `ARaidGameMode::BeginPlay`에서 서버만 `ADronePartInventory`를 Spawn하고, `ARaidGameState::DronePartInventory` 포인터로 Replicated 보관.
- `ARaidPlayerController`에 Server RPC 추가:
  - `Server_RequestSelectPart(EPartSlot Slot, FName NewPartID)`
  - `Server_RequestCancelPart(EPartSlot Slot)`
  - `Client_NotifyPartSelectionResult(EPartSlot Slot, FName PartID, bool bSuccess, FString Reason)`
- D4 임시 선택 상태는 `RaidPlayerController`의 `SelectedCorePartID`, `SelectedLeftWeaponPartID`, `SelectedRightWeaponPartID`에 보관. 아직 `ADrone::EquippedParts`/`RecalculateStats`와 연결하지 않음.
- PIE 수동 검증용 Exec helper:
  - `D4SelectPart Core CORE_ZENITH`
  - `D4SelectPart Core CORE_BOOSTER`
  - `D4CancelPart Core`

### 서버 권한 흐름

- 클라이언트는 서버 소유 Inventory에 직접 RPC를 보내지 않고, client-owned `RaidPlayerController` Server RPC로 요청한다.
- 서버는 `GameState->DronePartInventory`를 찾아 PartID 존재 여부, 슬롯 타입(Core/Weapon), 재고를 검증한다.
- 같은 슬롯 같은 부품 선택은 no-op 성공(`Already selected`).
- 교체는 새 부품 `TryConsumePart` 성공 후 기존 부품 `ReturnPart`, 그 다음 슬롯 교체. 새 부품 차감 실패 시 기존 선택 유지.
- 취소는 선택값이 있으면 반환 후 `NAME_None`, 이미 빈 슬롯이면 no-op 성공.

### 검증

- 빌드: `DroneProtoEditor Win64 Development` 성공.
- 자동화 테스트: `DroneProto.D4.DronePartInventory.StockConsumeReturn` 추가. 기본 재고, consume, return, max clamp, unknown part 실패를 검증하도록 작성했고 빌드 포함은 확인. 현재 CLI 실행은 `Ready to start automation` 이후 worker 대기 상태로 timeout되어 에디터/CI에서 재실행 필요.
- 2클라 PIE 수동 로그 목표:
  1. 클라 A `D4SelectPart Core CORE_ZENITH` → 서버 `CORE_ZENITH` 차감, 모든 클라 재고 OnRep 로그.
  2. 클라 A `D4SelectPart Core CORE_BOOSTER` → `CORE_BOOSTER` 차감 후 `CORE_ZENITH` 반환, 슬롯 변경.
  3. 클라 A `D4CancelPart Core` → 선택 부품 반환, 슬롯 비움.
  4. 동일 Weapon 재고 3개를 모두 소비한 뒤 추가 선택 → 실패 RPC 사유 `Out of stock`, 기존 선택 유지.

### 다음 (D5)

- 최종 드래프팅 UI 작성 및 `OnPartStocksChanged` 연결.
- 2클라 이상 동시 클릭 race condition 검증. 설계상 서버 수신 순서 선착순으로 처리.
- 사망/접속 종료 시 선택 부품 반환.
- FloatingPawnMovement 이동 동기화 방식 확정.

---

## 2026-06-16 — Drone + DronePart 골격 + 스탯 합산 (D3)

### 구현

- `ADrone` (`APawn`) — `UFloatingPawnMovement`, Enhanced Input(MoveAction), Replicated 3종(Health/MaxHealth/AttackPower), `TakeDamage` HasAuthority 가드, `OnRep_Health` 화면 출력, `MuzzlePoint` USceneComponent (D6~8용)
- `UDronePart` (`UObject`, Abstract/EditInlineNew/DefaultToInstanced) — `EPartSlot` {Core/LeftWeapon/RightWeapon}, `FDronePartStats` {HealthBonus/AttackBonus}, `ServerFire` stub
- `DummyParts` 3종 (슬롯별 합산 검증용):

| 클래스 | Slot | HealthBonus | AttackBonus |
|---|---|---|---|
| `UCorePart` | Core | 100 | 0 |
| `ULeftWeaponPart` | LeftWeapon | 0 | 20 |
| `URightWeaponPart` | RightWeapon | 20 | 15 |

- `EquippedParts TArray<UDronePart*>` — 서버 전용, 복제 안 함
- `ServerEquipPart` / `RecalculateStats` — 부품 합산 → MaxHealth/AttackPower 갱신 → Replicated로 자동 복제
- BeginPlay(서버): 3종 순차 장착 → 검증 기대값 MaxHealth=220, AttackPower=35

### 파일 구조

```
Source/DroneProto/
  DronePart.h/.cpp      ← UDronePart + EPartSlot + FDronePartStats
  DummyParts.h/.cpp     ← UCorePart / ULeftWeaponPart / URightWeaponPart (한 파일)
  Drone.h/.cpp          ← ADrone
```
모듈 루트에 평면 배치 — Build.cs IncludePath 추가 불필요. UE는 `PublicIncludePaths`의 루트 경로(`"DroneProto"`) 아래를 자동 탐색.

### 설계 결정

- **네트워크 경계**: 이동 = 클라 권한(반응성 우선), 전투 판정 = 서버 권한. 조작 픽스 — 방향키 이동 / Z 공격(서버 부품 `ServerFire`) / C(+방향) 회피(서버 i-frame 판정)
- **UDronePart 미복제**: 클라는 최종 스탯(Replicated 3종)만 필요. 부품 객체 복제 시 서브오브젝트 복잡도 급증 → 불필요
- **RecalculateStats에서 Health = MaxHealth**: 장착 시 값 변경이 없으면 OnRep_Health가 트리거되지 않음(UE 복제 시스템 특성). 장착 시 풀피 초기화 = 드래프팅 전제라 자연스러움
- **암묵 제약**: RecalculateStats는 드래프팅(전투 전)에서만 호출 — 전투 중 호출 시 풀피 회복 버그
- **소형 더미 클래스 묶기**: 단일 h/cpp에 UCLASS 여럿 선언 가능 (UHT가 파일 단위로 `.generated.h` 생성). 2개 파일로 3개 더미 처리

### 이슈 & TODO

**FloatingPawnMovement 이동 동기화 (D5 이전 확정 필요)**
- `UFloatingPawnMovement`는 `CharacterMovementComponent`와 달리 클라→서버 입력 RPC 미내장
- autonomous proxy가 로컬에서 `AddMovementInput` 호출 → 서버 Pawn은 제자리 → 서버가 제자리 위치를 복제 → rubber-banding
- 단일 클라 PIE에서는 서버=클라라 미발현. **2클라 PIE 이동 테스트 필수**
- 해결 방향: Server RPC로 입력 전달(서버 권한에 가깝게) vs 클라 위치 보고(PvE라 허용 가능, 직접 구현)
- `Drone.cpp` 생성자에 TODO 박아둠

### 검증 (2클라 PIE)

1. 스탯 복제: 양쪽 클라 화면에 `[Client] Health=220  MaxHealth=220  AttackPower=35` 출력 확인
2. 서버 로그: `[Server] Equipped: UCorePart` × 3종 + `[Server] Stats: MaxHealth=220, AttackPower=35`
3. 이동 동기화: 클라1 이동 → 클라2 화면에서 클라1 드론 움직임 확인 (rubber-banding 여부 체크)

### 다음

- D4: 공유 부품 풀(`DronePartInventory`) + 드래프팅 UI

### 트러블슈팅 — UHT include 6글자 잘림 (반나절 소요)

- **증상**: UHT가 `.gen.cpp` 생성 시 헤더명 앞 6글자 잘림 (`DroneProtoCharacter.h` → `"rotoCharacter.h"`). 디스크에 그대로 기록됨
- **원인**: Build.cs에 하위폴더용 IncludePath(`"DroneProto/Drone"`) 오설정 → UHT가 헤더 경로 prefix를 strip하다 파일명까지 먹음
- **헛다리 (전부 무관)**: Intermediate 클린 / UBA 비활성화 / LiveCoding / 캐시 삭제 → 모두 "생성 이후" 레이어라 무의미
- **진단 분기점**:
  1. `.gen.cpp` 직접 열어 깨진 include 실재 확인 → 생성 단계 문제
  2. TPTest(코드 있는 새 프로젝트) 빌드 성공 → 엔진 정상
  3. D2 클론 깨끗 + D3 추가 시 깨짐 → 범인 좁힘
  4. DronePart 단독 + Build.cs 변경만으로 재현 → IncludePath 확정
- **해결**: IncludePath 제거 + 소스를 모듈 루트에 평면 배치 (UE는 모듈 루트 하위를 자동 컴파일, IncludePath 추가 자체가 불필요)
- **교훈**: 소스 멀쩡한데 생성물만 깨지면 캐시 아닌 생성 단계 의심 / "잘 되다 갑자기"는 깨끗한 클론 클린빌드로 트리거 노출 / Build.cs IncludePath 남용 금지

---

## 2026-06-15 — 입장 UI 트랙 (D2 누락분)

### 구현

- 레이드 입장 플로우: "레이드 참가" 단일 버튼 → 자동 배정 → 레이드 레벨 진입
- `FServerEndpoint` (SlotId / TravelTarget / bIsLevelName)
- `URaidAssignmentBase` (추상 전략) / `ULocalAssignment` (항상 A 반환)
- `URaidSessionSubsystem` (`UGameInstanceSubsystem`) — `RequestRaidEntry` 진입점
- `URaidLobbyWidget` / `ALobbyGameMode` / `ALobbyPlayerController`
- 예외 경로 stub: `ERaidEntryFailReason` + 팝업 3종(MatchmakingWait / NoServer / LoadFailed) + `RequestRaidEntry` [확인1·2·3] 분기 주석
- 에디터: LobbyMap, WBP_RaidLobby, BP_LobbyPlayerController, BP_LobbyGameMode

### 설계 결정

- 배정 결정 = 게임 서버 권한 밖(별도 레이어). A/B/C 독립 데디라 단일 인스턴스가 전역 정원을 모름 → 매치메이킹은 인스턴스 위 레이어
- 단일→멀티 전환 = `ULocalAssignment` → `URemoteMatchmaking` 교체 한 줄 (`Subsystem::Initialize`)
- 진입 = `OpenLevel` 기반(`bIsLevelName=true`). 데디 접속(`ClientTravel`/IP)은 플래그 플립으로 나중
- 입장 시퀀스/예외 중 단일에서 닫히는 건 정상경로뿐. 만석/배정/매칭대기/재탐색은 멀티 전제 → stub, D11
- `ServerState`(인스턴스 가용성) vs `RaidState`(레이드 진행단계) 분리 — 합치지 않음. 출격은 개인별

### 검증

- PIE 정상경로 확인: 참가 → 레이드 레벨 진입 → `CurrentPlayers` 증가

### 트러블슈팅

- `VisualStudioTools` 플러그인 `RulesError` 재발 → `.uproject`에서 `Enabled: false` (D1과 동일)

### 다음

- D3: Drone(Pawn, HP/스탯 Replicated) + DronePart(부모클래스). 서버 권한 핵심

---

## Day 3 — Day 2 버그 수정 & PIE 검증

### 목표
- Day 2에서 발생한 빌드/실행 문제 원인 파악 및 수정
- RaidGameMode가 실제로 적용되는지 PIE로 검증

### 한 일

**1. VisualStudioTools 플러그인 재비활성화 (`DroneProto.uproject`)**
- 증상: VS에서 DroneProtoCharacter 관련 에러 다수 (전체 모듈 빌드 차단)
- 원인: `.uproject`가 `"VisualStudioTools": Enabled: true`로 되어 있었음
- Day 1 문제 3에서 이미 해결했던 문제가 다시 발생
- 해결: `Enabled: false`로 복원

**2. 소스 파일 인코딩 수정 (`DroneProtoCharacter.h`, `.cpp`)**
- 증상: VS IntelliSense 오탐 에러, 한국어 주석이 `������` 같은 깨진 문자로 표시
- 원인: VS 기본 저장 인코딩 CP949로 저장된 파일을 UTF-8로 읽어 mojibake 발생
- 해결: 파일 전체를 UTF-8로 재작성

**3. World Settings GameMode Override 수정**
- 증상: `DefaultEngine.ini`에서 `GlobalDefaultGameMode`를 `RaidGameMode`로 바꿨음에도
  PIE에서 `BP_ThirdPersonGameMode`가 계속 적용됨
- 원인: 레벨(`Lvl_ThirdPerson`) World Settings의 GameMode Override가 Blueprint GameMode를
  직접 지정하고 있었고, 레벨 설정이 `.ini`보다 우선순위가 높음
- 해결: 에디터 World Settings → GameMode Override → `RaidGameMode`로 변경 후 레벨 저장

### PIE 검증 결과

```
LogLoad: Game class is 'RaidGameMode'                        ✅
LogNet: Welcomed by server (..., Game: /Script/DroneProto.RaidGameMode)  ✅
[Server] PostLogin: CurrentPlayers = 1                       ✅ 서버 입장
[Server] PostLogin: CurrentPlayers = 2                       ✅ 클라이언트 접속
PC: RaidPlayerController_0 / RaidPlayerController_1          ✅ 우리 PC 사용
[Server] Logout: CurrentPlayers = 1                          ✅ 클라 퇴장
[Server] Logout: CurrentPlayers = 0                          ✅ 세션 종료
```

### 설계 확인 — CurrentPlayers 위치

- `CurrentPlayers`는 `ARaidGameState`에 `Replicated`로 선언되어 있음 (처음부터 올바른 위치)
- `ARaidGameMode`는 값을 소유하지 않고 `GetGameState<ARaidGameState>()->CurrentPlayers`로 씀
- 클라이언트는 `GetGameState<ARaidGameState>()->CurrentPlayers`를 바로 읽을 수 있어
  "현재 N명 참가 중" UI에 추가 작업 없이 사용 가능

### 학습

- **GameMode Override 우선순위**: 레벨 World Settings > `DefaultEngine.ini` > 기본값
  `.ini`만 바꾸면 레벨 오버라이드가 여전히 이김 — 레벨 저장 필수
- **소스 파일 인코딩**: UE5 소스는 UTF-8 (BOM 없이). VS에서 저장 시 인코딩 확인 필요
- **GameMode vs GameState 역할**: GameMode = 서버 전용 로직 창구,
  GameState = 클라에 노출할 월드 상태 저장소. 클라이언트가 볼 값은 항상 GameState에

---

## Day 2 — 플레이어 접속 & GameMode/GameState 골격

### 목표
- 레이드 전용 GameMode/GameState/PlayerController/PlayerState C++ 클래스 생성
- ERaidState UENUM 및 CurrentPlayers 복제 변수 구현
- 접속(PostLogin) / 퇴장(Logout) 시 CurrentPlayers 서버 권한 변경

### 한 일

**1. 클래스 4종 신설 (`Source/DroneProto/Raid/`)**
- `ARaidGameMode` : `AGameModeBase` — 접속/퇴장 처리, 나머지 3개 클래스 등록
- `ARaidGameState` : `AGameStateBase` — 복제 상태 보관소
- `ARaidPlayerController` : `APlayerController` — stub (드론 입력은 다음 단계)
- `ARaidPlayerState` : `APlayerState` — stub

**2. ERaidState UENUM**
```cpp
UENUM(BlueprintType)
enum class ERaidState : uint8 { Waiting, Drafting, Battle, End };
```
`RaidGameState.h`에 선언. 이후 레이드 흐름 제어에 사용.

**3. ARaidGameState 복제 변수**
- `ReplicatedUsing=OnRep_RaidState` → `ERaidState RaidState` (초기값 Waiting)
- `Replicated` → `int32 CurrentPlayers` (초기값 0)
- `GetLifetimeReplicatedProps`에 `DOREPLIFETIME`으로 등록
- `OnRep_RaidState`: 클라이언트 측 로그 출력

**4. 접속/퇴장 처리 (서버 권한)**
```cpp
void ARaidGameMode::PostLogin(APlayerController*)
{
    if (HasAuthority())
        GetGameState<ARaidGameState>()->CurrentPlayers++;
}
void ARaidGameMode::Logout(AController*)
{
    if (HasAuthority())
        GetGameState<ARaidGameState>()->CurrentPlayers--;
    Super::Logout(Exiting);
}
```
GameMode → GameState 경유, HasAuthority() 가드 유지.

**5. 프로젝트 기본 클래스 등록**
- `DefaultEngine.ini` `GlobalDefaultGameMode` → `/Script/DroneProto.RaidGameMode`
- `ARaidGameMode` 생성자에서 GameStateClass / PlayerControllerClass / PlayerStateClass 지정

### 검증 방법 (PIE)
- Net Mode: Play As Listen Server, Players: 2
- 접속 시 `[Server] PostLogin: CurrentPlayers = 2` 로그 확인
- 한 명 퇴장 시 `[Server] Logout: CurrentPlayers = 1` 로그 확인
- 클라이언트 창에서 `RaidState` = Waiting 복제 확인 (`OnRep_RaidState` 로그)

### 회고
- GameMode(서버 전용)와 GameState(전체 복제)의 역할 분리가 서버 권한 구조의 핵심
- GameMode는 논리 흐름, GameState는 "클라가 알아야 할 세계 상태" — 이 원칙을 드론 HP·부품 재고에도 그대로 적용 예정
- 다음: 드론 Pawn 스폰 + 부품 데이터 구조

---

## Day 1 — 프로젝트 셋업 & 멀티플레이어 동기화 검증

### 목표
- UE 5.7 C++ 프로젝트 생성 및 멀티플레이어 동작 검증
- 서버 권한(server authoritative) 변수 복제 기초 구현
- GitHub + Git LFS 협업 환경 구축

### 한 일

**1. 프로젝트 생성**
- UE 5.7, Third Person 템플릿(C++), 하이브리드(Blueprint+C++) 구조로 생성
- 첫 언리얼 프로젝트 — Unity/C++ 경험 기반으로 진입

**2. 멀티플레이어 동기화 검증**
- PIE 다중 클라이언트(Listen Server)로 2~3명 접속 테스트
- Third Person 템플릿 기본 replication으로 캐릭터 위치 동기화 확인

**3. 서버 권한 변수 복제 구현**
- `Health`를 `Replicated` 변수로 선언하고 `OnRep_Health` 콜백 연결
- `ApplyTestDamage()`에 `HasAuthority()` 가드를 두어 서버에서만 값 변경
- `GetLifetimeReplicatedProps()`에 `DOREPLIFETIME`으로 복제 등록
- 결과: 서버 창에서 데미지 적용 시 → 모든 클라이언트에 자동 복제 확인
  ```
  Server: Health = 90
  Client: Health replicated = 90
  ```
- 클라이언트에서 직접 변경 시도 시 `HasAuthority()`에 막혀 적용 안 됨을 확인
  → "진짜 값은 서버에만, 클라는 받아서 표현"하는 서버 권한 구조의 최소 단위 검증

이 패턴(Replicated 변수 + 서버 권한 변경 + OnRep 반영)은 이후 드론 HP,
공유 부품 재고, 보스 HP에 그대로 재사용 예정.

### 막힌 것 & 해결 (트러블슈팅)

**문제 1: C++ 선언/구현 불일치로 컴파일 실패**
- 증상: `IsContiguousContainer`, `StaticAssert` 등 엔진 내부 템플릿 에러 다수
- 원인: `.cpp`에 구현만 추가하고 `.h`에 선언을 누락 → `DOREPLIFETIME`이
  `Health` 타입을 추론하지 못해 연쇄 에러 발생
- 해결: `.h`에 변수·함수 선언 추가

**문제 2: 빌드 캐시 꼬임**
- 증상: 코드가 맞는데도 빌드 실패 지속
- 해결: 에디터·VS 종료 후 `Binaries`/`Intermediate` 삭제 →
  Generate VS project files → Rebuild
- 학습: `Build`(변경분만)는 "최신 상태"로 건너뛰므로, 강제 재컴파일은 `Rebuild` 사용

**문제 3: VisualStudioTools 플러그인이 빌드 차단**
- 증상: `Expecting to find a type ... 'VisualStudioTools' ... Result: Failed (RulesError)`
  → DroneProto 모듈까지 빌드 실패, 에디터 실행 불가
- 원인: 엔진 기본 포함 플러그인의 빌드 규칙 충돌 (개발에 불필요한 플러그인)
- 해결: `.uproject`에서 해당 플러그인 비활성화 → 정상 실행
  ```json
  "Plugins": [ { "Name": "VisualStudioTools", "Enabled": false } ]
  ```

**문제 4: VRAM 부족 경고**
- 증상: PIE 다중 창 실행 시 Video Memory 부족 경고
- 해결: Engine Scalability를 Low로 낮추고 플레이어 수를 2로 조정
  (테스트 단계에서는 화질 불필요)

### 협업 환경
- GitHub 리포지토리 연동, Git LFS 설정 (`.uasset`/`.umap` 등 바이너리 관리)
- `.gitignore`로 `Binaries`/`Intermediate`/`Saved` 등 자동생성물 제외
- 팀 그라운드룰에 커밋 규칙 추가 (빌드 깨진 채 push 금지, 바이너리 동시작업 금지 등)

### 회고
- 첫 언리얼임에도 멀티 동기화 + 서버 권한 변수 복제까지 도달
- 빌드 트러블슈팅에 시간이 들었으나, 캐시·플러그인·빌드 구성 등
  언리얼 빌드 파이프라인의 동작을 이해하는 계기가 됨
- 다음: GameManager(GameMode/GameState 분리) → Player → Drone+DronePart 통합 구현

---

## 2026-07-08 - Q1 Boss MaxHP 60,000 alignment

### Scope
- Applied Q1 from `docs/Audit/NextWorkQueue_CurrentSpec_20260708.md`.
- Changed the raid boss default HP to the current spec value: 60,000.
- Kept the change C++/test-only. No UMG, `.uasset`, `.umap`, timer, boss pattern, or damage-formula logic changes.

### Changes
- `ARaidBoss::MaxHP` default: `1000.0f` -> `60000.0f`.
- `ARaidBoss::CurrentHP` default: `1000.0f` -> `60000.0f`; existing `BeginPlay()` still copies `MaxHP` into `CurrentHP` on authority.
- Added `DroneProto.Q1.RaidBoss.SpecMaxHP` automation coverage.
- Updated DroneReport formula fixtures from 1,000-based test inputs to 60,000-based inputs while preserving the same intended boss-damage ratios.

### Verification
- RED: `Automation RunTests DroneProto.Q1.RaidBoss.SpecMaxHP; Quit` failed before the production edit on `Q1 boss MaxHP matches current 60000 spec`.
- GREEN: same Q1 test passed after the HP change and logged `[DR_SUMMARY] BossDamage: OldHP=60000.00 Damage=1000.00 NewHP=59000.00 MaxHP=60000.00`.
- Build: `Build.bat DroneProtoEditor Win64 Development -Project="D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject" -NoLiveCoding -WaitMutex` succeeded.
- Full automation: `Automation RunTests DroneProto; Quit` found 65 tests, 65 succeeded, 0 failed, exit code 0.

### Manual Follow-up
- PIE/manual balance remains unverified: 16-player real DPS and average DPS 330 are planning assumptions, not proven by this code pass.

---

## 2026-07-08 - Q2 Core base modifiers

### Scope
- Applied Q2 from `docs/Audit/NextWorkQueue_CurrentSpec_20260708.md`.
- Added core base attack/move modifiers from the Core data-table values.
- Kept Zenith/Booster/Drain special formulas, Drain heal cap, weapon formulas, RecalculateStats guard, boss HP/pattern/timer, RPC, replication, UMG, and assets unchanged.

### Changes
- `FDroneCoreCalculationResult` now carries `CoreMoveSpeedModifier`.
- `CalculateCoreBonus()` returns Zenith 1.0/1.0, Booster 0.95/1.0, and Drain 0.85/0.9.
- `RefreshMoveSpeedForServer()` now uses `BaseMoveSpeed * CoreMoveSpeedModifier * (1 + MoveSpeedBonus)`.
- Updated D7/D9/D13 tests and added server-path coverage for Drain 4.05 m/s.

### Verification
- RED: `Automation RunTests DroneProto.D13.DroneCombat.SpecAlignment; Quit` failed before the production edit on Booster/Drain base modifiers and Drain move speed.
- GREEN: same D13 test passed after the production edit.
- Build: `Build.bat DroneProtoEditor Win64 Development -Project="D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject" -NoLiveCoding -WaitMutex` succeeded.
- Full automation: `Automation RunTests DroneProto; Quit` found 65 tests, 65 succeeded, 0 failed, exit code 0.

### Manual Follow-up
- PIE/manual movement feel for Drain 0.9 speed remains unverified.

---

## 2026-07-08 - Q3 RaidState Drafting flow

### Scope
- Applied Q3 from `docs/Audit/NextWorkQueue_CurrentSpec_20260708.md`.
- Formalized the global raid state flow as Waiting -> Drafting -> Battle -> End while keeping `PlayerSelectionState` separate.
- Kept selection timer, AutoReady timer handling, part return/loadout paths, RPC, replication, UMG, assets, and BossState untouched.

### Changes
- `ARaidGameMode::PostLogin()` moves Waiting raids to Drafting when the first server-side player enters.
- `ARaidPlayerController::Server_RequestSelectPart_Implementation()` also moves Waiting to Drafting on real selection entry, covering test worlds and selection-stage entry.
- `ProcessReadyForRaidForServer()` rejects Ready while RaidState is End and only promotes Waiting/Drafting to Battle.
- `ARaidGameState::SetRaidStateForServer()` now logs named `[DR_SUMMARY] RaidState Previous=... New=...` transitions.

### Verification
- RED: `Automation RunTests DroneProto.Q3.RaidState.DraftingFlow; Quit` failed before the production edit on missing Drafting and End -> Battle re-entry.
- GREEN: same Q3 test passed after the production edit and logged Waiting -> Drafting -> Battle -> End.
- Build: `Build.bat DroneProtoEditor Win64 Development -Project="D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject" -NoLiveCoding -WaitMutex` succeeded.
- Full automation: `Automation RunTests DroneProto; Quit` found 66 tests, 66 succeeded, 0 failed, exit code 0.

### Manual Follow-up
- PIE/manual UI reaction to the named RaidState log and OnRep path remains unverified.

---

## 2026-07-08 - Q4 BossState and raid join gate

### Scope
- Applied Q4 from `docs/Audit/NextWorkQueue_CurrentSpec_20260708.md`.
- Added server-owned BossState Spawn/Battle/Dead/Clear while keeping the existing HP-based `IsDefeated()` check.
- Added explicit join/Ready rejection for BossDead, BossClear, TimeOver, and invalid RaidState without changing PlayerSelectionState, RaidState, loadout, inventory, return, pattern, or stun formulas.

### Changes
- `ARaidBoss` now replicates one new `BossState` value with `OnRep_BossState` log and BP visual hook.
- `SetBossStateForServer()` handles server-authoritative transitions and `[DR_SUMMARY] BossState` logs.
- Boss pattern start promotes Spawn -> Battle, HP reaching 0 promotes Battle -> Dead, and RaidEnd completion promotes Dead/Battle -> Clear.
- `ARaidGameMode::CanAcceptRaidJoinForServer()` gates Waiting/Drafting/Battle joins against Dead/Clear/TimeOver.
- `ProcessReadyForRaidForServer()` uses the new gate before loadout/equipped state changes and logs `[DR_SUMMARY] RaidJoinRejected`.

### Verification
- RED: Q4 test compile failed before production code because `EBossState`, `GetBossState()`, and `CanAcceptRaidJoinForServer()` did not exist.
- GREEN: `Automation RunTests DroneProto.Q4.RaidBoss.BossStateJoinGate; Quit` passed after implementation.
- Build: `Build.bat DroneProtoEditor Win64 Development -Project="D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject" -NoLiveCoding -WaitMutex` succeeded.
- Full automation: `Automation RunTests DroneProto; Quit` found 67 tests, 67 succeeded, 0 failed, exit code 0.

### Manual Follow-up
- PIE/manual OnRep_BossState client visual-hook firing remains unverified.

---

## 2026-07-09 - Q5 DataTable schema and part-count fallback

### Scope
- Applied Q5 from `docs/Audit/NextWorkQueue_CurrentSpec_20260708.md`.
- Prepared C++ DataTable row schemas for part counts, cores, weapons, bonuses, and grades.
- Kept `.uasset` DataTable creation/import, UMG, maps, formulas, RPC, replication, FastArray conversion, inventory return, Ready, Q1 boss HP, Q2 core modifiers, Q3 RaidState, and Q4 BossState behavior unchanged.

### Changes
- Added `FDronePartCountRow`, `FDroneCoreRow`, `FDroneWeaponRow`, `FDroneBonusRow`, and `FDroneGradeRow` in `Raid/DroneDataTableRows.h`.
- Added an `EditDefaultsOnly` `PartCountDataTable` candidate to `ADronePartInventory`.
- `ADronePartInventory` can now load selectable part stock rows from a `UDataTable` during server BeginPlay.
- DataTable-missing or invalid-table fallback keeps the existing hardcoded stock values: Zenith 5, Booster 6, Drain 5, Pulse 11, Fracture 10, Vector 11.
- Renamed file-local log helper functions in Q3/Q4 raid files to avoid UE unity-build anonymous-namespace collisions; no state formula or transition behavior changed.

### Verification
- Baseline before Q5 edits: `Automation RunTests DroneProto; Quit` found 67 tests, 67 succeeded, 0 failed, exit code 0.
- RED: Build failed after the Q5 tests were added because `Raid/DroneDataTableRows.h` did not exist yet.
- GREEN: `Automation RunTests DroneProto.Q5.DataTable; Quit` found 3 tests, 3 succeeded, 0 failed, exit code 0.
- Build: `Build.bat DroneProtoEditor Win64 Development -Project="D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject" -NoLiveCoding -WaitMutex` succeeded.
- Full automation: `Automation RunTests DroneProto; Quit` found 70 tests, 70 succeeded, 0 failed, exit code 0.

### Manual Follow-up
- Editor `.uasset` DataTable creation/import remains unperformed by Codex and is still an owner/editor task.

---

## 2026-07-09 - Q6 Raid timer replication and boss HUD parent

### Scope
- Applied Q6 from `docs/Audit/NextWorkQueue_CurrentSpec_20260708.md`.
- Prepared C++ plumbing for boss HP and the 03:00 raid timer without UMG layout, assets, maps, AddToViewport automation, or client-authoritative timer decisions.
- Kept boss HP/damage/pattern/stun values, RaidState, BossState, Report, Return, Inventory, Loadout, RPC, and existing UI presentation paths unchanged.

### Changes
- Added one replicated `RaidTimeEndServerTime` value to `ARaidGameState` with `OnRep_RaidTimeEndServerTime` logs and UI refresh marker.
- `ARaidGameMode::StartRaidTimeLimitTimerForServer()` records the server end time when the 180-second raid timer starts, and timer cleanup clears it.
- Added `UBossHUDWidget` as a C++ UMG parent with optional `BossHPText`, `RaidTimerText`, and `BossHPProgressBar` bindings.
- `UBossHUDWidget` exposes `GetBossHPPercent()`, `GetBossHPText()`, `GetRaidRemainingSeconds()`, `GetRaidTimerText()`, and `RefreshBossHUD()`.

### Verification
- RED: Build failed after Q6 tests were added because `Raid/BossHUDWidget.h` did not exist yet.
- GREEN: `Automation RunTests DroneProto.Q6.RaidHUD; Quit` found 2 tests, 2 succeeded, 0 failed, exit code 0.
- Build: `Build.bat DroneProtoEditor Win64 Development -Project="D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject" -NoLiveCoding -WaitMutex` succeeded.
- Full automation: `Automation RunTests DroneProto; Quit` found 72 tests, 72 succeeded, 0 failed, exit code 0.

### Manual Follow-up
- PIE/manual UMG binding and visual placement for the actual boss HUD remains unverified and is still an owner/editor task.

---

## 2026-07-09 - Q7 Raid load failure return-to-lobby hook

### Scope
- Applied Q7 from `docs/Audit/NextWorkQueue_CurrentSpec_20260708.md`.
- Added a C++ return-to-lobby hook for explicit raid load/spawn failure paths without UMG layout, assets, maps, ReportWidget success return path, inventory, return, loadout, RaidEnd, or matchmaking-server changes.

### Changes
- Added one owning-client RPC, `ARaidPlayerController::Client_NotifyRaidLoadFailed(FName Reason, FName TargetMap)`.
- Added `HandleRaidLoadFailedForClient()` and `BP_OnRaidLoadFailed()` so Blueprint/UI can react to failure without Codex creating popup assets.
- Client failure handling logs `[DR_SUMMARY] RaidLoadFailed` and uses a guarded `ReturnToLobby` path targeting `LobbyMap`.
- `ARaidGameMode::SpawnDefaultPawnAtTransform_Implementation()` now notifies the affected raid PlayerController only when required spawn context is missing or all spawn attempts fail.
- Automation-only test shim mirrors the client handler after the server notification condition because the mock world has no owning client NetConnection to deliver the RPC.

### Verification
- RED: Q7 build failed after tests were added because the load-failure PC helper and spawn-failure test hook did not exist yet.
- GREEN: `Automation RunTests DroneProto.Q7.RaidLoadFailed; Quit` found 2 tests, 2 succeeded, 0 failed, exit code 0.
- Build: `Build.bat DroneProtoEditor Win64 Development -Project="D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject" -NoLiveCoding -WaitMutex` succeeded.
- Full automation: `Automation RunTests DroneProto; Quit` found 74 tests, 74 succeeded, 0 failed, exit code 0.

### Manual Follow-up
- PIE/manual popup Blueprint implementation and real map/spawn failure UX remain unverified and are still owner/editor tasks.
