# 레이드 입장 실패 처리 설계

## 목표

`ENTRY-06`의 매칭 10초 실패와 `ENTRY-10`의 레이드 맵 로드 10초 초과·실패를 하나의 클라이언트 실패 처리 경로로 정리한다. 기존 서버 예약, single-use token, spawn 실패 Client RPC, 로비 UI 상태 전환을 재사용하며 UMG·Blueprint·에셋은 수정하지 않는다.

## 범위

- 매칭 대기 10초 만료는 기존처럼 `NoServerAvailable`로 기록하고 로비의 `NoServer` 상태를 표시한다.
- 예약 성공 후 실제 `OpenLevel` 또는 `ClientTravel` 직전에 별도의 10초 로드 watchdog을 시작한다.
- 대상 월드가 로드되면 watchdog을 해제한다.
- engine travel failure 또는 watchdog 만료는 `MapLoadFailed`로 한 번만 처리한다.
- 실패 시 이미 로비이면 기존 native popup fallback을 즉시 표시한다. 로비가 아니면 실패 표시를 pending으로 보존하고 `LobbyMap` 복귀를 한 번만 요청한 뒤 새 로비 world에서 popup을 표시한다.
- 서버의 Pawn spawn 실패는 기존 `Client_NotifyRaidLoadFailed` 경로를 유지한다.

## 구조와 실행 흐름

`URaidSessionSubsystem`은 GameInstance 수명이라 로비 월드에서 시작한 입장 요청의 상태를 travel 경계 너머까지 보유한다. subsystem 초기화 시 map-load/travel-failure delegate를 연결하고 종료 시 모두 해제한다.

1. `RequestRaidEntry`가 새 요청을 시작하면 이전 매칭 retry와 이전 load watchdog을 정리한다.
2. assignment가 `Waiting`이면 기존 1초 retry와 10초 매칭 제한을 사용한다.
3. assignment가 `Success`이면 Loading 상태를 표시하고, source world와 endpoint를 snapshot한 뒤 load watchdog을 시작한다.
4. `TravelToRaidEndpoint`가 `OpenLevel` 또는 `ClientTravel`을 호출한다.
5. 같은 GameInstance의 새 world가 post-load되면 성공으로 판정해 watchdog과 pending endpoint를 해제한다.
6. engine travel failure 또는 10초 만료가 먼저 발생하면 pending 요청을 원자적으로 종료하고 `MapLoadFailed` 결과를 기록한다.
7. 실패 처리는 중복 guard를 거친다. 현재 world가 이미 `LobbyMap`이면 popup을 표시하고 그 자리에 머문다. 아니면 실패 표시를 pending으로 남기고 `LobbyMap` 복귀를 한 번만 요청하며, 로비 world post-load에서 popup을 표시한다.

테스트에서 실제 travel을 억제한 경우에도 watchdog 시작·성공 해제·만료를 직접 구동할 수 있는 최소 test seam을 둔다.

## 오류 처리

- world 또는 local PlayerController가 travel 호출 전에 없으면 기존 즉시 `MapLoadFailed` 경로를 사용하며 watchdog을 남기지 않는다.
- timeout, engine failure, spawn failure가 겹쳐도 첫 실패만 UI와 로비 복귀를 수행한다.
- 새 입장 요청과 사용자의 매칭 취소는 이전 pending travel 상태를 정리한다.
- 로드 실패의 사용자 표시는 신규 에셋 없이 현행 `ShowLoadFailed`/native `NoServer` panel fallback을 재사용한다. 로그의 `FailReason=MapLoadFailed`와 `DebugReason`으로 NoServer 실패와 구분한다.

## 검증

직접 자동화는 RED 후 다음 계약을 확인한다.

- 매칭 10초 만료가 retry를 중단하고 `NoServerAvailable`을 한 번 표시한다.
- 성공 assignment가 load watchdog을 시작한다.
- 새 world 성공 통지가 watchdog을 해제해 이후 timeout이 무효다.
- watchdog 만료가 `MapLoadFailed`를 기록하고 popup/로비 복귀를 한 번만 요청한다.
- travel failure와 timeout 중복 입력이 두 번째 UI 또는 travel을 만들지 않는다.
- travel 호출 전 world/PlayerController 부재가 즉시 실패하고 pending watchdog을 남기지 않는다.

변경 후 UE 5.7 Editor Build와 `RaidEntry`·`RaidLoadFailed` 직접 테스트를 실행한다. GameInstance/world 전환 공용 경계를 건드리므로 전체 `DroneProto` 자동화도 실행한다. 실제 네트워크 단절과 10초 체감은 필요 시 2 Client PIE 경계로 남긴다.

## 비범위

- Dedicated Server A/B/C 실프로세스 및 16/17 동시 접속 검증
- 예약·token·server ledger 변경
- UMG/Blueprint/맵/팝업 에셋 수정
- VFX와 DataTable 전환
