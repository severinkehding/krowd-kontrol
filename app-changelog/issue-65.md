# Issue #65: Colour-match duration bonus for Root (TR-UPR) and Fear (B0-0MR)

Extends `AEnemyBase::GetControlledDurationOverrideSeconds()`'s already-shipped
per-enemy-subclass override pattern (issue #121's Sleep-vs-SN-1PR, `ASniperEnemy`) to
the two remaining duration-bearing colour matchups: `ATrooperEnemy` (TR-UPR) now
returns 8.0f for Root, and `ABomberEnemy` (B0-0MR) now returns 7.0f for Fear, both
falling through to `Super::` (base 5.0f duration) for every other ability. This closes
the gap the operator's 2026-08-22 ruling identified: colour-matching (REQ-2) had zero
mechanical payoff for Root/Fear, only Sleep already rewarded it. Snare->RU-NNR's
75%-slow potency bonus is explicitly out of scope (no `ControlledSpeedMultiplier`
override hook exists yet; deferred pending the Ability Targeting Shapes PRD's
slow-flavour work).

## Files changed

| File | Action |
|------|--------|
| `TrooperEnemy.h` / `TrooperEnemy.cpp` | Added `GetControlledDurationOverrideSeconds` override (Root -> 8.0f) |
| `BomberEnemy.h` / `BomberEnemy.cpp` | Added `GetControlledDurationOverrideSeconds` override (Fear -> 7.0f) |
| `EnemyBase.h` | Added `friend class FKrowdKontrolAbilityColourMatchTest;` |
| `Private/Tests/KrowdKontrolAbilityColourMatchTest.cpp` | CREATE - `KrowdKontrol.Unit.AbilityColourMatch`, covering matched (Root/TR-UPR, Fear/B0-0MR), mismatched (Fear/TR-UPR, never gated/reduced), and Stun-vs-both-types (no bonus) |
| `Private/Tests/KrowdKontrolTrooperEnemyTest.cpp` | UPDATE - case (r)'s expiry-reversion assertion now reads the actually-applied duration (`GetTotalControlledSeconds()`, 8.0f) instead of the now-stale `AbilityData::Get(Root).BaseDurationSeconds` (5.0f) assumption |
| `Private/Tests/KrowdKontrolBomberEnemyTest.cpp` | UPDATE - case (t), same fix for Fear (7.0f) |

## Acceptance criteria

- [x] Applying Root to `ATrooperEnemy` (TR-UPR) grants the 8s bonus (Root's base is 5s)
- [x] Applying Fear to `ABomberEnemy` (B0-0MR) grants the 7s bonus (Fear's base is 5s)
- [x] Applying any non-countering ability to `ATrooperEnemy`/`ABomberEnemy` still
      applies at full base effectiveness - never blocked, never reduced
- [x] Stun never grants a bonus against TR-UPR or B0-0MR
- [x] `KrowdKontrol.Unit.AbilityColourMatch` exists and passes, covering one matched
      application, one mismatched application, and Stun-vs-any-enemy
- [x] `KrowdKontrol.Unit.TrooperEnemy`, `KrowdKontrol.Unit.BomberEnemy`,
      `KrowdKontrol.Unit.SniperEnemy`, `KrowdKontrol.Unit.EnemyBase` all still pass
      (two pre-existing expiry assertions in Trooper/Bomber needed updating - see
      Deviations below - not a behavioral regression)
- [x] Sleep->SN-1PR's existing 7s bonus (issue #121) is untouched
- [x] Snare->RU-NNR's 75%-slow potency bonus is explicitly NOT implemented here (out
      of scope per the 2026-08-22 operator ruling)
- [x] `app/` and `app-source-tracked/` copies of all changed/new files are identical
      (verified via `diff`)

## Deviation from plan

The plan's Task 1/2 VALIDATE steps assumed `KrowdKontrol.Unit.TrooperEnemy` and
`KrowdKontrol.Unit.BomberEnemy` would pass unaffected ("this test doesn't yet assert
duration"). That assumption was incorrect: both files already had an expiry-reversion
case (Trooper case (r), Bomber case (t)) that explicitly asserted "no per-enemy
override exists ... so the base duration governs" and hardcoded the 5.0f base duration
as the expiry point. Adding the new overrides made that premise false - both enemies
now expire at 8.0f/7.0f instead of 5.0f, so `python harness/ci.py --quick` failed on
first run with `UE_AUTOMATION_FAILED` for both suites (`GATE_FAILED: unit`). Fixed by
updating both cases to read the actually-applied duration off
`GetTotalControlledSeconds()` (with a precondition `TestEqual` against the expected
override value) instead of hardcoding the stale base-duration assumption, mirroring
how `KrowdKontrolSniperEnemyTest.cpp`'s own pre-existing Sleep expiry case already
does it. Not a scope change - both tests' actual coverage goal (does the enemy revert
to Alert once its controlled duration elapses) is unchanged, only the hardcoded
duration value they exercised was updated to match the now-correct override contract.

## Validation evidence

Full gate (`python harness/ci.py`, mode=full):

```
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=106
PIE_PASSED tests=5
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

`UNIT_PASSED tests=106` includes the new `KrowdKontrol.Unit.AbilityColourMatch` suite
(prior baseline before this change: 105 - one new suite added, no others removed).
Targeted first run (before the Deviation fix above) showed `UE_AUTOMATION_FAILED` for
`KrowdKontrol.Unit.BomberEnemy` and `KrowdKontrol.Unit.TrooperEnemy`; re-run after the
fix shows all suites, including those two, passing.

MISSION.md Hard Invariants reviewed against this diff: no kill-rule, colour-lock,
ability-roster, enemy-roster, engine/dimensionality, networking, or `app`-tracking
invariant is touched - this is a same-shape extension of an already-shipped,
already-invariant-compliant per-enemy override mechanism (issue #121).

---

The real Unreal project stays under `app/` (gitignored, D-003) - this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
