---
name: issue-workflow
description: Use when a DroneProto request names a Linear POR issue and expects focused implementation, verification, documentation, or a read-only dry-run without a broad repository re-audit.
---

# DroneProto Issue Workflow

Treat the Linear issue as scope, source specs as requirements, and code/tests as implementation truth. Load context progressively.

## Invoke

Use `$issue-workflow POR-123 <short instruction>`. If the identifier is missing, ask only for it. Treat `dry-run` as strictly read-only: do not edit files, run mutating tests/builds, change git, write ADRs, comment on Linear, or update issue state.

## Guardrails

- Capture `git status --short --branch` and HEAD at start/end; recheck only after an unexpected change. Preserve unrelated dirty files; never stash/switch automatically.
- Read Linear by default. Never move an issue to `Done` without explicit approval in the current conversation.
- Do not touch tutorial code, UMG, assets, maps, or adjacent systems unless the issue requires them.
- Do not create another implementation map, history document, converter, graph DB, hook, or command wrapper.
- Never open Codex `MEMORY.md` or rollout summaries; use Codebase Memory only when its lane requires it.
- Use one documented fallback per failed tool; then report the blocker.

## Choose the Smallest Lane

| Lane | Use when | Minimum evidence |
|---|---|---|
| `dry-run` | Read-only planning or audit | Issue body plus only the sources needed for the requested claim |
| `docs-only` | No implementation-status claim | Issue body and affected canonical document section |
| `known-symbol` | Acceptance, files, symbols, and direct test are identifiable | Issue body, source section, map row, live definition/caller, direct test |
| `shared-unknown` | Symbols, callers, authority boundary, or multi-system impact is unclear | `known-symbol` evidence plus conditional memory/graph narrowing |

For `docs-only` work that changes implementation status, require `known-symbol` evidence.

## Fast Evidence Ladder

1. Batch status/HEAD with the Linear body/current state. Read relations/comments only for a named dependency/blocker or unclear acceptance; search sources/code after keywords are fixed.
2. If Linear gives a source or requirement ID, search it directly. Otherwise search the issue-title keywords in `docs/sources/*.md` and select the latest matching source. Read only its matching section and `ImplementationMap_Current.md` row; never raw-read unrelated documents.
3. If exact symbols/files are known, batch targeted `rg` for definitions, callers, authority/replication boundaries, and direct tests. Skip both graph tools.
4. For an unknown symbol, query Codebase Memory once with limit 5. If callers/shared lifecycle remain unclear, call Code Review Graph `get_minimal_context` for one exact symbol and depth 1. Pass `changed_files=[]` during planning/dry-run, then only planned files. Use the second tool only for a named gap; request flows only for lifecycle ambiguity.
5. Confirm candidates in live `.h/.cpp` and tests. Graph or memory output is never proof.
6. Stop when the lane's minimum evidence is filled. Reuse same-turn evidence until HEAD, Linear, or a relevant file changes. After editing, reread the diff and changed definitions instead of whole files.

Use the already-injected `AGENTS.md`; read it only when absent or changed. If the selected source lacks current Markdown, read [references/spec-conversion.md](references/spec-conversion.md) and convert only that source.

Escalate one lane when evidence conflicts, the exact symbol/caller is unknown, multiple systems share the path, a Blueprint/asset boundary affects acceptance, no direct test exists, or authority/replication impact cannot be bounded. Report a blocker instead of inferring unavailable required evidence.

## Fix the Scope Card

Before editing, emit exactly four short lines:

```text
Scope: <lane; one acceptance slice>
Exclude: <adjacent work>
Files: <expected files>
Verify: <smallest direct test; build, broader suite, or PIE only with reason>
```

Ask only for a whole audit, broad refactor, destructive action, or unresolved product decision.

## Implement and Verify

For `known-symbol` or `shared-unknown` implementation, read [references/verification.md](references/verification.md) before editing and follow it. Do not load it for `dry-run` or pure `docs-only` work.

Reuse existing server-authoritative paths and make the smallest root-cause change that satisfies acceptance.

## Record Proven Results Once

| Condition | Action |
|---|---|
| Structural path/status changed | Update only affected map rows and current tracking |
| Verified implementation changed | Append actual files, commands, counts, and results to `docs/DEVLOG.md`; do not read prior DEVLOG |
| Rules/current next step changed | Update `AGENTS.md` |
| Durable design decision exists | Then, and only then, list/update the relevant Codebase Memory ADR section |
| Work completed | Produce a Linear-ready summary; keep state unchanged pending approval |

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

For `dry-run`, replace `Docs/Memory` with one compact `Read:` line and confirm code, git, files, ADR, and Linear were unchanged. Emit nothing after the block.
