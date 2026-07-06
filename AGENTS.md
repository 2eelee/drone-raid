# DroneProto — 드론 MMORPG 프로토타입

## 프로젝트 개요
- UE 5.7, C++/Blueprint 하이브리드
- 장르: PvE 보스 레이드 + 드론 조립 MMORPG
- 핵심: 서버 전체가 공유하는 한정된 부품 풀로 드론을 설계해 보스 레이드
- 개발 기록/트러블슈팅 원문: docs/DEVLOG.md 확인

## 이슈 관리 (Linear)
- 이 프로젝트의 작업 추적은 Linear `DroneProto` 프로젝트를 기본으로 한다.
- 기본 라우팅: Team `Portfolio`, Project `DroneProto`, Assignee `me`, 초기 상태 `Backlog`.
- 새 기능/버그/검증 보강처럼 범위가 있는 작업은 Linear 이슈를 만들거나 기존 이슈에 연결한 뒤 진행한다.
- 작업 브랜치/커밋/DEVLOG 항목에는 가능하면 Linear 이슈 ID(`POR-5` 형식)를 함께 남긴다.
- `docs/DEVLOG.md`는 구현 이력/트러블슈팅의 canonical 기록이고, 작업 우선순위와 백로그 관리는 Linear를 기준으로 한다.
- 현재 감사 기준 문서: `docs/ImplementationAudit_DroneRaid.md`; 해당 감사 결과는 Linear `Implementation Audit Snapshot - 2026-06-27` 문서와 POR-5~POR-10 이슈에 반영되어 있다.

## 네트워크 아키텍처 (핵심)
- Dedicated Server, 서버 권한(server authoritative) 구조
- 4명 검증 → 16명 목표 (관심영역 최적화는 확장 단계)
- 진짜 값은 서버에만, 클라는 RepProp로 받아 표현
- 전투: 타겟팅 공격(명중 100%) / 액션 회피(보스 패턴 직접 피함)

## 핵심 시스템
- 공유 부품 풀: 서버 단독 관리, Server RPC 요청 + 동시성 처리(중복 선택 방지)
- 사망 시 부품 환원 → 풀에 복귀
- 보스 상태(스턴 등)일 때 데미지 배율
- 드론 전투 부품 효과: Z 입력 1회에 좌/우 무기 동시 발동, 서버에서 무기 피해/코어 배율/흡혈 회복 계산
- 드론 이동: 클라는 이동 축 입력만 Server RPC로 전달하고, 서버가 `UFloatingPawnMovement` 이동 적용/위치 복제/이동거리 누적을 담당

## 코딩 규칙
- Unreal 네이밍: A=Actor, U=UObject, F=struct
- 서버 권한 변경은 HasAuthority() 가드
- Replicated 변수는 GetLifetimeReplicatedProps에 등록 + OnRep 콜백
- 소스 파일 인코딩은 UTF-8 (BOM 없이) — VS 기본 CP949로 저장하면 mojibake 발생
- GameMode = 서버 전용 로직, GameState = 클라에 노출할 상태. 클라가 읽을 값은 GameState에 Replicated로
- Build.cs `PublicIncludePaths` 남용 금지 — 하위 IncludePath 오설정으로 UHT generated include가 깨진 적 있음. 가능하면 모듈 루트(`Source/DroneProto`) 기본 탐색을 사용하고, 새 소스는 루트/기존 구조에 맞춘다.

## 프로젝트 설정 주의사항
- `VisualStudioTools` 플러그인은 `Enabled: false` 유지 — 활성화 시 빌드 규칙 충돌로 모듈 전체 빌드 차단
- `DefaultEngine.ini` GlobalDefaultGameMode 변경만으로는 부족 — 레벨 World Settings GameMode Override도 함께 변경해야 함 (레벨 설정이 .ini보다 우선)

## 확정 설계 결정
- 부품 선점 = 선착순(서버 수신 순서). `DronePartInventory`는 서버 요청 순차처리로 race 방지 → 락·경매큐 불필요
- 선택 타이머 15초 (기존 미확정/60초에서 변경)
- `ServerState`(인스턴스 가용성) vs `RaidState`(레이드 진행단계) 분리 — 합치지 않음
- 매치메이킹 레이어는 게임 서버 위 별도 계층. 단일 인스턴스는 전역 정원 불인지
- `RecalculateStats`는 드래프팅(전투 전)에서만 호출 — 전투 중 호출 시 Health=MaxHealth로 풀피 회복 버그
- 이동거리 기반 효과 계산은 별도 단계로 분리. D8에서 서버 권한 이동 입력과 Vector/Booster용 이동거리 누적 기반을 만들었고, 이후 Vector Cannon 피해 보너스와 Booster Core 공격/속도 보너스 계산 기반을 서버 경로에 연결했다.
- 이동거리 누적은 서버 ActorLocation delta 기준. `InBattle + Alive`일 때만 누적하고 Spawn/Possess/Loadout/Death/RaidEnd/FirstSample/TooLargeDelta/Teleport성 delta는 reset 또는 ignore 처리.
- `VectorAccumulatedMoveDistanceMeters`와 `BoosterAccumulatedMoveDistanceMeters`는 분리 유지. Vector 공격 후에는 Vector 누적만 reset하고, Booster 누적은 공격으로 reset하지 않는다.
- Drain Core 회복은 서버에서 실제 Boss HP 감소량 기준으로 1회 Z 입력당 최대 3까지만 적용한다.
- RaidEnd 또는 BossDead 이후에는 `PlayerSelectionState == InBattle`만으로 공격/이동을 허용하지 않는다. 서버에서 `RaidState`, Boss defeated, Boss HP를 함께 확인해 입력을 무시한다.
- DroneReport는 서버가 생성하고 owning client RPC로 표시 요청한다. 클라이언트는 Report UI 표시와 복제값 읽기만 담당한다.

