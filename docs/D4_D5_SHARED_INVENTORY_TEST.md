# D4/D5 Shared Inventory UI Multiplayer Test

Date: 2026-06-20

## Confirmed

- PIE 2 Clients reached `CurrentPlayers=2`.
- Client received `OnRep_DronePartInventory`.
- `ADronePartInventory` replicated with `bReplicates=true` and `bAlwaysRelevant=true`.
- `OnRep_PartStocks` refreshed client stock counts.
- Server-side part selection refreshed both clients' UI through replicated stock updates.
- `RequestReadyForRaid` logged each player's selected Core/Left/Right parts.

## Cleanup

- `ADronePartInventory` constructor uses direct `bReplicates = true` for pre-init actor setup.
- High-frequency stock detail, UI refresh, and widget refresh logs were lowered to `Verbose`/`VeryVerbose`.
- Kept the important test logs: `PostLogin CurrentPlayers`, select/cancel/return results, ready request, and initial `OnRep_DronePartInventory`.
- Kept `TODO(D5 CombatStart): pass selected parts to Drone::ApplyLoadout before battle transition.`

## PIE 2 Clients Smoke Checklist

- `CurrentPlayers=2` is logged.
- Client logs initial `OnRep_DronePartInventory`.
- Client logs `OnRep_PartStocks`.
- Selecting a part on one client updates the other client's visible stock count.
- Ready logs include Core/Left/Right selected part IDs for each player.

## Next

- Wire the combat-start flow so selected parts are passed into `Drone::ApplyLoadout`.
- Continue using PIE 2 Clients as the manual smoke test for shared stock visibility and first-come stock consumption.
