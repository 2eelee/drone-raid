# Git 추적·제외 정책

이 문서는 저장소에서 무엇을 Git으로 공유하고 무엇을 로컬에만 둘지 정한다. 실제 적용 규칙의 기준은 저장소 루트 `.gitignore`다.

## 추적 대상

- 게임 소스와 설정: `Source/`, `Config/`, `Content/`
- 프로젝트 설명: `README.md`
- 검증이 끝난 결과 문서: `docs/Results/**`
- 저장소 운영 정책: `docs/Policies/**`
- 공유 프로젝트 스킬: `.agents/skills/**`
- 밸런스 로그 수집·분석 도구: `Tools/BalanceTelemetry/**`

## 로컬 전용

- 기획 원본과 변환본: `docs/sources/**`, `docs/sources_converted/**`
- 기획을 바탕으로 작성한 설계안, 구현 계획, 작업 초안
- UI 배치 가이드 등 작업 중 참고 문서
- `docs/DEVLOG.md`
- `docs/Audit/**`의 현행 감사·계획과 보관 문서
- `Saved/`, 로그, 빌드 산출물
- `Saved/BalanceTelemetry/**`의 inbox, SQLite, CSV, Streamlit 가상환경
- `.codex/`, `.code-review-graph/`, `.agents/skills/` 이외의 `.agents` 상태
- `CLAUDE.md`, `AGENTS.md`, `.claude/`

`docs/Audit/Archive/**`는 디렉터리 전체를 자동 추적하지 않는다. 공유가 필요한 최종 감사 결과는 범위를 검토한 뒤 `.gitignore`에 파일별 예외를 추가한다.

## 문서 배치

| 문서 성격 | 경로 | Git |
|---|---|---|
| 최종 검증 결과 | `docs/Results/` | 추적 |
| 저장소 운영 정책 | `docs/Policies/` | 추적 |
| 현행 구현 기준선과 감사·계획 | `docs/Audit/` | 로컬 |
| 완료·대체된 감사·계획 | `docs/Audit/Archive/` | 로컬, 필요 시 파일별 허용 |
| 기획 원본·변환본 | `docs/sources/`, `docs/sources_converted/` | 로컬 |
| 누적 작업일지 | `docs/DEVLOG.md` | 로컬 |

## 유지 절차

1. 새 문서가 검증 결과나 저장소 정책인지 먼저 판단한다.
2. 결과면 `docs/Results/`, 정책이면 `docs/Policies/`에 둔다.
3. 공유 스킬은 `.agents/skills/<skill-name>/SKILL.md`에 둔다.
4. 제외된 파일은 `git add -f`로 우회하지 않고 필요한 예외를 `.gitignore`에 명시한다.
5. 규칙 변경 전후에 아래 명령으로 실제 적용 위치를 확인한다.

```powershell
git check-ignore -v --no-index -- <path>
git ls-files -- <path>
git status --short --ignored
```

이미 추적 중인 파일은 `.gitignore`만 바꿔도 추적이 멈추지 않는다. 추적 종료가 필요하면 해당 파일의 보존 여부를 확인한 뒤 별도 작업으로 처리한다.

정책을 바꿀 때는 `.gitignore`와 이 문서를 함께 검토한다.