## 현재 상태 / 다음 단계
- 현재: D11 기반. D5 선택/Ready/AutoReady, D6 DeathReturn/RaidEndReturn/Dead 차단, D7 전투 효과, D8 서버 권한 이동거리 누적, D9 Vector/Booster 계산, D10/D11 DroneReport 표시 기반까지 연결.
- TestMap PIE 2 Players 기준 1차 루프 동작: 부품 선택 → Ready → InBattle → Boss 피격 → BossDeath → RaidEnd → DroneReport 표시 → 부품 반환.
- D7-D9 전투 효과: Pulse Laser는 슬롯별 독립 3타 카운트(8/8/18), Fracture Burst는 11 damage/HitCount 4, Vector Cannon은 이동거리 기반 보너스 후 Vector 누적 reset, Zenith Core는 HP 비율 단계 보너스, Booster Core는 이동거리 기반 공격/속도 보너스 계산, Drain Core는 피해량 기반 최대 3 회복.
- D8 이동 기반: `ADrone::Move -> Server_SetMoveInput -> ApplyMoveInputForServer -> ApplyPendingServerMoveInputForServer` 경로로 축 입력만 서버에 전달하고, 서버 Tick에서 위치 이동/복제/Vector-Booster 이동거리 누적을 처리.
- D6 테스트 경로: `D6KillDrone`은 현재 콘솔/owning PlayerController의 Pawn을 사망 처리하고, `D6RaidEndReturn`은 레이드 종료 반환 경로를 실행한다. 에디터/서버 콘솔에서는 서버 쪽 PC가 대상이 될 수 있으므로 `[DR_SUMMARY] D6KillDrone RequestPC=... TargetPC=... TargetDrone=...` 로그로 대상 확인.
- RaidEnd 이후 Z 입력은 `[DR_SUMMARY] Attack Ignored: Reason=RaidEnd` 또는 `Reason=BossDead`가 떠야 하며, `Attack Accepted` / `BossDamage OldHP=0.00`가 남으면 회귀.
- RaidEnd 이후 이동 입력은 `[DR_SUMMARY] MoveInput ... Result=Ignored Reason=RaidEnd` 또는 `[DR_SUMMARY] ServerMoveIgnored ... Reason=RaidEnd`가 떠야 하며, `ServerMoveApplied`가 남으면 회귀.
- 검증: `Build.bat DroneProtoEditor Win64 Development -Project="D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject" -NoLiveCoding -WaitMutex` 성공, `Automation RunTests DroneProto.D6.RaidGameMode.RaidEndReturnClearsInBattlePlayers` 성공. 전체 자동화 재실행은 앱 사용량 제한으로 추가 확인 필요.
- 다음: ContributionManager / DataTable 전환 / 보스 패턴 / VFX / Report UI polish를 별도 범위로 진행.

## 보류 (D11)
- 팝업 위젯 클래스 지정 + `IsSlotEnabled`(현재 dead) 정리
- `ServerState` 구현 (인스턴스 가용성)
- 만석/배정/매칭대기/재탐색 예외 경로 (멀티 전제, 현재 stub)

## 보류 (D6 이후)
- UMG 배치/디자인/아이콘 polish.
- 보이는 보스 액터/HP UI polish.
- D8 서버 이동 경로의 2 Clients PIE 체감/복제 추가 튜닝. 코드 기준 서버 이동/owner-only 보정 기반은 마련됐지만, 로컬 체감은 별도 조정 대상.
- ContributionManager / DataTable 전환 / 보스 패턴 / VFX.

## D7 PIE / 로그 검색어
- `[DR_SUMMARY] PulseAttack PC=`
- `[DR_SUMMARY] FractureAttack PC=`
- `[DR_SUMMARY] ZenithBonus PC=`
- `[DR_SUMMARY] DrainHeal PC=`
- `[DR_SUMMARY] Attack PC=`
- `[DR_SUMMARY] DeadInputIgnored PC=`

## D8 PIE / 로그 검색어
- `[DR_SUMMARY] MoveInput PC=`
- `[DR_SUMMARY] MoveAudit PC=`
- `[DR_SUMMARY] MoveDistance PC=`
- `[DR_SUMMARY] MoveDistanceIgnored PC=`
- `[DR_SUMMARY] MoveDistanceReset PC=`

