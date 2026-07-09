# DroneProto

## 프로젝트 요약

DroneProto는 UE 5.7 기반 멀티플레이 PvE 드론 레이드 프로토타입이다. 플레이어는 서버가 관리하는 공유 부품 풀에서 드론을 조립하고, Ready 후 레이드에 입장해 서버 권한 보스와 전투한 뒤 서버가 생성한 전투 리포트를 받는다.

현재 README 기준 스냅샷은 Q1~Q10 stacked branch `codex/q10-tutorial-state-skeleton` / 커밋 `8df38e4`다. 이 브랜치는 Q1~Q10을 선형으로 포함하는 통합 후보이며, Q1 직전 문서 정리 커밋 `154d20e`도 함께 포함한다.

## 핵심 기능

- 서버 권한 부품 선택, 취소, 교체, 반환, Ready 흐름.
- 서버 권한 타겟팅 공격, 보스 피해, 레이드 종료, 리포트 생성 흐름.
- 서버 소유 이동/회피 처리와 아레나 경계 및 보스 최소 거리 클램프.
- 공유 부품 재고 기반 중복 선택 및 중복 반환 방지.
- 보스 HP 60,000, 180초 레이드 타이머, BossState 기반 입장/Ready gate.
- 코어/무기 전투 계산: Booster/Drain 기본 페널티, Drain 회복 상한, Vector/Booster 이동거리 효과.
- DataTable 전환 준비: C++ row schema와 PartCountDataTable fallback.
- BossHUDWidget, RaidLoadFailed, Tutorial 상태머신 등 UMG/에디터 연결용 C++ hook.
- 서버 전용 중앙 보스 데미지 기여도 map. DroneReport 계산 소스는 기존 CombatRecord 유지.

## 현재 상태

- Q1: Boss MaxHP/CurrentHP 기본값을 60,000으로 정합.
- Q2: Core base modifier 적용. Booster AttackModifier 0.95, Drain AttackModifier 0.85, Drain MoveSpeedModifier 0.9.
- Q3: 전역 RaidState를 Waiting -> Drafting -> Battle -> End로 정식화.
- Q4: BossState Spawn/Battle/Dead/Clear 추가 및 Dead/Clear/TimeOver join gate 추가.
- Q5: DataTable row schema와 PartCountDataTable fallback 준비. `.uasset` DataTable 생성/임포트는 미수행.
- Q6: RaidTimeEndServerTime 복제와 BossHUDWidget C++ getter 준비.
- Q7: raid load/spawn failure 시 owning client 통지와 ReturnToLobby C++ hook 추가.
- Q8: server-only central boss damage contribution map 추가. Report UI/점수식은 미변경.
- Q9: 보스 패턴/스턴은 source spec 전까지 placeholder boundary로 봉인. 동작 변경 없음.
- Q10: 튜토리얼 C++ 상태 뼈대만 추가. 맵, UMG, SaveGame, 로그인, 대사/연출은 미구현.

## 서버/클라 책임 요약

- 서버: `RaidGameMode`, `RaidGameState`, `RaidBoss`, `DronePartInventory`, `ADrone`의 공격/반환/리포트/타이머/기여도 계산.
- 클라: UMG C++ 부모 getter, OnRep/ClientRPC 수신, BP hook, 로비 복귀 표현.
- 추가 RPC: Q7 `Client_NotifyRaidLoadFailed(FName Reason, FName TargetMap)` 1개.
- 추가 Replicate/OnRep: Q4 `BossState`, Q6 `RaidTimeEndServerTime`.
- 반환/재고/Loadout 경로: Q1~Q10에서 구조 변경 없음. 기존 서버 단일 경로 유지.

## 검증

- Build: `Build.bat DroneProtoEditor Win64 Development -Project="D:\Documents\Unreal Projects\DroneProto\DroneProto.uproject" -NoLiveCoding -WaitMutex` 성공.
- 자동화: `Automation RunTests DroneProto; Quit` 기준 `79/79 success`, `0 fail`, `EXIT CODE 0`.
- 자동화 검증 범위: 서버 권한 로직, 선택/반환/Ready, 전투 계산, RaidState/BossState gate, DataTable fallback, RaidTimer/BossHUD getter, RaidLoadFailed hook, Contribution map, Q9 boundary, Tutorial state skeleton.

## 남은 수동 PIE 검증

- 2 Client PIE 보스 패턴 실피격:
  - `[DR_SUMMARY] BossAttackHit`
  - `[DR_SUMMARY] CombatVisual DroneDamaged`
- 클라 OnRep/BP visual:
  - `[DR_SUMMARY] BossState`
  - `[DR_SUMMARY] RaidTimer`
  - `[DR_SUMMARY] Tutorial Step=`
- UMG 배치/바인딩:
  - `BossHUDWidget`
  - Tutorial 안내 위젯
  - RaidLoadFailed 팝업
- 에디터/오너 작업:
  - DataTable `.uasset` 생성/임포트
  - 튜토리얼 맵/대사/연출/첫 실행 저장
  - 보스 비주얼, VFX, Report/Contribution 표시 polish
