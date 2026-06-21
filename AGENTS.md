# DroneProto — 드론 MMORPG 프로토타입

## 프로젝트 개요
- UE 5.7, C++/Blueprint 하이브리드
- 장르: PvE 보스 레이드 + 드론 조립 MMORPG
- 핵심: 서버 전체가 공유하는 한정된 부품 풀로 드론을 설계해 보스 레이드
- 개발 기록/트러블슈팅 원문: docs/DEVLOG.md 확인

## 네트워크 아키텍처 (핵심)
- Dedicated Server, 서버 권한(server authoritative) 구조
- 4명 검증 → 16명 목표 (관심영역 최적화는 확장 단계)
- 진짜 값은 서버에만, 클라는 RepProp로 받아 표현
- 전투: 타겟팅 공격(명중 100%) / 액션 회피(보스 패턴 직접 피함)

## 핵심 시스템
- 공유 부품 풀: 서버 단독 관리, Server RPC 요청 + 동시성 처리(중복 선택 방지)
- 사망 시 부품 환원 → 풀에 복귀
- 보스 상태(스턴 등)일 때 데미지 배율

## 코딩 규칙
- Unreal 네이밍: A=Actor, U=UObject, F=struct
- 서버 권한 변경은 HasAuthority() 가드
- Replicated 변수는 GetLifetimeReplicatedProps에 등록 + OnRep 콜백
- 소스 파일 인코딩은 UTF-8 (BOM 없이) — VS 기본 CP949로 저장하면 mojibake 발생
- GameMode = 서버 전용 로직, GameState = 클라에 노출할 상태. 클라가 읽을 값은 GameState에 Replicated로
- Build.cs `PublicIncludePaths` 남용 금지 — 하위 IncludePath 오설정으로 UHT generated include가 깨진 적 있음. 가능하면 모듈 루트(`Source/DroneProto`) 기본 탐색을 사용하고, 새 소스는 루트/기존 구조에 맞춘다.

## 프로젝트 설정 주의사항
- `VisualStudioTools` 플러그인은 `Enabled: false` 유지 — 활성화 시 빌드 규칙 충돌로 모듈 전체 빌드 차단
- `DefaultEngine.ini` GlobalDefaultGameMode 변경만으로는 부족 — 레벨 World Settings GameMode Override도 함께 변경해야 함 (레벨 설정이 .ini보다 우선)

## 확정 설계 결정
- 부품 선점 = 선착순(서버 수신 순서). `DronePartInventory`는 서버 요청 순차처리로 race 방지 → 락·경매큐 불필요
- 선택 타이머 15초 (기존 미확정/60초에서 변경)
- `ServerState`(인스턴스 가용성) vs `RaidState`(레이드 진행단계) 분리 — 합치지 않음
- 매치메이킹 레이어는 게임 서버 위 별도 계층. 단일 인스턴스는 전역 정원 불인지
- `RecalculateStats`는 드래프팅(전투 전)에서만 호출 — 전투 중 호출 시 Health=MaxHealth로 풀피 회복 버그

## 현재 상태 / 다음 단계
- 현재: D5 완료. TestMap PIE 2 Players에서 `BP_Drone_C` possess, 공유 재고 선택/취소/반환, 15초 AutoReady, 수동 Ready, Selecting 공격 차단, InBattle Z 공격으로 Boss HP 감소까지 확인.
- 검증: `Build.bat DroneProtoEditor Win64 Development -NoLiveCoding` 성공, `Automation RunTests DroneProto` 전체 12개 성공.
- 마지막 D5 확인: PIE 종료 시 EquippedParts 반환 summary와 Count Max 초과 여부만 최종 확인.
- 다음: D6 — 보스 더미 시각화/HP UI 또는 드론 사망 반환 검증 중 하나를 선택해 좁게 진행.

## 보류 (D11)
- 팝업 위젯 클래스 지정 + `IsSlotEnabled`(현재 dead) 정리
- `ServerState` 구현 (인스턴스 가용성)
- 만석/배정/매칭대기/재탐색 예외 경로 (멀티 전제, 현재 stub)

## 보류 (D6 이후)
- UMG 배치/디자인/아이콘 polish.
- 보이는 보스 액터/HP UI.
- 드론 사망 플로우 전체 검증.
- 레이드 종료 시 전체 플레이어 EquippedParts 반환.
- Booster Core 이동거리 보너스, Drain Core 흡혈, Vector Cannon 이동거리 피해 보너스.
- ContributionManager / DroneReport / DataTable 전환 / 보스 패턴 / VFX.
- FloatingPawnMovement 이동 동기화: autonomous proxy 로컬 이동만 → 서버 Pawn 제자리 → rubber-banding. Server RPC로 입력 전달 vs 클라 위치 보고 방식 결정 필요. (`Drone.cpp` 생성자 TODO 참조)

## 하지 말 것
- 게임 변형(Combat/Platforming 등) 없음 — 단일 게임
- 클라에서 직접 권한 데이터 변경 금지
