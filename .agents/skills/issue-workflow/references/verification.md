# Focused Unreal Verification

Read this file only for `known-symbol` or `shared-unknown` implementation.

## Direct RED and GREEN

- For behavior changes, run one smallest direct RED, implement, then run the same test GREEN. Add a test only for another acceptance criterion or reproduced edge case.
- An expected RED is evidence only after its exact expected assertion is confirmed. Capture the original exit code and output; after inspection, normalize to exit 0 only when that exact assertion is present. Preserve nonzero for crashes, timeouts, build failures, or different assertions so infrastructure/test errors remain visible.

## Windows Codex Execution

- Run `Build.bat` and `UnrealEditor-Cmd.exe` outside the filesystem sandbox on the first attempt because UBT and DDC write under `%LOCALAPPDATA%`. Use the already-approved command prefixes; do not probe inside the sandbox first.
- If `UnrealEditor-Cmd.exe` cannot run outside the sandbox, retry with `-DDC-ForceMemoryCache`.
- Run the UE editor build only for `.h`, `.cpp`, module, target, or build-configuration changes. Documentation, workflow, and test-selection changes require no Unreal build or automation.

## Escalation Ladder

- Broaden `direct test -> smallest named affected suite -> full DroneProto`. Before broadening, record `Escalate: <changed symbol -> confirmed callers -> uncovered risk -> next smallest suite>`.
- Use a named suite only when confirmed callers expose multiple uncovered contracts. Use the full suite only when no named suites bound a broad multi-system risk, large merge, central structure change, major spec revision, or confirmed implementation/spec inconsistency. A networking/replication label or shared-looking file alone is insufficient.
- If a broad run fails, iterate on the failed test and direct suite; repeat the broad run only as its final gate.
- Use PIE only for a required network, replication, UI, map, or asset criterion automation cannot prove.
- Stop after sufficient checks pass. Do not add suites for general confidence; record broader checks as `not required` with the missing escalation condition.
