# Issue #357: Controlled-duration timers read as inconsistent across robots

**Type**: bug (legibility) | **Priority**: medium

## Summary

Operator playtest reported the Controlled-duration timer looking inconsistent
across robots. Investigation audited the three named likely-by-design
contributors (colour-match duration bonus from PR #303, per-enemy overrides
from issue #121, and #313/#336's separate attack-window timer) and found no
logic bug — every duration traces to an intentional per-enemy
`GetControlledDurationOverrideSeconds()` override. The real defect is
legibility: the existing indicator (issue #225) rendered every application
identically, with no visual channel telling the player a colour-matched
duration bonus apart from ordinary ability-to-ability spread. The fix adds a
reflected `bIsColourMatchBonused` flag and renders it as a visibly thicker bar
(40uu vs 24uu depth), reusing the exact boolean `EnemyBase::ReceiveControl`
already computes to pick the override duration — no new colour-matching logic,
no change to any duration value.

## Acceptance Criteria Mapping

| Criterion | Status |
|---|---|
| Audit the colour-match duration bonus (PR #303) for correctness | Done — verified correct against `TrooperEnemy.cpp`/`BomberEnemy.cpp`, matches PR #303's shipped ACs |
| Audit per-enemy overrides (issue #121 lineage) for correctness | Done — verified correct against `SniperEnemy.cpp`, matches issue #121's spec |
| Audit #313/#336 attack-window interaction with Controlled-duration | Done — ruled out; `TickAttackDuration`/`TickControlledDuration` are mutually exclusive by `EEnemyState`, no shared state |
| If audit finds no bug, make duration modifiers visually legible | Done — new `bIsColourMatchBonused` bool + bar-thickness signal (`ApplyVisualFillFraction()`), wired from `EnemyBase::ReceiveControl` |
| No change to actual duration values/balance | Done — `AbilityData::BaseDurationSeconds` and all `GetControlledDurationOverrideSeconds()` overrides left untouched |
| No new information colour (Hard Invariant #3) | Done — signal is geometry (bar depth) only; `CurrentColour` untouched, confirmed by existing test case (g4) plus new case (i) |
| Regression coverage | Done — new World-backed test case (i) asserts both the flag and the geometric bar-depth difference between a bonused and non-bonused application in the same World |

## Validation Evidence

`harness/ci.py --full` (from `validation.md`):
```
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=127
PIE_PASSED tests=8
APP_STARTED driver=cli
UE_BUILD_START KrowdKontrolEditor Win64 Development
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
GATE_OK mode=full
```

Hard Invariant #3 (5-colour lock) verified by inspection: the bonus signal is
rendered purely as bar thickness (geometry scale on the existing fill mesh) —
no new material, texture, or colour value anywhere in the diff.

`app/` cross-checked byte-identical against `app-source-tracked/` for every
changed file — no concurrent-task leakage into this PR's mirror.
