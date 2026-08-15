# Issue #70: Reserved gameplay-colour guardrail

Adds `ReservedGameplayColours.h`/`.cpp`, a single C++ source of truth for MISSION.md's
five locked gameplay-information colours (Purple/Teal/Orange/Blue/White — `GetPurple()`
/ `GetTeal()` / `GetOrange()` / `GetBlue()` / `GetWhite()` / `GetAll()`), and a new
Automation Framework test that audits `UEnergyMeterWidget`'s and
`UAbilityCooldownTrayWidget`'s chrome colours against it. The audit finds no existing
violation — both widgets already use a desaturated near-black background nowhere near
any of the five reserved values — so this is a guardrail-and-test addition, not a
colour fix. Concrete RGB values are placeholders pending real enemy/ability art; every
consumer goes through `ReservedGameplayColours::Get*()`, so a future art-direction
ruling is a one-file change with zero call-site updates.

## Files changed (all under `app/`, gitignored per D-003 — this is the tracked-repo
record of that change, see the closing note below)

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/ReservedGameplayColours.h` | CREATE | `namespace ReservedGameplayColours` declaring `Get{Purple,Teal,Orange,Blue,White}()` and `GetAll()`, doc-commented back to MISSION.md Hard Invariant 3 / PRD 13 REQ-4 |
| `app/Source/KrowdKontrol/ReservedGameplayColours.cpp` | CREATE | Accessor implementations returning distinct, saturated `FLinearColor` placeholder values |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolReservedGameplayColoursTest.cpp` | CREATE | `KrowdKontrol.Unit.ReservedGameplayColours` — constant-list sanity (5 entries, mutually distinct) plus the chrome audit of both widgets (all 5 tray slots, not just index 0) |
| `app/Source/KrowdKontrol/EnergyMeterWidget.h` | UPDATE | Added `friend class FKrowdKontrolReservedGameplayColoursTest;` alongside the existing friend declaration, so the audit test can read `BackgroundBorder` without widening the widget's public API |
| `app/Source/KrowdKontrol/AbilityCooldownTrayWidget.h` | UPDATE | Same, for `SlotIconBorders` |

## Acceptance criteria

- [x] **A single, clearly-named constant list enumerates the 5 reserved colour
      values**, with a comment citing MISSION.md Hard Invariant 3.
      `ReservedGameplayColours::GetAll()` / `Get{Purple,Teal,Orange,Blue,White}()`.
- [x] **`UEnergyMeterWidget` and `UAbilityCooldownTrayWidget` audited for REQ-4
      compliance**; both already compliant, confirmed by the new automated test
      rather than by inspection alone (no code fix needed in either widget).
- [x] **A new Automation Framework test reads both widgets' known chrome colours and
      asserts none match the reserved list.** Covers all 5 tray slots.
- [x] **No regressions in `KrowdKontrol.Unit.EnergyMeterWidget` /
      `KrowdKontrol.Unit.AbilityCooldownTrayWidget`.** Both still pass after the added
      friend declarations.

## Validation

```
$ python harness/ci.py
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=8
APP_STARTED driver=cli
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
HOLDOUT_ABSENT no .factory/holdout/run.py - NOTHING above the independence line ran.
MUTATIONS_ABSENT no harness/mutations/run.py - this gate has never been shown to fail.
GATE_OK mode=full
```

A direct `Automation RunTests KrowdKontrol.Unit.` pass (via `UnrealEditor-Cmd.exe`,
outside the harness's `unit_count_pattern` regex which undercounts) confirms all 11
`KrowdKontrol.Unit.*` tests pass, including the new
`KrowdKontrol.Unit.ReservedGameplayColours` and the two audited widgets' pre-existing
tests — no regression from the added friend declarations.

---

Source lives under `app/` (gitignored, D-003) — this file is the tracked-repo record
of that change, not a substitute for reading the actual code.
