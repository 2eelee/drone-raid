# DroneProto

## 프로젝트 요약

DroneProto는 UE5.7 기반 멀티플레이 PvE 드론 레이드 프로토타입이다. 플레이어는 서버가 관리하는 공유 부품 풀에서 드론을 조립하고, Ready 후 레이드에 입장해 서버 제어 보스와 전투한 뒤 서버가 생성한 전투 리포트를 받는다.

현재 README의 기준 스냅샷은 POR-16~19 작업을 한 번에 봉인한 커밋 `6f5bfd2`다.

## 핵심 기능

- 서버 권한 부품 선택, 취소, 교체, 반환, Ready 흐름.
- 서버 권한 타겟팅 공격, 보스 피해, 레이드 종료, 리포트 생성 흐름.
- 서버 소유 이동/회피 처리와 아레나 경계 및 보스 최소 거리 클램프.
- 공유 부품 재고 기반 중복 선택 및 중복 반환 방지.
- 보스 패턴/스턴 서버 흐름과 스턴 피해 배율 처리.
- DroneReport 생성, 중복 리포트 방지, owning client 표시 요청.

## 현재 상태

- POR-16 보스 타겟팅, POR-17 이동 클램프, POR-18 방어 가드, POR-19 보스 패턴/스턴 작업은 `6f5bfd2` 커밋 하나로 봉인되어 있다.
- 부품 선택, Ready, 전투, 타겟팅, 이동/회피, 보스 상태, 리포트 생성의 핵심 권한은 서버에 남아 있다.
- 타겟 마커와 스턴 상태 같은 클라이언트 시각 요소는 아직 2 Client PIE 수동 확인이 필요하다.

## 검증

- 자동화: `Automation RunTests DroneProto` 기준 `64/64 PASS`, `EXIT CODE 0`.
- 자동화 검증 범위: 서버 권한 로직, 선택/반환 흐름, 중복 방지, 이동/회피 클램프, 보스 패턴/스턴 서버 흐름.
- 안전 요약: 서버 측 자동화 64/64는 통과했고, 클라이언트 시각 검증은 수동 PIE 대기 상태다.

## 수동 PIE 체크리스트

- Target Set Result=Success
- owner client target marker only
- MoveClamp Boundary/BossMinDistance
- BossPattern Started/Fired/Stopped
- DroneReport DuplicateIgnored
- StatsRecalcIgnored: Reason=InBattle
- DebugBossSetStunned 1
- StunMultiplier=1.50
- CombatVisual BossStunChanged

## 알려진 한계 / 다음 단계

- 2 Client PIE에서 owner-only target marker 동작 확인.
- 2 Client PIE에서 stun visual `OnRep` 동작 확인.
- 현재 에디터 화면/로그 기준 수동 체크리스트 증거 캡처.
- UI 배치, 보스 비주얼, VFX, DataTable 전환, Contribution/Report 표시 polish는 별도 범위로 진행.
