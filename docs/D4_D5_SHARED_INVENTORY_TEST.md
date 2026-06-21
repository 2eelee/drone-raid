# D4/D5 Shared Inventory and Ready Flow Multiplayer Test

Date: 2026-06-21

Note: `docs/DEVLOG.md` is the canonical development log. This file is only a compact PIE smoke checklist/evidence note.

## Confirmed

- TestMap PIE 2 Players에서 두 PlayerController 모두 `BP_Drone_C` 계열 Pawn possess.
- `[DR_SUMMARY] Spawn ... IsADrone=true` 확인.
- `[DR_SUMMARY] SelectTimerStart ... Duration=15.00` 확인.
- TimerText/`UIRefresh TimeLeft`가 15.00 이하로 표시됨.
- Client received `OnRep_DronePartInventory`.
- `ADronePartInventory` replicated with `bReplicates=true` and `bAlwaysRelevant=true`.
- `OnRep_PartStocks` refreshed client stock counts.
- Server-side part selection refreshed both clients' UI through replicated stock updates.
- 부품 선택 성공: `Core=CORE_002`, `Left=WEAPON_002`, `Right=WEAPON_002`.
- Selecting 상태에서 공격 입력 시 `[DR_SUMMARY] AttackIgnored ... Reason=NotInBattle` 출력.
- 수동 Ready 성공: `SelectTimerStop Reason=ManualReady`, `Ready Result=Success`, `SelectionState=Selecting->InBattle`, `AttackPower=22`.
- 아무 부품 없는 플레이어도 15초 뒤 AutoReady 성공: `Core=None Left=None Right=None AttackPower=0`.
- InBattle 상태에서 무기 장착 드론 Z 공격 시 `Damage=20.90`, Boss HP `979.10 -> 958.20 -> 937.30` 감소 확인.
- InBattle `UIRefresh`는 SelectedParts가 아니라 EquippedParts 기준으로 Core/Left/Right를 표시.
- `Build.bat DroneProtoEditor Win64 Development -NoLiveCoding` 성공.
- `Automation RunTests DroneProto` 전체 12개 성공.

## Cleanup

- `ADronePartInventory` constructor uses direct `bReplicates = true` for pre-init actor setup.
- High-frequency stock detail, UI refresh, and widget refresh logs were lowered to `Verbose`/`VeryVerbose`.
- Kept the important summary logs: spawn, select/cancel/return, ready, auto ready, attack ignored, attack, and UI refresh.
- `UIRefresh` summary is local-controller only and reports EquippedParts after InBattle.
- `GetSelectionRemainingTime()` clamps display to `0.0..15.0`.

## PIE 2 Clients Smoke Checklist

- `CurrentPlayers=2` 또는 `[DR_SUMMARY] Spawn ... IsADrone=true`가 두 플레이어 모두 출력된다.
- Client logs initial `OnRep_DronePartInventory`.
- Client logs `OnRep_PartStocks`.
- Selecting/cancel/replacing a part on one client updates the other client's visible stock count.
- Selecting 상태 공격은 `[DR_SUMMARY] AttackIgnored ... Reason=NotInBattle`만 출력되고 Boss HP를 줄이지 않는다.
- Manual Ready logs include `SelectionState=Selecting->InBattle` and equipped Core/Left/Right part IDs.
- AutoReady logs include `SelectionState=Selecting->InBattle`; empty loadout is valid.
- InBattle Z attack logs include `[DR_SUMMARY] Attack ... Damage=... BossHP=...`.
- PIE 종료 시 장착 플레이어는 `[DR_SUMMARY] Return ... Source=EquippedParts`가 실제 장착 슬롯별로 출력되고 Count가 Max를 넘지 않는다.
- 기본 드론 플레이어는 반환할 부품이 없으므로 Return 로그가 없어도 정상이다.

## Next

- Final D5 closeout: confirm EquippedParts return summary on PIE exit.
- D6 후보: 보스 더미 시각화/HP UI 또는 드론 사망 반환 검증 중 하나 선택.
- Booster/Drain/Vector, ContributionManager, DroneReport, DataTable, boss patterns, and VFX stay out of D5.
