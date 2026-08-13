# Balance Telemetry Smoke - 2026-08-13

## 결과

- UE 5.7 `DroneProtoEditor Win64 Development`: 성공
- `DroneProto.Telemetry`: 6/6
- 인접 자동화: D7 4/4, D17 3/3, BossPattern DamageGate 1/1, D10 2/2
- Python importer/dashboard query: 9/9
- 실제 UE 텔레메트리 로그: 13건 삽입, 동일 로그 재수집 0건
- CSV export: `loadout_balance.csv`, `pattern_balance.csv`, `raid_balance.csv`
- CSV 금지 식별자 검색: 0건
- Streamlit headless smoke: 프로세스 유지 및 `127.0.0.1:8514` listen 성공 후 종료
- 대시보드 확장: 8개 탭, 완료 세션 토글, 공격·피해/생존·회피·원본 이벤트 조회, Streamlit 기본 chart
- Streamlit AppTest: 기본 필터와 `Automation + 중단 세션 포함` 두 경로 모두 8 tabs, 0 exceptions. 후자는 dataframe 7개 렌더링
- 확장 Streamlit headless smoke: `127.0.0.1:8515` listen 성공 후 종료

## 미실행 경계

- 신규 RPC·복제·게임플레이 판정을 추가하지 않아 전체 `DroneProto` 회귀는 실행하지 않았다.
- 대시보드 확장은 Python/Streamlit 조회·표시만 변경해 UE Build·Automation을 재실행하지 않았다.
- GUI PIE와 실제 Dedicated Server 로그 다운로드는 미실행이다. 다운로드 자동화는 서버 OS·호스팅·로그 경로가 확정된 뒤 연결한다.
