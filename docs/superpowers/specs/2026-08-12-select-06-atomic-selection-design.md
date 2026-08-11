# SELECT-06 원자 부품 선택 오류 복구 설계

## 목표

선택 중 서버 내부 오류가 발생해도 공유 재고와 플레이어의 Core/Left/Right 선택 정보가 부분 변경 상태로 남지 않게 한다. 서버는 authoritative 3슬롯 snapshot을 owning client에 다시 전달하고, 선택 화면은 `일시적인 오류가 발생했습니다. 다시 시도해주세요.`를 눈에 보이는 팝업으로 표시한다.

## 기준선과 범위

- 시작 기준은 POP-04가 반영된 `main`의 `9e5ec9e`다.
- 대상 명세는 `SELECT-06`이며, 원자 교체가 직접 바꾸는 `REPLACE-01`~`REPLACE-04`도 다시 검증한다.
- 서버 선택 요청, 공유 재고, 기존 반환 로그, owning-client 결과 RPC, 선택 위젯의 C++ fallback만 수정한다.
- `WBP_DronePartSelect`, 다른 `.uasset`, `.umap`, VFX, 사운드는 수정하지 않는다.
- 일반 거부와 서버 내부 오류를 분리한다. 재고 부족·선택 잠금·잘못된 요청은 기존 일반 실패이고, 준비된 서버 의존성 부재나 서버 보유 선택/재고 불일치는 서버 오류다.

## 구조

### `ADronePartInventory`: 원자 재고 commit

`TryCommitSelectionExchange(PreviousPartID, NewPartID, OutFailureReason)`를 추가한다.

- authority, 새 부품 존재, 새 재고 `> 0`을 commit 전에 검증한다.
- `PreviousPartID`가 있으면 기존 부품 존재와 현재 수량 `< MaxCount`도 commit 전에 검증한다.
- 모든 검증이 성공한 뒤에만 `previous + 1`, `new - 1`을 같은 함수 호출에서 적용한다.
- 성공 시 `OnPartStocksChanged.Broadcast()`와 `ForceNetUpdate()`를 각각 한 번만 호출한다.
- 실패 시 `PartStocks`, delegate, replication dirty state를 변경하지 않는다.
- `PreviousPartID == NAME_None`은 최초 선택으로 처리한다. 같은 부품 재선택과 취소는 Controller의 기존 경로가 처리한다.

반환값은 `Success`, `OutOfStock`, `ServerError` 세 의미로 제한한다. 상세 내부 원인은 `OutFailureReason`과 서버 로그에만 사용한다.

### `UDronePartReturnManager`: 선택 변경 조정과 반환 감사 로그

`TryCommitSelectedPartChange(PC, Slot, NewPartID, OutFailureReason)`를 추가한다.

- 현재 슬롯의 기존 부품 ID를 읽어 Inventory 원자 API에 전달한다.
- 원자 재고 commit이 성공한 뒤에만 Controller의 선택 슬롯을 `NewPartID`로 바꾼다.
- 기존 부품이 있던 교체 성공에만 기존 `Replace` 반환 로그를 남긴다.
- 실패 시 반환 성공 로그나 슬롯 clear를 만들지 않는다.
- 취소·로그아웃·사망·RaidEnd 반환은 기존 `ReturnSingleSelectedPart`/`ReturnEquippedParts` 경로를 유지한다.

### `ARaidPlayerController`: 오류 분류와 authoritative snapshot RPC

`Server_RequestSelectPart` 시작 시 Core/Left/Right ID를 snapshot으로 캡처한다.

- 정상 선택과 교체는 ReturnManager의 원자 선택 변경 API를 호출한다.
- `OutOfStock`은 기존 일반 실패 결과를 보낸다. 팝업을 띄우지 않는다.
- Inventory/ReturnManager 부재, 서버 보유 기존 선택의 재고 불일치, 원자 API의 `ServerError`는 서버 오류로 처리한다.
- 서버 오류 시 세 슬롯을 시작 snapshot으로 명시적으로 복원한다. 원자 commit 전 실패이므로 재고 보상 연산은 없다.
- 전용 Reliable Client RPC에 Core/Left/Right snapshot만 보낸다. 내부 오류 문자열은 보내지 않는다.
- Client RPC는 세 로컬 선택 ID를 모두 authoritative snapshot으로 덮고 `OnSelectedPartsChanged`, UI refresh, 신규 `OnPartSelectionServerError` delegate를 순서대로 호출한다.

## 클라이언트 팝업