## D9-D11 PIE / 로그 검색어
- `[DR_SUMMARY] WeaponCalc Player=`
- `[DR_SUMMARY] CoreCalc Player=`
- `[DR_SUMMARY] Attack Accepted: Player=`
- `[DR_SUMMARY] Attack Ignored: Reason=RaidEnd`
- `[DR_SUMMARY] Attack Ignored: Reason=BossDead`
- `[DR_SUMMARY] BossDamage: OldHP=`
- `[DR_SUMMARY] BossDeath:`
- `[DR_SUMMARY] BossDamageIgnored: Reason=BossDead`
- `[DR_SUMMARY] CombatRecord Player=`
- `[DR_SUMMARY] ReportCreated Player=`
- `[DR_SUMMARY] ReportWidgetShown Player=`
- `[DR_SUMMARY] RaidEndStateCleaned Player=`
- `[DR_SUMMARY] RaidEndClientRefresh Player=`
- `[DR_SUMMARY] ServerMoveIgnored PC=`

## 하지 말 것
- 게임 변형(Combat/Platforming 등) 없음 — 단일 게임
- 클라에서 직접 권한 데이터 변경 금지

## DroneRaid Codex Reporting Rules

### 1. 작업 시작 전 확인
- 모든 구현/감사 작업 시작 전 `git branch --show-current`와 `git status --short`를 확인한다.
- 관련 Linear issue key/title/status/link를 확인하고, 없으면 범위가 있는 작업인지 판단해 생성 또는 연결한다.
- 작업 범위와 금지 범위를 먼저 요약한다.

### 2. 구현 완료 보고 필수 항목
- 수정 파일 목록
- 연결 Linear issue
- 서버 실행 위치
- 클라이언트 실행 위치
- 추가/변경 RPC
- Replicate/OnRep 변경 여부
- 권한 검증 위치
- 자동화 테스트 결과
- `git diff --check` 결과
- staged 여부
- PIE에서 볼 `DR_SUMMARY` 검색어
- 수동 미검증 항목
- 건드리지 않은 항목

### 3. DronePartInventory / 선택 / 취소 / 교체 / 반환 작업 규칙
- 재고 차감/반환/교체는 서버 단일 경로인지 먼저 확인한다.
- 교체 순서는 반드시 “새 부품 재고 확인 → 가능할 때만 기존 부품 반환 → 새 부품 차감 → 슬롯 갱신”이어야 한다.
- 반환 중복 방지는 로그가 아니라 슬롯 상태 `None`/`Invalid` 처리 여부로 판단한다.
- 선택 취소, 교체, 선택 중 접속 종료, 전투 중 접속 종료, 사망, 레이드 종료가 중복 반환을 만들지 않는지 확인한다.

### 4. UI 작업 규칙
- UMG 배치, `.uasset`, `.umap`은 Codex가 임의 수정하지 않는다.
- C++ 부모 위젯, `BindWidgetOptional`, `BlueprintCallable`/`BlueprintPure` getter, `Refresh`/`OnRep`/`ClientRPC` 연결만 담당한다.
- UI는 로컬 컨트롤러에서만 `AddToViewport` 한다.

### 5. 로그 / 검증 규칙
- 전체 PIE 로그를 요구하지 않는다.
- 가능한 `DR_SUMMARY` 검색어와 성공/실패 기준으로 보고한다.
- Move 계열 반복 로그는 사건성 로그가 아니면 throttle/summary 기준을 유지한다.
- 사건성 로그는 유지한다:
  - Select / Cancel / Return / Ready
  - Attack Accepted / Ignored
  - BossDamage / BossDeath / RaidEnd
  - DroneReport / ReportWidgetShown
  - CombatVisual Attack / BossDamaged / Telegraph / DroneDamaged
  - Dodge Started / VisualHidden / VisualShown / End

### 6. 구현 금지 기본값
- `git add`/commit 금지. 사용자가 명시할 때만 한다.
- UMG, `.uasset`, `.umap` 수정 금지.
- 큰 구조 리팩터링 금지.
- 서버 권한 판정을 클라로 옮기지 않는다.
- 새 RPC/Replicated 변수는 필요성과 책임 분리를 보고한 뒤 최소로만 추가한다.

## DroneRaid Naming Rules
- D 번호는 `현현_개발마일스톤`의 원래 Day 번호 기준이다.
- 새 작업마다 D 번호를 자동 증가시키지 않는다.
- 마일스톤 이후 추가 보강/감사/로그 정리 작업은 Linear POR 번호 중심으로 기록한다.
- 자동화 테스트 이름에 남은 D 번호는 호환상 유지할 수 있지만, devlog 제목/커밋 메시지/작업명은 POR 번호와 실제 작업명 중심으로 작성한다.
- 예: `2026-07-06 — POR-14/POR-15 전투 가시화 및 로그 의미 정리`
