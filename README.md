# drone-raid (언리얼 프로젝트명: DroneProto)
서버 권한 기반 드론 조립 PvE MMORPG 프로토타입 (UE 5.7)

## 현재 상태
- D5 완료: 서버 공유 드론 부품 선택, 15초 AutoReady, 수동 Ready, 부분/빈 슬롯 출격, Selecting 공격 차단, InBattle Z 공격으로 Boss HP 감소까지 확인.
- TestMap PIE 2 Players에서 두 PlayerController 모두 `BP_Drone_C` possess 및 `[DR_SUMMARY]` 기반 수동 검증 완료.
- 자동화: `Automation RunTests DroneProto` 전체 12개 성공.
- 빌드: `Build.bat DroneProtoEditor Win64 Development -NoLiveCoding` 성공.

## 핵심 기술
- Dedicated Server, 서버 권한 전투
- 공유 부품 풀 동시성 처리
- PlayerController Server RPC 기반 부품 선택/취소/Ready
- GameState/Inventory Replication 기반 UI 표시
- 플레이어별 `SelectionState`와 전역 `RaidState` 분리
- 최소 전투 루프: 장착 좌/우 무기 계산 → RaidBoss HP 감소
- 4명 → 16명 확장 구조

## 실행 방법

### 사전 준비
1. Visual Studio 2022 + "Game development with C++" 워크로드 설치
2. Git LFS 설치 → 설치 후 한 번만:  git lfs install
   ⚠️ LFS 없이 clone하면 .uasset 등 에셋이 깨져서 받아짐

### 빌드
1. git clone (LFS 설치 후)
2. DroneProto.uproject 우클릭 → Generate Visual Studio project files
3. 빌드 후 .uproject 실행

## 수동 검증 기준
- `TestMap` PIE 2 Players 실행.
- `[DR_SUMMARY] Spawn ... IsADrone=true` 확인.
- `[DR_SUMMARY] SelectTimerStart ... Duration=15.00` 확인.
- Selecting 상태 공격은 `[DR_SUMMARY] AttackIgnored ... Reason=NotInBattle`만 출력되어야 함.
- Ready/AutoReady 후 InBattle 상태에서 Z 공격 시 `[DR_SUMMARY] Attack ... Damage=... BossHP=...` 확인.
- PIE 종료 시 장착 플레이어의 EquippedParts 반환 로그와 Count Max 초과 여부 확인.

## 다음 작업
- D5 종료 반환 summary 최종 확인 후 D6로 이동.
- D6 후보: 보스 더미 시각화/HP UI 또는 드론 사망 반환 검증.
- Booster/Drain/Vector 세부 효과, ContributionManager, DroneReport, DataTable 전환, 보스 패턴/VFX는 D6 이후 별도 범위.
