# DroneProto

## Project Summary

DroneProto is a UE5.7 multiplayer PvE drone raid prototype. Players assemble drones from a shared part pool, ready up, enter a raid, fight a server-controlled boss, and receive a server-generated combat report.

The current public status is based on the POR-16~19 integration snapshot sealed in commit `6f5bfd2`.

## Core Features

- Server-authoritative part selection, cancellation, replacement, return, and Ready flow.
- Server-authoritative combat flow for targeting attacks, boss damage, raid end, and report creation.
- Server-owned movement and dodge handling, including movement clamps for arena bounds and boss minimum distance.
- Shared part inventory with duplicate-selection and duplicate-return protections.
- Boss pattern and stun server flow, including stun damage multiplier handling.
- DroneReport generation with duplicate-report guard and owning-client display request.

## Current Status

- POR-16 boss targeting, POR-17 movement clamp, POR-18 defensive guards, and POR-19 boss pattern/stun work are sealed together in `6f5bfd2`.
- Raid-critical authority remains on the server for part selection, Ready, combat, targeting, movement/dodge, boss state, and report creation.
- Client-facing visuals for target markers and stun state still need manual 2 Client PIE confirmation.

## Verification

- Automation: `Automation RunTests DroneProto` reported `64/64 PASS`, `EXIT CODE 0`.
- Covered automation areas: server authority logic, selection/return flow, duplicate prevention, movement/dodge clamp behavior, and boss pattern/stun server flow.
- Safe status summary: server-side automation 64/64 passed, client visual checks pending manual PIE.

## Manual PIE Checklist

- Target Set Result=Success
- owner client target marker only
- MoveClamp Boundary/BossMinDistance
- BossPattern Started/Fired/Stopped
- DroneReport DuplicateIgnored
- StatsRecalcIgnored: Reason=InBattle
- DebugBossSetStunned 1
- StunMultiplier=1.50
- CombatVisual BossStunChanged

## Known Limits / Next Steps

- Verify owner-only target marker behavior in 2 Client PIE.
- Verify stun visual `OnRep` behavior in 2 Client PIE.
- Capture current in-editor screen/log evidence for the manual checklist.
- Continue later polish for UI layout, boss visuals, VFX, DataTable conversion, and contribution/report presentation as separate scopes.
