# 밸런스 텔레메트리 정책

## 목적

밸런스 텔레메트리는 부품 조합, 플레이어 공격, 회피, 보스 패턴, 사망과 최종 리포트를 비교하기 위한 익명 gameplay 데이터다. 운영 장애 감시나 계정 추적 용도로 사용하지 않는다.

## 수집 원칙

- 게임 서버는 SQLite에 연결하지 않고 서버 권한 판정 지점에서 `[DR_SUMMARY] Telemetry Schema=1` 한 줄 로그만 기록한다.
- 호출부호, UniqueNetId, PlayerId, PlayerName, PC 이름, 계정, IP와 주소는 이벤트와 DB에 저장하지 않는다.
- 플레이어는 세션 안에서만 유효한 `P1`, `P2` 별칭을 사용한다. 서로 다른 세션의 플레이어를 연결하지 않는다.
- 매 프레임 위치와 Tick은 수집하지 않는다. 이동거리, 생존시간 등은 기존 서버 집계 결과만 기록한다.
- `PIE`, `Standalone`, `DedicatedServer`, `Automation`을 구분하며 `Automation`은 기본 밸런스 집계에서 제외한다.

## 저장과 확인

- 원본 Unreal 로그, SQLite DB, CSV와 Streamlit 가상환경은 `Saved/BalanceTelemetry/**`에 두며 Git에 추가하지 않는다.
- 검토가 끝난 익명 집계와 검증 결과만 `docs/Results/BalanceTelemetry/**`에 공유할 수 있다.
- 원본 이벤트는 완료 세션 기준 90일 뒤 삭제할 수 있다. 익명 세션·플레이어 요약과 집계 결과는 비교를 위해 유지한다.
- 같은 `(Session, Seq)` 이벤트는 한 번만 적재한다. 종료 이벤트가 없는 세션은 `Aborted`로 유지한다.

## 기획 판단

- 완료 세션 20개 미만은 동작 확인과 탐색용으로만 보고 수치 변경의 근거로 사용하지 않는다.
- 밸런스 값을 변경할 때 `BalanceVersion`을 올린다. 빌드만 바뀐 경우에는 `BuildVersion`으로 분리한다.
- PIE 데이터는 빠른 개발 확인, Dedicated Server 데이터는 최종 밸런스 판단에 사용한다.
- CSV는 외부 공유나 포트폴리오 자료가 필요할 때만 명시적으로 내보낸다.

## 서버 운영 경계

서버에는 게임 실행 파일과 로그 보존만 필요하다. 서버 로그 자동 다운로드는 실제 서버의 OS, 호스팅 업체, SFTP/API 또는 영구 볼륨 방식을 최초 smoke에서 확인한 뒤 별도 작업으로 구성한다. SQLite와 Streamlit은 개인 개발 PC에서 실행한다.
