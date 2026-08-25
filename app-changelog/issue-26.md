# Issue #26: Add per-punishment toggles to a debug/accessibility menu (REQ-3)

PRD `08-difficulty-punishment-and-balance.md` REQ-3 requires all three punishments
(ability-lockout, run-speed reduction, Overcrowd) to be individually toggleable in a
debug/accessibility menu, so Alpha playtesting sessions can isolate which punishment
is driving observed player behavior. All three underlying punishment systems already
existed (`UAbilityLockoutComponent`, `USpeedReductionPunishmentComponent`,
`UOvercrowdDetectionComponent`), each already gated on its own `kk.Punishment.*Enabled`
CVar per REQ-5's earlier "CVars are enough at this stage" deferral — this issue builds
the menu UI on top of that existing mechanism, not a replacement for it.

## Summary

New `UPunishmentDebugMenuWidget`: three independent `UCheckBox` toggles, one per
punishment. Each checkbox both flips its punishment's existing CVar and, only on
checked→unchecked, calls that punishment's instant-end method
(`EndAllLockouts()` / `EndSpeedReduction()` / new `ForceEndPanicOverload()`) so an
already-active effect ends immediately rather than merely being prevented from
re-triggering. The menu is hidden by default and toggled with F1 via
`AKrowdKontrolPlayerController::HandleToggleDebugMenu()`.

`UOvercrowdDetectionComponent` didn't yet have a `kk.Punishment.OvercrowdEnabled`
CVar or an instant-end method (Punishment 3 was newer than the other two), so both
were added: the CVar gates the `Inactive->Active` transition, and
`ForceEndPanicOverload()` ends an active Overcrowd effect on demand — mirroring the
existing pattern `UAbilityLockoutComponent`/`USpeedReductionPunishmentComponent`
already used.

## Acceptance criteria

- [x] Debug/accessibility menu exposes three independent on/off toggles, one per
      punishment (ability-lock, speed-reduction, Overcrowd).
- [x] Turning a toggle off prevents that punishment's trigger condition from
      producing effects, and ends an already-active effect from that punishment
      immediately.
- [x] Toggle state persists for the current play session (CVar-backed, session-only
      — no save-to-disk).
- [x] Automation Framework test confirms toggling a punishment off prevents its
      trigger condition from producing effects even when the trigger condition is
      met (`KrowdKontrolPunishmentDebugMenuWidgetTest.cpp`, plus new CVar-gate/
      `ForceEndPanicOverload()` scenarios in
      `KrowdKontrolOvercrowdDetectionComponentTest.cpp`).

## Files changed

| File | Action | What it contains |
|------|--------|-------------------|
| `OvercrowdDetectionComponent.h` / `.cpp` | UPDATE | New `kk.Punishment.OvercrowdEnabled` CVar gating `Inactive->Active`; new `ForceEndPanicOverload()`/`IsOvercrowdEnabledByCVar()`; widget-test friend grant |
| `PunishmentDebugMenuWidget.h` / `.cpp` | CREATE | New widget: 3 checkboxes bound to the three punishment components, each wired to its CVar + instant-end call |
| `KrowdKontrolPlayerController.h` / `.cpp` | UPDATE | Creates the widget, binds it to the possessed pawn's punishment components, binds F1 to `HandleToggleDebugMenu()` |
| `EnemyBase.h` | UPDATE | `friend class FKrowdKontrolPunishmentDebugMenuWidgetTest;` grant (mirrors the existing `FKrowdKontrolPunishmentArbitrationComponentTest` grant) so the new test can drive real Overcrowd detection through `AEnemyBaseTestActor::TickCheckDetection()` |
| `Private/Tests/KrowdKontrolOvercrowdDetectionComponentTest.cpp` | UPDATE | New CVar-gate + `ForceEndPanicOverload()` scenarios |
| `Private/Tests/KrowdKontrolPunishmentDebugMenuWidgetTest.cpp` | CREATE | New Automation test: visibility toggle, all 3 checkboxes wired to CVar + instant-end |

## Validation evidence

Ran the real validation ladder directly (Unreal C++ requires a real Editor build,
not just the quick static-analysis path):

- Real `UnrealBuildTool` compile: succeeded (0 errors)
- `harness/run_ue_automation.sh KrowdKontrol.Unit.PunishmentDebugMenuWidget` → `passed=1 total=1`
- `harness/run_ue_automation.sh KrowdKontrol.Unit.OvercrowdDetectionComponent` → `passed=1 total=1`
- `harness/run_ue_automation.sh KrowdKontrol.Unit.` (full unit rung, regression check) → `passed=110 total=110`
- `python harness/ci.py --quick` → `GATE_OK mode=quick` (`STATIC_SKIPPED`, `UNIT_PASSED tests=110`, `PIE_PASSED tests=5`)

Not covered by Automation Framework tests: a real PIE F1-keypress + visible checkbox
click — headless `-nullrhi` automation can't render/click real Slate. This remains a
manual-verification item.

## Notes

- Two commits landed on this branch: the widget/CVar implementation itself, and a
  follow-up fix dropping a friend-class grant (`FKrowdKontrolTeachingPromptComponentTest`)
  that leaked into the `app-source-tracked/EnemyBase.h` mirror from a concurrently-running,
  unrelated task (issue #219) sharing the same `app/` symlink — not part of this
  issue's scope, removed from the tracked mirror only.
- `app-source-tracked/` mirror and this changelog both written, so the PR has a real,
  openable diff.
