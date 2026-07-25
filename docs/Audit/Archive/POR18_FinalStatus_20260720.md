# POR-18 최종 상태 — 2026-07-20

## 최종 판정

> POR-18은 서버 gameplay 및 deterministic visual geometry 코드 계약 기준으로 완료.
> 최종 VFX와 Pawn 높이 기반 수동 피격 sign-off는 별도 후속 작업.

이 판정은 UE 5.7 Build, 자동화, 정적 계약 검사와 Dedicated Server + headless Client 2개 접속 결과를 기준으로 한다. GUI PIE에서의 시각적 완성도나 수동 피격 성공을 의미하지 않는다.

## 통합 범위

통합 브랜치: `codex/por-18-integration`
기준: local `main` / `origin/main` `4cc0ee9`

| 구분 | 원본 커밋 | 통합 커밋 | 내용 |
|---|---:|---:|---|
| Pattern visual tuning | `8e2c89f` | `9d59cc6` | 공용 점선 debug 표시 기반 |
| Pattern visual tuning | `eb9d8fd` | `4ea1132` | 반복 로그 억제와 foreground 가시성 |
| Corrupted 표시 | `9ef9d65` | `2f4ac2c` | 4개 단일 채움 부채꼴 표시 |
| Stellar HitLock 회귀 | `752954f` | `09aa499` | 두 wave의 공용 0.7초 HitLock 계약 |
| Stellar visual geometry | `6546152` | `e661266` | visual-only 파편 Z/크기 적용 |
| Arena alignment | `1a98709` | `3a2dfdb` | 보스 transform, PlayerStart, 바닥 정렬 |

`1e88b69`은 전체 자동화 수를 99개로 적은 오래된 README 갱신이므로 제외했다.

## 완료된 코드 계약

- 패턴 상태 머신, 충돌 판정, 피해 적용, HitLock은 서버 권한 경로를 유지한다.
- Corrupted Actino는 4개 동시 궤적, XY ±25도, Z ±300cm, 800~5000cm 범위와 기존 충돌·시각 폭 계약을 유지한다.
- Stellar Remnant는 32개 피해 파편과 16개 visual-only 파편, 2 wave, 0.5초 간격, 두 번째 wave 11.25도 오프셋을 유지한다.
- 전역 HitLock은 0.7초이며 실제 HP 감소 뒤에만 생성된다.
- Stellar visual-only 파편에만 ±300cm Z 오프셋과 100~120cm full size가 적용된다.
- visual-only 파편은 `IsPointInsideSweptSample()`에서 즉시 제외되며 서버 피해·swept collision에 들어가지 않는다.
- 실제 피해 파편의 위치식, 충돌 반경 70cm, 피해 경로는 변경하지 않았다.
- 보스 root는 spawn transform `(600, 0, 100)cm`를 보존한다.
- 두 PlayerStart는 GameMode 단일 선택 경로로 서로 다르게 배정된다. TestMap 위치는 `(-400, 500, 192)cm`, `(-400, -500, 92)cm`이고 보스 XY 기준 거리는 각각 `1118.034cm(11.180m)`다.
- 바닥 중심은 보스 XY `(600, 0)cm`, 반경은 `5500cm(55m)`다. MoveClamp는 `5000cm(50m)`, 패턴 범위는 `800~5000cm(8~50m)`를 유지한다.
- 클라이언트는 복제된 `FBossPatternRepState.StartServerTime`과 `GetServerWorldTimeSeconds()`로 위치를 재구성한다.
- Dedicated Server에서는 debug 시각화를 생성하지 않는다.

## 네트워크 계약 확인

| 항목 | 판정 | 근거 |
|---|---|---|
| 서버 권한 상태·충돌·피해·HitLock | Match | `HasAuthority()` guard와 서버 전용 pattern component 경로 유지 |
| 보스 spawn transform | Match | 서버와 Client 1/2 모두 `(600, 0, 100)cm` 기록 |
| PlayerStart 선택 | Match | 서버 GameMode가 두 client를 서로 다른 위치에 spawn |
| 클라이언트 시간 재구성 | Match | 기존 단일 pattern state의 `StartServerTime`과 서버 시간식 유지 |
| Dedicated Server 시각화 제외 | Match | `NM_DedicatedServer` guard 유지 |
| 새 RPC | 없음 | `main...integration` diff에 새 Server/Client/NetMulticast marker 없음 |
| 새 복제/OnRep 필드 | 없음 | 기존 `PatternState` RepNotify 외 파편별 복제 추가 없음 |
| Client 1/2 pattern state 런타임 로그 대조 | 미확인 | 기존 client 로그에 pattern state 식별 marker가 없어 headless 실행에서 직접 비교 불가 |

## 검증 결과

- UE 5.7 `DroneProtoEditor Win64 Development`: 성공
- `DroneProto.BossPattern.StellarRemnant`: 4/4 성공
- `DroneProto.POR18.Arena`: 3/3 성공
- `DroneProto.BossPattern`: 17/17 성공
- `DroneProto`: 102/102 성공
- `git diff main...HEAD --check`: 성공
- Dedicated Server + headless Client 2개: 접속 성공, network failure marker 없음
- 서버 spawn: Client 1 `(-400, 500, 192)cm`, Client 2 `(-400, -500, 92)cm`
- 서버와 Client 1/2 보스 위치: 모두 `(600, 0, 100)cm`
- 서버 패턴 시작: `CorruptedActino`, `Active`, `InstanceID=1` 확인
- GUI PIE 자동화: 사용자 지시에 따라 재시도하지 않음

자동화 성공은 GUI PIE에서 패턴 피격이 일관되게 재현됐다는 증거로 사용하지 않는다.

## 미완료 및 후속 작업

- Corrupted의 꽃잎형 곡선 최종 실루엣
- Stellar trail, shard mesh, glow, 중심 폭발 등 최종 VFX
- 임시 Drone이 바닥에 일부 묻혀 보이는 문제
- 현재 GUI PIE에서 패턴 피격을 일관되게 재현하지 못한 수동 검증
- Pawn gameplay point Z=0과 실제 보이는 mesh 높이의 정합성 감사

위 항목은 POR-18 코드 계약을 변경하지 않는 별도 후속 작업으로 분리한다.

## 제외 및 보존 상태

- `RaidSessionSubsystem`, `DroneReportWidget`, `DronePartInventoryTests` 변경은 mixed stash `stash@{0}`에만 남아 있으며 통합하지 않았다.
- 원본 worktree의 ignore 대상 `AGENTS.md`, `docs/DEVLOG.md`는 자동 복사하거나 덮어쓰지 않았다.
- Niagara, Material, Blueprint, 최종 VFX와 추가 gameplay 보정은 수행하지 않았다.
- push와 `main` merge는 수행하지 않았다.
