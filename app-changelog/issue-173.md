# Issue #173: Boss-checkpoint re-entry on level restart

`AKrowdKontrolPlayerController::RequestLevelRestart()` (issue #172) always reloads the
current map with no memory of how far the player got, so a level with a boss encounter
sent every failed attempt back to the level's own start - contradicting PRD REQ-4's
"re-enterable without a full level restart" requirement for boss fights.

This adds a one-shot "boss reached" checkpoint. `ULevelLifecycleSubsystem` gains
`bHasReachedBossCheckpoint` / `HasReachedBossCheckpoint()`, latched by a new
`RefreshBossCheckpointState()` (called from `Tick()`, mirroring the existing
`RefreshLevelClearState()`) the first time any `ABossBase` in the world leaves
`EBossState::Idle` - the existing, already-public signal for "this boss's encounter has
begun" (every boss subclass calls `AdvanceToArmed()` unconditionally from its own
`BeginPlay()`; no new marker was added to `ABossBase`). `RequestLevelRestart()` reads
that flag from the dying world's subsystem via a new `ComputeRestartOptions()` (mirrors
`ComputeRestartLevelName()`'s extraction) and passes it forward through
`UGameplayStatics::OpenLevel()`'s `Options` string - the engine's standard mechanism for
carrying state across a level transition, since `UTickableWorldSubsystem` state does not
itself survive `OpenLevel`. In the reloaded world, a new
`ApplyBossCheckpointIfRequested(APawn*)`, called from both `BeginPlay()`'s
already-possessed branch and `OnPossess()`, checks
`GetWorld()->URL.HasOption(TEXT("BossCheckpoint"))` and teleports the pawn to the first
`ABossBase` actor's placed location.

## Files changed

| File | Action | Contains |
|------|--------|----------|
| `LevelLifecycleSubsystem.h` | UPDATE | New `bHasReachedBossCheckpoint` field, `HasReachedBossCheckpoint()` accessor (`BlueprintPure`), `RefreshBossCheckpointState()` declaration |
| `LevelLifecycleSubsystem.cpp` | UPDATE | `#include "BossBase.h"`; `RefreshBossCheckpointState()` implementation; call added to `Tick()` |
| `KrowdKontrolPlayerController.h` | UPDATE | New `ComputeRestartOptions()` and `ApplyBossCheckpointIfRequested()` private method declarations, `FKrowdKontrolBossCheckpointRestartTest` friend declaration |
| `KrowdKontrolPlayerController.cpp` | UPDATE | `#include "LevelLifecycleSubsystem.h"`, `#include "BossBase.h"`; `ComputeRestartOptions()` and `ApplyBossCheckpointIfRequested()` implementations; `RequestLevelRestart()`'s `OpenLevel` call now passes `ComputeRestartOptions()`; both calls wired into `BeginPlay()`/`OnPossess()` |
| `PlayerEnergyComponent.h` | UPDATE (plan gap) | Appended `FKrowdKontrolBossCheckpointRestartTest` friend-class line, so the new test can seed `CurrentEnergy` deterministically the same way `KrowdKontrolLevelRestartTest.cpp` does |
| `Private/Tests/BossBaseTestActor.h` / `.cpp` | UPDATE (review follow-up) | Added an `ABossBaseTestActor()` constructor that creates and sets a `USceneComponent` as `RootComponent` - `ABossBase` itself has none (real placed bosses are Blueprint subclasses whose mesh supplies one), so a bare `SpawnActor<ABossBaseTestActor>()` previously always reported `GetActorLocation()`/`SetActorLocation()` as a no-op at the origin. Needed so `KrowdKontrolBossCheckpointRestartTest.cpp` can give a spawned boss a real, distinct test location to teleport to |
| `Private/Tests/KrowdKontrolBossCheckpointRestartTest.cpp` | CREATE, then UPDATE (review follow-up) | `KrowdKontrol.Unit.BossCheckpointRestart` - Case A/B assert `ComputeRestartOptions()` is empty with no boss checkpoint latched, and `"BossCheckpoint"` once `RefreshBossCheckpointState()` has actually latched it from a real `ABossBaseTestActor` state transition (`AdvanceToArmed()`, not friend-forced), in both cases confirming `bRestartRequested` still flips true via a real `OnLevelFailed` fire; Case B also proves the returned options string round-trips through `FURL::HasOption()` the same way the reader checks it; new Case C spawns a boss and a pawn, and asserts `ApplyBossCheckpointIfRequested()` is a no-op with the `BossCheckpoint` URL option absent and teleports the pawn to the boss's location when it's present |

