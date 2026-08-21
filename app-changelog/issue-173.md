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
| `LevelLifecycleSubsystem.h` | UPDATE | New `bHasReachedBossCheckpoint` field, `HasReachedBossCheckpoint()` accessor (`BlueprintPure`), `RefreshBossCheckpointState()` declaration, `FKrowdKontrolBossCheckpointRestartTest` friend declaration |
| `LevelLifecycleSubsystem.cpp` | UPDATE | `#include "BossBase.h"`; `RefreshBossCheckpointState()` implementation; call added to `Tick()` |
| `KrowdKontrolPlayerController.h` | UPDATE | New `ComputeRestartOptions()` and `ApplyBossCheckpointIfRequested()` private method declarations, `FKrowdKontrolBossCheckpointRestartTest` friend declaration |
| `KrowdKontrolPlayerController.cpp` | UPDATE | `#include "LevelLifecycleSubsystem.h"`, `#include "BossBase.h"`; `ComputeRestartOptions()` and `ApplyBossCheckpointIfRequested()` implementations; `RequestLevelRestart()`'s `OpenLevel` call now passes `ComputeRestartOptions()`; both calls wired into `BeginPlay()`/`OnPossess()` |
| `PlayerEnergyComponent.h` | UPDATE (plan gap) | Appended `FKrowdKontrolBossCheckpointRestartTest` friend-class line, so the new test can seed `CurrentEnergy` deterministically the same way `KrowdKontrolLevelRestartTest.cpp` does |
| `Private/Tests/KrowdKontrolBossCheckpointRestartTest.cpp` | CREATE | `KrowdKontrol.Unit.BossCheckpointRestart` - asserts `ComputeRestartOptions()` is empty with no boss checkpoint latched, and `"BossCheckpoint"` once it is, in both cases confirming `bRestartRequested` still flips true via a real `OnLevelFailed` fire |

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
      The branching logic (`ComputeRestartOptions()`) is automated-test-covered; the
      actual reload + teleport requires a real `OpenLevel()` map travel, which an
      in-process Automation World cannot execute (see "Manual PIE verification" below).
- [x] An automation test (`KrowdKontrol.Unit.BossCheckpointRestart`) sets the checkpoint
      flag directly, triggers a restart, and asserts the restart logic branches to boss
      re-entry rather than level-start
- [x] `python harness/ci.py --quick` passes: `GATE_OK mode=quick`
- [x] No regressions in existing tests (`KrowdKontrol.Unit.LevelLifecycleSubsystem`,
      `KrowdKontrol.Unit.LevelRestart`, `KrowdKontrol.Unit.BossBase` all still pass)
- [x] This changelog documents the manual-PIE-verification gap and the
      `app-source-tracked/` mirror catch-up for issue #170's pre-existing wiring

## Manual PIE verification

Not automatable in-process, same limitation `KrowdKontrolLevelRestartTest.cpp` already
documents for issue #172's own reload: a `CreateNewMap()` Automation World hangs on a
real `UGameplayStatics::OpenLevel()` call, so the actual map reload and pawn teleport to
the boss's location must be checked live in PIE rather than by an Automation test.
`KrowdKontrol.Unit.BossCheckpointRestart` instead asserts the testable branching seam
(`ComputeRestartOptions()` returning `"BossCheckpoint"` once the flag is latched).

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

`python harness/ci.py --quick` → `STATIC_SKIPPED` (no static command configured),
`UNIT_PASSED tests=78`, `GATE_OK mode=quick`.

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
