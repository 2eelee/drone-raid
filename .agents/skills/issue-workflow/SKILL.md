---
name: issue-workflow
description: Use when a DroneProto request names a Linear POR issue and expects focused implementation, verification, documentation, or a read-only dry-run without a broad repository re-audit.
---

# DroneProto Issue Workflow

Treat the Linear issue as scope, source specs as requirements, and code/tests as implementation truth. Load context progressively.

## Invoke

Use `$issue-workflow POR-123 <short instruction>`. If the identifier is missing, ask only for it. Treat `dry-run` as strictly read-only: do not edit files, run mutating tests/builds, change git, write ADRs, comment on Linear, or update issue state.

## Guardrails

- Capture `git status --short --branch` and HEAD first. Preserve unrelated dirty files. Stop only when they overlap the issue; never stash or switch them automatically.
- Read Linear by default. Never move an issue to `Done` without explicit approval in the current conversation.
- Do not touch tutorial code, UMG, assets, maps, or adjacent systems unless the issue requires them.
- Do not create another implementation map, history document, converter, graph DB, hook, or command wrapper.
- Never open Codex `MEMORY.md` or rollout summaries; use Codebase Memory MCP only.

## Load the Minimum Context

1. Read the Linear issue and relations. Extract requirement, acceptance criteria, keywords, and current state.
2. Use `rg -n -C 2` on `docs/AI/DRONERAID_DOC_MAP_20260708.md` to select one source spec, then on that spec and `docs/Audit/ImplementationMap_Current.md` for matching headings/rows. Never use `Get-Content -Raw` on routed documents.
3. Use the already-injected `AGENTS.md`. Read it from disk only when absent or changed.
4. Query Codebase Memory project `DroneProto` with issue keywords or known symbols. Start with `search_graph` or compact `search_code`, limit 10. Do not request full architecture unless targeted searches fail.
5. Call Code Review Graph `get_minimal_context` first; pass `changed_files=[]` during planning/dry-run so unrelated dirty work is excluded. Before files are known, search one exact symbol. Afterward request minimal impact with only planned files and depth 1. Request flows only when callers remain unclear or shared lifecycle paths are involved. Confirm candidates in `.h/.cpp` and tests with `rg`.
6. Read only definitions, callers, authority/replication boundaries, and directly related tests.

If a selected source lacks a current Markdown derivative, reuse `.local-tools/ConvertSpecs.ps1` for that single file in normal work. In `dry-run`, use MarkItDown to convert only to a temporary path, run targeted `rg`, then remove it. If the repo venv launcher is stale:

1. Call `codex_app.load_workspace_dependencies` for its Python 3.12 path.
2. Run that Python with `-c "import runpy,sys; sys.path.insert(0,r'<repo>\.local-tools\markitdown\.venv\Lib\site-packages'); sys.argv=['markitdown',r'<source>', '-o',r'<temp>']; runpy.run_module('markitdown.__main__',run_name='__main__')"`.

If the dependency loader is unavailable, use the existing Code Review Graph Python at `C:\Users\RJW-DESKTOP\AppData\Local\pipx\pipx\venvs\code-review-graph\Scripts\python.exe` with the same command.

Never unzip or parse DOCX/XML as a substitute; report MarkItDown unavailable instead of installing another converter.

## Evidence Gate

Do not scope, implement, or summarize until every slot is filled:

- [ ] Linear requirement and acceptance criteria
- [ ] One matching source-spec section, read from current Markdown or MarkItDown output
- [ ] Matching `ImplementationMap_Current` row
- [ ] Targeted Codebase Memory result
- [ ] Code Review Graph minimal context plus exact symbol
- [ ] Live definition and caller in `.h/.cpp`
- [ ] Direct test declaration/body

If a slot is unavailable, report that blocker instead of inferring the missing evidence.

## Fix the Scope Card

Before editing, emit exactly four short lines:

```text
Scope: <one acceptance slice>
Exclude: <adjacent work>
Files: <expected files>
Verify: <RED, GREEN, build, regression/PIE>
```

Ask before proceeding only when the issue needs a whole audit, broad refactor, destructive action, or unresolved product decision.

## Implement and Verify

- Reuse existing server-authoritative damage, return, report, and replication paths. Apply Ponytail: the smallest root-cause change that satisfies acceptance criteria.
- For behavior changes, run the smallest direct RED test, implement, then run it GREEN.
- Run the UE editor build for C++ changes. Run directly related automation first.
- Run the full `DroneProto` suite when shared state, networking, authority, replication, lifecycle, common return/inventory/damage paths, or multiple systems are affected. Otherwise record the targeted result. Never skip required build/tests to save tokens.
- Use PIE only for network, replication, UI, map, or asset evidence automation cannot prove.

## Record Only Proven Results

| Condition | Action |
|---|---|
| Structural path or status changed | Update only affected rows and current tracking in `ImplementationMap_Current.md` |
| Verified implementation changed | Append actual files, commands, counts, and result to `docs/DEVLOG.md`; never read prior DEVLOG when the map, current output, and commit already prove the issue |
| Rules/current next step changed | Update `AGENTS.md` |
| Durable design choice or reusable root cause | Read with `manage_adr --project DroneProto --mode list`, then merge only the relevant section with `--mode update`; skip session facts |
| Work completed | Produce a Linear-ready summary; keep state unchanged until approval |

## Output Contract

Return at most six evidence-first lines:

```text
POR-123 — <result>
Changed: <files or none>
Verified: <commands and counts, or not run>
Docs/Memory: <updated or unchanged>
Remaining: <manual boundary or none>
Linear: <ready to link; Done unchanged pending approval>
```

For `dry-run`, replace `Docs/Memory` with one compact `Read:` line listing only the sources actually used. Confirm code, git, files, ADR, and Linear were unchanged. Emit nothing after the six-line block.

## Common Mistakes

- Broad graph impact before identifying files: search symbols first.
- Reading all specs or DEVLOG for history: use the doc map, exact rows, git diff, and targeted memory.
- Treating graph/memory as proof: confirm live code and current test output.
- Repeating stale-doc cleanup inside the issue: list it only as a follow-up candidate.
