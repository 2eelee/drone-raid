# Balance Telemetry

게임 서버는 SQLite에 연결하지 않고 `[DR_SUMMARY] Telemetry Schema=1` 로그만 기록한다. 로컬 도구가 로그를 SQLite로 중복 없이 가져오며 대시보드는 SQLite를 직접 읽는다.

## 가장 간단한 사용법

1. 최초 한 번 `Tools/BalanceTelemetry/SetupBalanceDashboard.ps1`을 실행한다. 프로젝트의 `Saved/BalanceTelemetry/.venv`에만 설치된다.
2. PowerShell에서 `Tools/BalanceTelemetry/OpenBalanceDashboard.ps1`을 실행한다.

실행하면 로컬 에디터의 `Saved/Logs/DroneProto*.log`를 자동으로 가져온다. 나중에 받은 실제 서버 로그는 `Saved/BalanceTelemetry/inbox/`에 넣고 다시 실행하면 된다. 이미 가져온 세션과 이벤트는 중복 추가되지 않는다.

로컬 PIE, 자동화, 실제 Dedicated Server 데이터는 `Environment` 필터로 분리된다. `Automation`은 대시보드 기본 선택에서 제외된다.

대시보드는 `종합`, `부품`, `공격 분석`, `피해·생존`, `회피`, `보스 패턴`, `세션 품질`, `원본 이벤트` 8개 탭을 제공한다. 현재 DB에 자동화 데이터만 있다면 사이드바의 `환경`에서 `Automation`을 선택하고, 중단된 세션까지 보려면 `완료 세션만`을 끄면 된다.

## CLI

```powershell
& '.\Saved\BalanceTelemetry\.venv\Scripts\python.exe' '.\Tools\BalanceTelemetry\import_telemetry.py' import --input '.\Saved\Logs\DroneProto.log' --db '.\Saved\BalanceTelemetry\balance.db' --source-commit HEAD
& '.\Saved\BalanceTelemetry\.venv\Scripts\python.exe' '.\Tools\BalanceTelemetry\import_telemetry.py' export --db '.\Saved\BalanceTelemetry\balance.db' --output '.\Saved\BalanceTelemetry\export'
& '.\Saved\BalanceTelemetry\.venv\Scripts\python.exe' '.\Tools\BalanceTelemetry\import_telemetry.py' prune --db '.\Saved\BalanceTelemetry\balance.db' --older-than-days 90
```

CSV는 공유나 포트폴리오 자료가 필요할 때만 내보낸다. 원본 로그와 `balance.db`는 Git에 추가하지 않는다. 실제 서버 로그 자동 다운로드는 서버 OS와 호스팅 방식이 결정된 뒤 별도 구성한다.
