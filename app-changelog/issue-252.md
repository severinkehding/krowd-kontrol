# Issue #252: Reconcile AbilityData target-type enum with the locked ability shape table

`EAbilityTargetType` previously only had `Single`/`Area`/`Cone`, which could not
distinguish Root's line shot from Stun/Sleep's thrown circle, or Fear's
self-centered circle from a generic "area." This is a pure data-model change:
expanded the enum to 4 distinct values matching the PRD's locked shape table
(`docs/prd-ability-shapes.md`) and pointed each of the 5 `AbilityData` rows at
its correct value. No shape-casting, indicator, or effect logic — that is
explicitly out of scope for this issue and reserved for 5 downstream
per-ability issues.

## Files changed (all under `app/`, gitignored per D-003 — this is the tracked-repo
record of that change)

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/AbilityData.h` | UPDATE | `EAbilityTargetType` expanded from `{Single, Area, Cone}` to `{SelfCircle, Cone, Line, ThrownCircle}`; `FAbilityData::TargetType`'s default member initializer updated to `SelfCircle`. |
| `app/Source/KrowdKontrol/AbilityData.cpp` | UPDATE | The 5 `GetXxx()` factory functions' `TargetType` argument updated per the locked table (see below). |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolAbilityDataTest.cpp` | UPDATE | The 5 `TargetType` `TestEqual` assertions updated to the new expected values and description strings. |

## TargetType mapping

| Ability | Old TargetType | New TargetType |
|---------|-----------------|------------------|
| Stun    | Single          | ThrownCircle     |
| Sleep   | Single          | ThrownCircle     |
| Root    | Single          | Line             |
| Fear    | Area            | SelfCircle       |
| Snare   | Cone            | Cone (unchanged) |

Stun and Sleep both resolve to `ThrownCircle`; they remain distinguishable via
their existing `EAbilityRange` tier (Stun=Short, Sleep=Long) as the PRD specifies.

## Acceptance criteria

- [x] **`EAbilityTargetType` has 4 distinct values matching the locked table**:
      a self-centered AoE circle type (Fear), a cone type (Snare), a line type
      (Root), and a thrown-AoE-circle type (Stun and Sleep, distinguished from
      each other by their existing `EAbilityRange` tier).
- [x] **Each of the 5 `AbilityData` rows references its correct target type**
      per the table above.
- [x] **No shape-casting or effect-flavour logic implemented** — this issue is
      data-model only, verified by inspection of the 3 changed files.
- [x] **Existing code that reads the current target-type values compiles
      against the new/expanded enum** — only the test file reads `TargetType`
      outside `AbilityData` itself (confirmed by repo-wide grep), and it was
      updated in this same change.
- [x] **A unit/automation test asserts each of the 5 `AbilityData` rows
      resolves to its correct locked target type** — the 5 updated `TargetType`
      assertions in `KrowdKontrolAbilityDataTest.cpp`.
- [x] **Level 1-3 validation commands pass with exit 0 / `GATE_OK`** — see
      Validation below.
- [x] `app-source-tracked/` mirror and `app-changelog/issue-252.md` both exist
      and accurately reflect the real `app/` change.

## Validation

```
$ python harness/ci.py --quick
HARNESS_START mode=quick driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=88
GATE_OK mode=quick

$ python harness/ci.py
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=88
APP_STARTED driver=cli
UE_BUILD_START KrowdKontrolEditor Win64 Development
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
HOLDOUT_ABSENT no .factory/holdout/run.py
MUTATIONS_ABSENT no harness/mutations/run.py
GATE_OK mode=full
```

An earlier `python harness/ci.py` attempt reported `UE_AUTOMATION_FAILED
KrowdKontrol.Unit.EnemyBase: state=Fail` ("Enemy actor should not be destroyed
by reaching Banked") — unrelated to this change (no diff between `app/` and the
committed `app-source-tracked/` mirror for `EnemyBase.cpp`/`.h` at the time, and
the failure has nothing to do with `AbilityData` or `TargetType`). A re-run
passed cleanly with `GATE_OK mode=full`, confirming it was a flake in that run,
not a regression introduced here.

Raw Automation Framework log double-checked directly: `KrowdKontrol.Unit.AbilityData`
reports `Result={Success}` with all 5 updated `TargetType` values, as part of a
full 88/88 `KrowdKontrol.Unit.*` pass in the same run.

MISSION.md hard invariants reviewed by inspection: 5-colour lock (HI3) and
exactly-5-abilities/Stun-neutral/no-enemy-counter (HI4) both untouched — this
change only modifies `TargetType`. No governance files touched, no kill logic,
no networking/dimensionality change, Unreal project remains untracked in git.

---

Source lives under `app/` (gitignored, D-003) — this file is the tracked-repo record
of that change, not a substitute for reading the actual code.
