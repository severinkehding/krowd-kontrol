# Issue #68: Give the ability tray a visually distinct Punishment-1 lockout state

Extends `UAbilityCooldownTrayWidget` (the ability/cooldown tray HUD widget) with a
second, distinct "locked" state per slot, per PRD 13 REQ-3: Punishment 1's future
ability-lock mechanic needs to read as visually distinct from an ordinary cooldown,
not just a longer one. The real Punishment 1 lockout system (PRD 08) doesn't exist
yet, so this issue wires up a placeholder boolean flag (`SetSlotLocked()`/
`IsSlotLocked()`) sufficient to demonstrate and test the distinct visual treatment;
`UAbilityCooldownComponent` itself stays untouched and decoupled from the future
mechanic.

## Files changed (all under `app/`, gitignored per D-003 — this is the tracked-repo
record of that change, see the closing note below)

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/AbilityCooldownTrayWidget.h` | UPDATE | `SetSlotLocked()`/`IsSlotLocked()` public API, private `SlotLocked` per-slot runtime state array |
| `app/Source/KrowdKontrol/AbilityCooldownTrayWidget.cpp` | UPDATE | Sizes `SlotLocked` in `BuildWidgetTree()`; adds the `"LCK"` label and `GetChromeBackgroundColor()`/`GetLockedBorderColor()` constants; implements `SetSlotLocked()`/`IsSlotLocked()`; extends `UpdateSlotVisual()` to swap label/border and suppress the cooldown countdown while locked |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolAbilityCooldownTrayWidgetTest.cpp` | UPDATE | Assertions for default-unlocked state, same-slot locked-vs-cooldown visual diff, locked colour reserved-safety, unlock reverting without clearing the underlying cooldown, and unbuilt-widget safety |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolReservedGameplayColoursTest.cpp` | UPDATE | Extends the centralized reserved-colour audit to also check the new locked-state border colour across all 5 slots |

## Acceptance criteria

- [x] **Ability tray widget supports a second, visually distinct "locked" state per
      slot.** `SetSlotLocked()`/`IsSlotLocked()` added; `UpdateSlotVisual()` branches
      on `SlotLocked[Index]`.
- [x] **Locked state driven by a minimal placeholder boolean flag per ability**
      (real Punishment 1 mechanic out of scope, tracked separately). `SlotLocked` is a
      private `TArray<bool>`, mutated only via `SetSlotLocked()` — no real lockout
      logic added.
- [x] **Locked vs. cooldown distinguishable via more than colour alone.** The label
      swaps from the ability abbreviation to `"LCK"` *and* the border tints dark red;
      the cooldown countdown is fully suppressed (collapsed, not just recoloured)
      while locked, so the state reads as structurally different, not a colour swap
      on the same display.
- [x] **Neither locked nor cooldown treatment uses a reserved gameplay colour
      (REQ-4).** New `GetLockedBorderColor()` is `FLinearColor(0.35, 0.05, 0.05, 0.92)`
      — a desaturated dark red, nowhere near the five reserved saturated hues
      (Purple/Teal/Orange/Blue/White). Verified both by a direct widget-test assertion
      and by extending the centralized `KrowdKontrolReservedGameplayColoursTest` audit.
- [x] **Automation Framework test confirms a different visual result for
      `locked=true` vs. a normal cooldown on the same slot.** New assertions start a
      cooldown on `EAbilitySlot::Root`, capture its border colour and cooldown display
      text, then lock the same slot and assert the label, border colour, and cooldown
      display all change — and that unlocking reverts them while leaving the
      underlying cooldown timer (`GetSlotRemainingSeconds()`) untouched.

## Validation

```
$ python harness/ci.py
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=33
APP_STARTED driver=cli
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
HOLDOUT_ABSENT no .factory/holdout/run.py - NOTHING above the independence line ran.
MUTATIONS_ABSENT no harness/mutations/run.py - this gate has never been shown to fail.
GATE_OK mode=full
```

Gate passed on the first run; no fixes were required. MISSION.md Hard Invariant #3
(the 5-colour gameplay-information lock) was additionally reviewed by inspection since
this diff touches tray chrome colours directly: the new locked-state colours are
near-black/desaturated dark red, not a 6th saturated information colour, and the
locked state is made distinguishable primarily via the `"LCK"` label swap with colour
as a secondary cue only. No other Hard Invariant is touched by this diff.

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