`UDronePartSelectWidget`은 `OnPartSelectionServerError`를 construct/destruct에서 bind/unbind한다.

- 표시 문구는 정확히 `일시적인 오류가 발생했습니다. 다시 시도해주세요.`다.
- 선택 화면에 `ServerErrorPopupPanel`/`ServerErrorPopupText` optional binding이 있으면 이를 사용한다.
- binding이 없으면 `ApplyPlanningLayout`이 중앙의 hit-test 불가 C++ Border/Text fallback을 생성한다.
- 팝업은 입력을 막지 않으며, 다음 선택 또는 취소 시도, 정상 선택 결과, 위젯 종료 때 숨긴다.
- 기존 `ResultText`는 일반 디버그 결과용으로 유지하고 팝업으로 재사용하지 않는다.
- 실제 WBP 외형·애니메이션은 이미지 레퍼런스를 받기 전까지 범위 밖이다.

## 서버 실행 흐름

1. Client가 기존 Server Reliable 선택 RPC를 보낸다.
2. Controller가 선택 상태, 슬롯, 새 부품 ID와 타입을 검증하고 3슬롯 snapshot을 캡처한다.
3. 재고 부족 같은 일반 거부는 기존 단일 결과 RPC로 종료한다.
4. ReturnManager가 기존 슬롯 ID와 새 ID를 Inventory 원자 API에 전달한다.
5. Inventory가 모든 수량을 사전 검증하고 한 번에 commit한다.
6. 성공하면 ReturnManager가 슬롯과 Replace 로그를 확정하고 Controller가 기존 성공 RPC를 보낸다.
7. 서버 오류면 재고와 반환 로그는 바뀌지 않고 Controller가 snapshot을 복원한 뒤 전용 Client RPC를 보낸다.
8. Client가 세 슬롯을 snapshot과 일치시키고 선택 UI를 refresh한 뒤 팝업을 표시한다.

## 오류 불변식

- 실패한 원자 선택 변경은 공유 재고 수량을 한 건도 바꾸지 않는다.
- 실패한 교체는 기존 슬롯 ID와 다른 두 슬롯 ID를 모두 유지한다.
- 실패한 교체는 성공한 `Replace` 반환 로그를 만들지 않는다.
- 일반 거부는 전용 서버 오류 팝업을 표시하지 않는다.
- 서버 오류는 내부 원인을 노출하지 않고 고정 사용자 문구만 표시한다.
- 성공 시 stock delegate/replication 갱신은 한 번이고, 클라이언트는 중간 반환 상태를 받지 않는다.

## 테스트 전략

1. Inventory 원자 API 테스트를 먼저 추가해 성공 교체, 최초 선택, 재고 부족 무변경, 기존 선택/재고 불일치 무변경을 RED에서 확인한다.
2. Controller/ReturnManager 통합 테스트를 추가해 성공 시 슬롯·두 재고·Replace 로그가 함께 commit되고, 서버 오류 시 3슬롯과 전체 관련 재고가 보존되는지 확인한다.
3. 전용 Client RPC 테스트로 authoritative 3슬롯 overwrite와 서버 오류 delegate 1회 발생을 검증한다.
4. 선택 위젯 테스트로 optional binding이 없는 현재 WBP에서도 fallback 팝업이 생성·표시되고 다음 시도에 숨겨지는지 확인한다.
5. 직접 `DroneProto.SELECT06` 테스트와 기존 `DroneProto.D5.DronePartReturnManager.ReturnAndReplace`, 선택 UI 테스트를 실행한다.
6. 공유 재고·반환·RPC 경계를 함께 바꾸므로 UE 5.7 Editor Build 후 전체 `DroneProto` 자동화를 실행한다. POP-04 반영 기준 기존 테스트 수는 141개다.
7. 실제 네트워크 owning-client 팝업 표시와 최종 시각 외형은 GUI PIE 경계로 기록한다.

## 문서 반영

- 구현 전 `ImplementationMap_Current.md`의 현재 작업 추적을 SELECT-06 브랜치와 시작 HEAD로 갱신한다.
- 검증 후 `SELECT-06`과 실제 영향받은 `REPLACE-01`~`REPLACE-04`만 다시 판정한다.
- 실제 Build/자동화 결과와 발견된 테스트 수를 `docs/DEVLOG.md`에 append한다.
- 현재 상태·다음 단계가 바뀐 내용을 `AGENTS.md`에 동기화한다. 기존의 오래된 `main=caad02c` 문구도 실제 `main` 기준선에 맞춘다.
