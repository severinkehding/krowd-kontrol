# Issue #63: Add core Ability data defining the 5 CC abilities with locked base stats

Adds `AbilityData` — a single source of truth for the 5 crowd-control abilities'
(Stun, Sleep, Root, Fear, Snare) locked base stats from the GDD table: base duration,
range category, target type, and colour (sourced exclusively via
`ReservedGameplayColours::Get*()`, never a local `FLinearColor` literal, per MISSION.md
Hard Invariant 3). Data-only: no gameplay logic, no `UENUM`/`USTRUCT`/Blueprint
exposure, no enemy-counter field.

## Files changed (all under `app/`, gitignored per D-003 — this is the tracked-repo
record of that change)

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/AbilityData.h` | CREATE | `EAbilityRange`/`EAbilityTargetType` enums, `FAbilityData` aggregate struct, `AbilityData::Get`/`GetAll` declarations. |
| `app/Source/KrowdKontrol/AbilityData.cpp` | CREATE | The 5 locked GDD data rows (see table below) and the `Get`/`GetAll` accessor implementations. |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolAbilityDataTest.cpp` | CREATE | `KrowdKontrol.Unit.AbilityData` Automation Framework test. |

## Locked base stats

| Ability | Duration | Range  | Target Type | Colour | Colour-neutral |
|---------|----------|--------|-------------|--------|-----------------|
| Stun    | 3.0s     | Short  | Single      | White  | true            |
| Sleep   | 5.0s     | Long   | Single      | Blue   | false           |
| Root    | 5.0s     | Long   | Single      | Teal   | false           |
| Fear    | 5.0s     | Short  | Area        | Orange | false           |
| Snare   | 4.0s     | Medium | Cone        | Purple | false           |

## Acceptance criteria

- [x] **Exactly the 5 CC abilities are defined, each with locked base stats
      (duration, range, target type, colour).** `AbilityData::GetAll()` returns
      exactly 5 entries, one per `EAbilitySlot` value, verified in
      `KrowdKontrol.Unit.AbilityData`.
- [x] **Colours come from the reserved gameplay-colour palette, not local literals**
      (MISSION.md Hard Invariant 3). Every row sources its colour via
      `ReservedGameplayColours::Get*()`; verified by inspection of
      `AbilityData.cpp` and by the test's exact-colour assertions.
- [x] **Stun has no countered-enemy value set** (MISSION.md Hard Invariant 4).
      `bIsColourNeutral` is `true` only for Stun; no enemy-type/counter field exists
      anywhere in `FAbilityData` or the codebase.
- [x] **`KrowdKontrol.Unit.AbilityData` Automation Framework test exists** and covers:
      exactly-5-entries, each slot's `Get()` matching its requested slot and appearing
      exactly once in `GetAll()`, each ability's stats matching the GDD table exactly,
      Stun-only colour-neutrality, and mutual distinctness of all 5 colours.

## Validation

```
$ python harness/ci.py
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=9
APP_STARTED driver=cli
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
HOLDOUT_ABSENT no .factory/holdout/run.py
MUTATIONS_ABSENT no harness/mutations/run.py
GATE_OK mode=full
```

Raw Automation Framework report (`index.json`) double-checked directly rather than
trusting the harness's own summary line (a pre-existing, unrelated `succeeded` vs
listed-test-count mismatch in `run_ue_automation.sh` is a known harness reporting
quirk, not a regression): all 12 `KrowdKontrol.Unit.*` tests — including the new
`KrowdKontrol.Unit.AbilityData` — report `"state": "Success"`, `failed=0`, `notRun=0`.
Confirmed the tested DLL actually includes this change (DLL mtime postdates both
new source files' mtimes, after a real `Build.bat KrowdKontrolEditor` rebuild).

MISSION.md hard invariants reviewed by inspection: 5-colour lock (HI3) and
exactly-5-abilities/Stun-neutral/no-enemy-counter (HI4) both hold. No governance files
touched, no kill logic, no networking/dimensionality change, Unreal project remains
untracked in git.

---

Source lives under `app/` (gitignored, D-003) — this file is the tracked-repo record
of that change, not a substitute for reading the actual code.
