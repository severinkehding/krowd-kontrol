# Issue #261: Punishment-lockout remaining-time readout, distinct from not-yet-unlocked

`UAbilityCooldownTrayWidget` previously had exactly one locked-style visual (red border
+ `LCK` label, via `SetSlotLocked`), shared indistinguishably by two different producers:
`BindAbilityUnlockComponent` (not-yet-unlocked) and `BindAbilityLockoutComponent`
(punishment lockout, issue #178), which bound `UAbilityLockoutComponent::OnAbilityLockoutChanged`
directly onto `SetSlotLocked` because the delegate signature happened to match. Both
states set the same `SlotLocked` bool and rendered pixel-identically, with no way —
even programmatically — to tell which was true for a given slot.

This issue splits punishment lockout into its own tracked state (`SlotPunishmentLockoutActive`
/ `SlotPunishmentLockoutRemaining`, driven by a new adapter `HandleAbilityLockoutChanged`
instead of the old direct bind) with a live numeric remaining-time readout layered on
top of the existing locked visual, while leaving not-yet-unlocked's appearance
byte-for-byte unchanged. A new `EAbilityTileState` enum (`Ready` / `Cooldown` /
`PunishmentLockout` / `NotYetUnlocked`) and `GetSlotState()` accessor make all four tile
states provably distinguishable via one checkable value, with precedence
`PunishmentLockout` > `NotYetUnlocked` > `Cooldown` > `Ready`.

`UAbilityLockoutComponent` itself is unmodified (lockout truth, per the issue's Notes) —
this widget only reads it (`GetRemainingLockoutSeconds`). `KrowdKontrolPlayerController`'s
production wiring is unaffected — both `BindAbilityUnlockComponent`/`BindAbilityLockoutComponent`
signatures are unchanged.

## Files changed

The real Unreal project lives under `app/` (gitignored symlink, D-003) and is what the
harness actually builds/tests against — `app/` itself is unchanged by this PR's
tracking. What this PR's diff actually contains is a **copy** of the changed source,
per D-009, at `app-source-tracked/<same path under app/Source/>`.

| File (under `app-source-tracked/Source/KrowdKontrol/`) | Action | What it contains |
|------|--------|-------------------|
| `AbilityCooldownTrayWidget.h` | UPDATE | New `EAbilityTileState` enum; `GetSlotState()`/`GetSlotPunishmentLockoutRemainingSeconds()`/`RefreshPunishmentLockoutReadouts()` public API; `HandleAbilityLockoutChanged` private adapter; `SlotPunishmentLockoutActive`/`SlotPunishmentLockoutRemaining`/`BoundLockoutComponent` state |
| `AbilityCooldownTrayWidget.cpp` | UPDATE | `BuildWidgetTree()` seeds the two new arrays; `BindAbilityLockoutComponent()` now binds `HandleAbilityLockoutChanged` (not `SetSlotLocked`) and keeps a weak ref; new `HandleAbilityLockoutChanged`/`RefreshPunishmentLockoutReadouts`/`GetSlotState`/`GetSlotPunishmentLockoutRemainingSeconds`; `NativeTick` polls the new readout each frame; `UpdateSlotVisual` splits the old single `bLocked` branch into `bNotYetUnlocked`/`bPunishmentLockout` (same border/label for both, but only punishment lockout shows a numeric countdown) |
| `Private/Tests/KrowdKontrolAbilityCooldownTrayWidgetTest.cpp` | UPDATE | Block (k) rewritten to assert `GetSlotState()`/live numeric readout through activation, partial-advance, and expiry (previously asserted `IsSlotLocked()` only); block (l)'s null-guard assertions updated to `GetSlotState()`; new block (m) walks one slot through all 4 states and asserts `PunishmentLockout` takes precedence over a simultaneous `NotYetUnlocked` flag |
| `Private/Tests/KrowdKontrolHUDWiringTest.cpp` | UPDATE | Production-wiring assertion updated from `IsSlotLocked()` to `GetSlotState() == PunishmentLockout` — this is the intended consequence of the split (punishment lockout no longer sets `SlotLocked`), not a new capability |

No `.Build.cs` change — no new module dependencies. No new files.

## Acceptance criteria

- [x] Punishment lockout gets its own live numeric remaining-time readout, layered on
      top of the existing locked visual
- [x] Not-yet-unlocked's appearance is byte-for-byte unchanged — verified by block (m)'s
      explicit assertion of empty display text
- [x] All four tile states (`Ready`, `Cooldown`, `PunishmentLockout`, `NotYetUnlocked`)
      drive a distinct, checkable `EAbilityTileState` via `GetSlotState()`
- [x] Automation tests assert each state's distinct flag, including that
      `PunishmentLockout`/`NotYetUnlocked` (both locked-style) are distinguishable via
      readout presence, and that `PunishmentLockout` takes precedence under simultaneity
- [x] `UAbilityLockoutComponent` itself is unmodified
- [x] `KrowdKontrolPlayerController`'s existing wiring is unmodified (both bind calls'
      signatures unchanged)
- [x] `python harness/ci.py --quick` reports `GATE_OK` (`UNIT_PASSED tests=88`)

## Validation evidence

```
HARNESS_START mode=quick driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=88
GATE_OK mode=quick
```

Two unrelated transient failures were hit and ruled out during iteration, not caused by
this change:
- `KrowdKontrol.Unit.HUDWiring` failed once before its own assertion was updated (see
  above) — a real, intended consequence of the split, not a flake.
- `KrowdKontrol.Unit.LevelRestart` failed once with `LogModelContextProtocol: Error:
  Call to unknown method "server/discover"` in the log alongside it — a known
  environment flake (unrelated MCP server chatter failing whatever test is running at
  that moment); passed clean on rerun with no code change.

Full suite (`harness/run_ue_automation.sh KrowdKontrol.Unit.`) passed at 88/88 on the
final run, confirming no regression elsewhere (including
`KrowdKontrolAbilityLockoutComponentTest`, which this PR does not touch).

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and its
matching `app-source-tracked/` copy are the tracked-repo record of that change, per
D-009. Not a substitute for reading `app-source-tracked/` directly.
