# Issue #169: Add level lifecycle subsystem with level-begin and level-clear signals

Adds `ULevelLifecycleSubsystem` (a `UTickableWorldSubsystem`, mirroring
`UMusicSubsystem`'s existing shape), the first world-scoped source of truth for "the
level began" and "the level was cleared." This is REQ-1 of the "Run Lifecycle &
Progression Signals" PRD — foundational plumbing that every other run-lifecycle
requirement (clear-time wiring, fail/restart, Crowd Mastery, post-run summary,
run-complete) depends on.

## Acceptance criteria

- [x] `ULevelLifecycleSubsystem` (a `UWorldSubsystem`) exists in the `KrowdKontrol` module.
- [x] `OnLevelBegin` (dynamic multicast, carrying the map name as `FName`) fires at world
      begin-play on any world type the default `UWorldSubsystem` supports, prototype maps
      included — implemented in `OnWorldBeginPlay()`.
- [x] `OnLevelClear` (dynamic multicast) fires when every spawned `AEnemyBase` in the
      world is `Banked`, given at least one existed, and no `UWaveSpawnerComponent` has
      `IsWaveTimerActive() == true` — implemented in `RefreshLevelClearState()`.
- [x] Both delegates are public and `BlueprintAssignable`.
- [x] `KrowdKontrol.Unit.LevelLifecycleSubsystem` automation test asserts `OnLevelBegin`
      fires exactly once and `OnLevelClear` fires exactly once, after `OnLevelBegin`.
- [x] A test scenario proves `OnLevelClear` does not fire while a spawner's
      `IsWaveTimerActive()` is true, even with every currently-spawned enemy already
      `Banked` (and that a later wave-spawned enemy must itself reach `Banked` too).
- [x] `harness/run_ue_automation.sh KrowdKontrol.Unit.LevelLifecycleSubsystem` passes.
- [x] No regressions in the existing `KrowdKontrol.Unit.*` suite.

## Validation evidence

`python harness/ci.py` (full mode):

```
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=67
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

67 unit tests pass (66 pre-existing + `KrowdKontrol.Unit.LevelLifecycleSubsystem`), zero
regressions. Editor build clean.

## Notes

- Out of scope (deferred to follow-up issues per the PRD): wiring
  `ULevelClearTimeSubsystem` to these signals (REQ-2), map-type filtering, and
  level-fail/restart/Crowd-Mastery/summary/run-complete (REQ-3–7; REQ-3 already merged
  independently in #171).
- `app/Source/KrowdKontrol/EnemyBase.h` was updated with one additive friend-class grant
  (`FKrowdKontrolLevelLifecycleSubsystemTest`) only — the mirror below excludes two
  unrelated friend-class lines that had leaked into the live `app/` symlink from other
  concurrently in-flight factory tasks.
