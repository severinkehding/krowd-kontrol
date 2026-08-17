# Issue #23: Scale Overcrowd trigger threshold per level with current ability unlocks (REQ-1)

Makes `UOvercrowdDetectionComponent`'s 3 trigger thresholds (crowd size, radius,
uncontrolled duration) data-driven per level instead of fixed global constants,
per PRD 08 REQ-1: a 3-enemy convergence should not trigger Overcrowd identically
whether the player has only Stun (level 1) or all 5 abilities (level 4). Adds an
embedded `EditDefaultsOnly` `LevelThresholds` array (mirrors `FWaveEntry` /
`WaveSpawnerComponent`, the existing precedent for this pattern in the codebase)
plus `NotifyLevelReached(LevelIndex)`, mirroring
`UAbilityUnlockComponent::NotifyLevelReached`'s name/signature/warn-on-miss
behavior. No new subsystem or asset type — wiring a real level-progression
caller is future work, same as `UAbilityUnlockComponent::NotifyLevelReached`'s
own current status.

## Files changed (all under `app/`, gitignored per D-003 — this is the tracked-repo
record of that change, see the closing note below)

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/OvercrowdDetectionComponent.h` | UPDATE | New `FOvercrowdLevelThreshold` struct (`LevelIndex`/`CrowdThreshold`/`RadiusUnits`/`UncontrolledDurationSeconds`), `LevelThresholds` array property, `NotifyLevelReached(int32 LevelIndex)` declaration, and a third friend-class grant for the new test |
| `app/Source/KrowdKontrol/OvercrowdDetectionComponent.cpp` | UPDATE | `NotifyLevelReached` implementation: looks up the matching `LevelThresholds` entry, overwrites the 3 live threshold fields and resets `UncontrolledSeconds`; warns and no-ops on no match (non-empty array), silently no-ops when `LevelThresholds` is empty |
| `app/Source/KrowdKontrol/EnemyBase.h` | UPDATE | Adds a friend grant for the new test class so it can drive enemies via `TickCheckDetection`, same pattern as the two existing Overcrowd tests |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolOvercrowdLevelThresholdTest.cpp` | CREATE | `KrowdKontrol.Unit.OvercrowdLevelThreshold` — 5 scenarios covering all acceptance criteria below |

## Acceptance criteria

- [x] **Overcrowd trigger thresholds are read from a per-level source, not only a
      single global constant.** `LevelThresholds` (`TArray<FOvercrowdLevelThreshold>`)
      added alongside the existing 3 fields; `NotifyLevelReached` is the write path.
- [x] **At least two distinct levels can be configured with different threshold
      values, and `NotifyLevelReached` applies whichever level's values are active
      at runtime.** Test scenarios 1/2 configure a tight level-1 entry and a loose
      level-4 entry and confirm each is applied correctly.
- [x] **The same enemy-convergence scenario triggers Overcrowd under one level's
      configured threshold and does not trigger it under a different (looser)
      level's configured threshold.** Scenario 1 (3 enemies, `CrowdThreshold=3`)
      reaches `Active`; scenario 2 (same 3 enemies, `CrowdThreshold=8`) stays
      `Inactive`.
- [x] **No-match and empty-array cases are safe no-ops.** Scenario 3 (`LevelIndex`
      not present, non-empty array) warns and leaves fields unchanged; scenario 4
      (empty `LevelThresholds`) silently leaves fields unchanged — existing
      placements that never call `NotifyLevelReached` are unaffected.
- [x] **In-progress `UncontrolledSeconds` accumulation does not carry over across a
      level-threshold change.** Scenario 5 accumulates a partial duration, calls
      `NotifyLevelReached`, and confirms the reset accumulator does not trip the
      new threshold early.
- [x] **No regression in `KrowdKontrol.Unit.OvercrowdDetectionComponent` or any
      other existing test.** Full `KrowdKontrol.Unit.` sweep passed (45/45).

## Validation

```
$ python harness/ci.py --quick
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=45
APP_STARTED driver=cli
UE_BUILD_START KrowdKontrolEditor Win64 Development
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
HOLDOUT_ABSENT no .factory/holdout/run.py - NOTHING above the independence line ran.
MUTATIONS_ABSENT no harness/mutations/run.py - this gate has never been shown to fail.
GATE_OK mode=full
```

`harness/run_ue_automation.sh KrowdKontrol.Unit.OvercrowdLevelThreshold` (1/1) and
`harness/run_ue_automation.sh KrowdKontrol.Unit.OvercrowdDetectionComponent` (1/1,
no regression) also run and pass individually. One transient failure
(`KrowdKontrol.Unit.DoorConnectorActor`, unrelated to any file this change touches)
appeared on the first full-sweep run and passed on an immediate rerun — a
pre-existing test-isolation flake in suite run order, not caused by this change.

MISSION.md's Hard Invariants reviewed by inspection: this change does not touch
the no-kill rule, the 5-colour lock, the 5-ability roster, the 4-enemy roster,
engine/dimensionality, networking scope, or the `app/` tracking exception —
`NotifyLevelReached` never changes `CurrentState` (the one-directional
Inactive→Active invariant), only the 3 threshold fields and the accumulator.

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog
and its matching `app-source-tracked/` copy are the tracked-repo record of that
change, per D-009. Not a substitute for reading `app-source-tracked/` directly.