**Note on the `LevelLifecycleSubsystem.{h,cpp}` diff size**: the plan's own Mandatory
Reading section flagged that `app-source-tracked/`'s copies of these two files were
stale relative to `app/`'s live versions, missing issue #170's
`EnsureLevelClearTimeSubscription()`/`SubscribeToLevelLifecycle` wiring (confirmed via
`diff` before this PR's changes were applied). Regenerating the mirror for this PR picks
up both #170's pre-existing code and this issue's new code in the same diff - the #170
lines are **not** new work from this PR, just a stale mirror catching up.

## Acceptance criteria

- [x] A "boss reached" checkpoint flag exists on `ULevelLifecycleSubsystem`
      (`bHasReachedBossCheckpoint` / `HasReachedBossCheckpoint()`)
- [x] The flag is set when a boss encounter begins - `RefreshBossCheckpointState()`
      polling `ABossBase::GetBossState() != Idle`
- [x] On restart, if the flag was set for the current level, the player is placed back
      at the boss encounter's start rather than the level's beginning -
      `ComputeRestartOptions()` + `OpenLevel` `Options` + `ApplyBossCheckpointIfRequested()`.
      The branching logic (`ComputeRestartOptions()`), the detection logic
      (`RefreshBossCheckpointState()`), the options-string round-trip, and the teleport
      itself (`ApplyBossCheckpointIfRequested()`) are all automated-test-covered; only the
      actual `OpenLevel()` map travel requires a real reload, which an in-process
      Automation World cannot execute (see "Manual PIE verification" below).
- [x] An automation test (`KrowdKontrol.Unit.BossCheckpointRestart`) latches the
      checkpoint from a real boss state transition, triggers a restart, asserts the
      restart logic branches to boss re-entry rather than level-start, and asserts the
      resulting teleport lands the pawn on the boss's location
- [x] `python harness/ci.py --quick` passes: `GATE_OK mode=quick`
- [x] No regressions in existing tests (`KrowdKontrol.Unit.LevelLifecycleSubsystem`,
      `KrowdKontrol.Unit.LevelRestart`, `KrowdKontrol.Unit.BossBase` all still pass)
- [x] This changelog documents the manual-PIE-verification gap and the
      `app-source-tracked/` mirror catch-up for issue #170's pre-existing wiring

## Manual PIE verification

Only the actual `UGameplayStatics::OpenLevel()` map reload itself is not automatable
in-process, same limitation `KrowdKontrolLevelRestartTest.cpp` already documents for
issue #172's own reload: a `CreateNewMap()` Automation World hangs on a real `OpenLevel()`
call. Everything downstream of that reload - `ApplyBossCheckpointIfRequested()`'s teleport
to the boss's location, and its no-op when the `BossCheckpoint` option is absent - is now
exercised directly by `KrowdKontrol.Unit.BossCheckpointRestart`'s Case C, since that
function only needs an already-loaded `UWorld*` and an already-possessed `APawn*`, neither
of which requires a real reload to construct.

Verification steps for a live PIE session (to be run by whoever validates this PR in the
Editor): enter a level containing a boss actor, let the boss's `BeginPlay()` run (arming
it), then fail the run (zero energy via `Cheat_ZeroPlayerEnergy` or real contact damage)
and confirm (1) the level reloads, and (2) the newly-possessed pawn spawns at the boss
actor's placed location rather than the level's normal start. Also confirm a *non-boss*
level's restart is unaffected (reloads to the normal level start, since the checkpoint
flag never latches without an `ABossBase` present).

**Status: not yet run.** No party in this PR's own pipeline (implement, review) has
editor/PIE access to execute this checklist - per repo memory, a holdout reviewer likely
can't either (no reflected gameplay-component state, no PIE camera/transform tooling for
this specific check). Whoever merges this PR must run the steps above in the Editor
first, then edit this section to record **who** ran it and **when**.

## Validation evidence

