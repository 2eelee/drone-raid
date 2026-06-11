# drone-raid (언리얼 프로젝트명: DroneProto)
서버 권한 기반 드론 조립 PvE MMORPG 프로토타입 (UE 5.7)

## 핵심 기술
- Dedicated Server, 서버 권한 전투
- 공유 부품 풀 동시성 처리
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
