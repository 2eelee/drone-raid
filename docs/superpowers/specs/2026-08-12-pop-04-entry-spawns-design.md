# POP-04 레이드 입장 스폰 설계

## 목표

`POP-04` 원문에 맞춰 초기 입장과 전투 중 후입장을 모두 전투 맵의 지정 스폰 포인트 4개 중 하나에 무작위 생성한다. 네 지점은 보스 중심에서 35m, 서로 90도 간격이며 여러 플레이어가 같은 지점을 선택할 수 있다. 빈 레이드 후입장은 Pawn 생성 완료 후 생존 전투자 수가 0에서 1로 바뀐 경우에만 기존 보스 패턴 재시작 경로를 호출한다.

## 범위

- `TestMap`의 기존 PlayerStart 2개를 제거하고 `RaidEntrySpawn` 태그가 붙은 PlayerStart 4개를 배치한다.
- 네 지점은 월드 원점의 보스를 기준으로 `(3500, 0)`, `(0, 3500)`, `(-3500, 0)`, `(0, -3500)` cm에 둔다.
- PlayerStart 캡슐 바닥이 현재 Floor에 닿도록 중심 Z를 `92cm`로 통일하고 각 지점은 보스를 향하게 한다.
- `ARaidGameMode`는 태그된 네 지점만 후보로 사용하고 컨트롤러별 최초 선택 시 균등 무작위로 하나를 고른다.
- 서로 다른 컨트롤러는 같은 PlayerStart를 선택할 수 있다. 같은 컨트롤러의 반복 조회만 최초 선택을 유지한다.
- Pawn 생성 성공 뒤 보스 패턴 시스템에 population 재평가를 알린다.

다음은 변경하지 않는다.

- A → B → C 서버 배정, reservation token, 수용 인원과 입장 거부 규칙
- 플레이어 간 충돌 설정과 신규 입장 보호시간
- 튜토리얼, UMG, Blueprint, 보스 공격 패턴 자체
- 기존 사망·Logout·Ready/선택 상태 기반 population 알림

## 맵 배치

| 지점 | 위치 cm | 보스를 향하는 Yaw |
|---|---:|---:|
| East | `(3500, 0, 92)` | `180°` |
| North | `(0, 3500, 92)` | `-90°` |
| West | `(-3500, 0, 92)` | `0°` |
| South | `(0, -3500, 92)` | `90°` |

네 액터 모두 `RaidEntrySpawn` 태그를 가진다. 태그가 없는 PlayerStart는 레이드 입장 후보에서 제외한다. 이 태그 경계는 다른 맵에 PlayerStart가 추가되더라도 POP-04 후보가 의도치 않게 늘어나는 것을 막는다.

## 서버 실행 흐름

1. Unreal의 기존 입장 흐름이 `ChoosePlayerStart`를 호출한다.
2. 서버 GameMode는 유효한 `RaidEntrySpawn` PlayerStart를 이름순으로 수집한다.
3. 기존 컨트롤러 할당이 있으면 그대로 반환한다. 없으면 네 후보 중 하나를 `RandRange`로 선택하고 컨트롤러에 캐시한다.
4. 다른 컨트롤러가 이미 선택한 지점은 제외하지 않는다. 따라서 동일 지점 중복이 명시적으로 허용된다.
5. 기존 `RestartPlayer`/`SpawnDefaultPawnAtTransform` 경로로 Pawn을 생성한다.
6. `RestartPlayer`는 유효한 Pawn이 생긴 뒤 `RaidBoss.NotifyPatternPopulationChangedForServer("PlayerSpawnCompleted")`를 호출한다.
7. 초기 Waiting/Drafting 입장에서는 패턴이 실행 중이 아니므로 알림이 상태를 바꾸지 않는다.
8. Battle 중 빈 레이드 후입장은 Pawn과 `InBattle` 상태가 모두 갖춰진 population 알림에서 기존 `0 → 1` guard를 통과해 Corrupted를 0.5초 뒤 재시작한다.
9. 기존 생존자가 있으면 count가 `1 → 2` 이상이므로 패턴을 초기화하지 않는다.

## 실패 처리

- 태그된 PlayerStart가 없으면 단일행 `[DR_SUMMARY]` 오류를 남기고 `Super::ChoosePlayerStart_Implementation`으로 안전하게 fallback한다.
- 후보가 4개가 아닌 경우에도 fallback은 유지하되 구성 오류를 로그로 노출한다. `TestMap` 자동화는 정확히 4개가 아니면 실패한다.
- Pawn 생성 실패는 기존 `NotifyRaidSpawnFailedForServer` 경로를 그대로 사용하며 population 완료 알림을 보내지 않는다.
- 신규 입장 보호시간은 추가하지 않는다. 기존 패턴 피격 후 0.7초 HitLock은 입장 보호가 아니라 피해 중복 방지이므로 그대로 둔다.

## 검증 설계

1. 기존 PlayerStart 할당 테스트를 RED로 바꿔 네 tagged 후보만 선택하고, 동일 seed의 두 컨트롤러가 같은 지점을 받을 수 있으며, 동일 컨트롤러 재조회는 같은 지점을 유지함을 검증한다.
2. `TestMap` 계약 테스트를 RED로 바꿔 PlayerStart가 정확히 4개이고 모두 태그, 35m 반경, 90도 간격, 보스 방향 회전을 만족하는지 검증한다.
3. spawn 완료 통합 테스트를 추가해 Battle/빈 population에서 실제 Pawn 생성 전에는 paused 상태이고 생성 완료 뒤에만 first delay로 전환되는지 검증한다.
4. 기존 `DroneProto.BossPattern.Population.PauseAndRestart`와 `DroneProto.POR18.Arena`를 재실행한다.
5. `ARaidGameMode`와 접속/spawn 생명주기를 변경하므로 UE 5.7 Editor Build 후 전체 `DroneProto` 자동화를 실행한다.
6. 맵 배치와 실제 랜덤 입장 위치는 GUI PIE 경계로 기록하며, 자동화로 확인하지 못한 시각·충돌 결과를 과거 기록으로 대체하지 않는다.

## 문서 반영

구현과 검증이 끝나면 `ImplementationMap_Current.md`의 현재 작업 추적과 `POP-04` 행만 갱신하고, 실제 명령·테스트 수·결과를 `docs/DEVLOG.md`에 append한다. 현재 상태와 다음 단계가 바뀌므로 `AGENTS.md`도 같은 작업에서 갱신한다.
