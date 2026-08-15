# Issue #70: Reserved gameplay-colour guardrail

Adds `ReservedGameplayColours.h`/`.cpp`, a single C++ source of truth for MISSION.md's
five locked gameplay-information colours (Purple/Teal/Orange/Blue/White — `GetPurple()`
/ `GetTeal()` / `GetOrange()` / `GetBlue()` / `GetWhite()` / `GetAll()`), and a new
Automation Framework test that audits `UAbilityCooldownTrayWidget`'s chrome colours
(both slot icon borders and slot cooldown text) against it. The audit finds no existing
violation — the tray already uses a desaturated near-black background and light-gray
text nowhere near any of the five reserved values — so this is a guardrail-and-test
addition, not a colour fix. Concrete RGB values are placeholders pending real
enemy/ability art; every consumer goes through `ReservedGameplayColours::Get*()`, so a
future art-direction ruling is a one-file change with zero call-site updates.

`UEnergyMeterWidget` (issue #64 / PR #92, not yet merged) is not audited here — an
earlier draft of this PR accidentally picked up that file from the shared `app/`
working copy per `CLAUDE.md` D-003's known concurrency risk; it's been dropped from
this PR's scope entirely. The energy-meter half of this audit is a small follow-up
once PR #92 merges and `EnergyMeterWidget.h`/`.cpp` legitimately exist on `main`.

## Files changed (all under `app/`, gitignored per D-003 — this is the tracked-repo
record of that change, see the closing note below)

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/ReservedGameplayColours.h` | CREATE | `namespace ReservedGameplayColours` declaring `Get{Purple,Teal,Orange,Blue,White}()` and `GetAll()`, doc-commented back to MISSION.md Hard Invariant 3 / PRD 13 REQ-4 |
| `app/Source/KrowdKontrol/ReservedGameplayColours.cpp` | CREATE | Accessor implementations returning distinct, saturated `FLinearColor` placeholder values |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolReservedGameplayColoursTest.cpp` | CREATE | `KrowdKontrol.Unit.ReservedGameplayColours` — constant-list sanity (5 entries, mutually distinct), a negative-case proving the audit assertion itself catches a real collision, plus the chrome audit of the tray widget (all 5 slots' borders and text, not just slot 0's border) |
| `app/Source/KrowdKontrol/AbilityCooldownTrayWidget.h` | UPDATE | Added `friend class FKrowdKontrolReservedGameplayColoursTest;` alongside the existing friend declaration, so the audit test can read `SlotIconBorders`/`SlotCooldownTexts` without widening the widget's public API |

## Acceptance criteria

- [x] **A single, clearly-named constant list enumerates the 5 reserved colour
      values**, with a comment citing MISSION.md Hard Invariant 3.
      `ReservedGameplayColours::GetAll()` / `Get{Purple,Teal,Orange,Blue,White}()`.
- [x] **`UAbilityCooldownTrayWidget` audited for REQ-4 compliance**; already
      compliant, confirmed by the new automated test rather than by inspection alone
      (no code fix needed). `UEnergyMeterWidget`'s audit is deferred to a follow-up —
      see note above.
- [x] **A new Automation Framework test reads the widget's known chrome colours and
      asserts none match the reserved list.** Covers all 5 tray slots' borders and
      text.
- [x] **No regressions in `KrowdKontrol.Unit.AbilityCooldownTrayWidget`.** Still
      passes after the added friend declaration.

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

A direct `Automation RunTests KrowdKontrol.Unit.` pass (via `UnrealEditor-Cmd.exe`)
confirms all 8 `KrowdKontrol.Unit.*` tests pass (`UE_AUTOMATION_RESULT passed=8
total=8`), including the new `KrowdKontrol.Unit.ReservedGameplayColours` (with its
text-colour audit and negative-case addition) and
`KrowdKontrol.Unit.AbilityCooldownTrayWidget`'s pre-existing test — no regression from
the added friend declaration.

---

Source lives under `app/` (gitignored, D-003) — this file is the tracked-repo record
of that change, not a substitute for reading the actual code.