Editor build (`KrowdKontrolEditor Win64 Development`): `Result: Succeeded`.

`harness/run_ue_automation.sh KrowdKontrol.Unit.BossCheckpointRestart` →
`UE_AUTOMATION_RESULT passed=1 total=1`, `UE_AUTOMATION_OK`.

Targeted regression checks: `harness/run_ue_automation.sh
KrowdKontrol.Unit.LevelLifecycleSubsystem` → `passed=1 total=1`;
`harness/run_ue_automation.sh KrowdKontrol.Unit.LevelRestart` → `passed=1 total=1`;
`harness/run_ue_automation.sh KrowdKontrol.Unit.BossBase` → `passed=1 total=1`. All
`UE_AUTOMATION_OK`, no regressions.

`python harness/ci.py --mode full` (post review self-fix pass) →
`UNIT_PASSED tests=78`, `UE_BUILD_OK`, `UE_AUTOMATION_RESULT passed=1 total=1`,
`E2E_PASSED steps=1`, `GATE_OK mode=full`. All 78 unit tests pass with the expanded
`KrowdKontrol.Unit.BossCheckpointRestart` (test count unchanged - the new coverage was
added as additional cases within the existing test, not new test entries).

## Deviations from plan

- The plan's "Files to Change" table did not list `PlayerEnergyComponent.h`, but the new
  test needs to seed `CurrentEnergy` deterministically the same way
  `KrowdKontrolLevelRestartTest.cpp` does, which requires friend access. Added a
  `FKrowdKontrolBossCheckpointRestartTest` friend-class line, mirroring the existing
  pattern exactly (no reset method or public mutator added - the class's own
  "ApplyContactDamage is the only permitted mutator" invariant is unchanged). Same gap
  issue #172's own changelog records for its own test.
- The plan's `ApplyBossCheckpointIfRequested()` code sample used
  `TActorIterator<ABossBase> It(*World)` (dereferencing the `UWorld*`). This does not
  compile - `TActorIterator`'s constructor takes `const UWorld*`, not a `UWorld&` - and
  every other `TActorIterator` use in this codebase (including this same plan's own
  `RefreshBossCheckpointState()` sample) passes the pointer directly. Implemented as
  `TActorIterator<ABossBase> It(World)` instead; confirmed via a full Editor rebuild
  that this compiles clean.

### Review self-fix pass (2026-08-21)

Both review passes (code-review, test-coverage) independently flagged the same core gap:
Case B originally set `bHasReachedBossCheckpoint` via friend access instead of driving
`RefreshBossCheckpointState()`'s real `TActorIterator`/`GetBossState()` detection logic,
and `ApplyBossCheckpointIfRequested()`'s teleport had no coverage at all. Addressed by:

- Rewriting Case B to spawn a real `ABossBaseTestActor`, assert the latch stays false
  while it's still `Idle`, call `AdvanceToArmed()`, and assert `RefreshBossCheckpointState()`
  then latches it - no more friend-field shortcut, no more unused
  `friend class FKrowdKontrolBossCheckpointRestartTest;` in `LevelLifecycleSubsystem.h`
  (removed, since nothing in the test needs private access to that class anymore).
- Adding a writer/reader round-trip assertion in Case B, built via `FURL`'s default
  constructor + `AddOption()` rather than parsing a full `"Map?Options"` string - the
  text-parsing `FURL` constructor resolves the map segment against the asset registry and
  silently resets the whole URL (wiping `Op`) if that name isn't a real package, which a
  placeholder name here isn't; discovered by hitting exactly that reset while writing this
  fix.
- Adding Case C, which spawns a boss and a pawn and asserts `ApplyBossCheckpointIfRequested()`
  is a no-op with the option absent and teleports the pawn to the boss's location with it
  present. This surfaced that `ABossBase` (and therefore a bare `SpawnActor<ABossBaseTestActor>()`)
  has no `RootComponent`, so `GetActorLocation()`/`SetActorLocation()` were always
  no-ops at the origin for the test double - real placed bosses are Blueprint subclasses
  whose mesh supplies a root. Fixed by giving `ABossBaseTestActor` its own constructor
  that creates and sets a plain `USceneComponent` as `RootComponent` (test-only file,
  `ABossBase` itself is untouched).
