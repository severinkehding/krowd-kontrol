# Issue #173: Boss-checkpoint re-entry on level restart

**Re-implementation note:** a prior attempt (PR #208) implemented this exact feature
and passed code review, but was rejected at validation because its
`LevelLifecycleSubsystem.cpp`/`.h` diff also carried `EnsureLevelClearTimeSubscription()`
/ a call to `ULevelClearTimeSubsystem::SubscribeToLevelLifecycle()` — code belonging to
a different, still-open PR (#205, issue #170-v2) that leaked in via the shared `app/`
symlink. That method does not exist on `main`; merging PR #208 standalone would have
broken the build. This PR reconstructs the same reviewed implementation, subtracting
only that leaked coupling. See "Concurrent-task leakage check" below.

`AKrowdKontrolPlayerController::RequestLevelRestart()` (issue #172) always reloaded the
current map to its default start with no memory of progress. PRD REQ-4 requires boss
encounters to be "re-enterable without a full level restart." `ULevelLifecycleSubsystem`
(issue #169) gains a one-shot latch, `bHasReachedBossCheckpoint`, set by a new
`RefreshBossCheckpointState()` the first time a `TActorIterator<ABossBase>` scan finds
any boss whose `GetBossState() != EBossState::Idle` — the existing, already-public
"encounter began" signal, since every current boss subclass calls `AdvanceToArmed()`
unconditionally from its own `BeginPlay()`. No new signal was added to `ABossBase`.

Because `OpenLevel()` destroys the old `UWorld` (and therefore the old
`ULevelLifecycleSubsystem` instance) before the new one exists,
`RequestLevelRestart()` reads the flag off the about-to-die world's subsystem *before*
issuing the reload, via a new `ComputeRestartOptions()` (mirrors the existing
`ComputeRestartLevelName()` extraction), and passes it through `OpenLevel`'s `Options`
parameter. In the reloaded world, a new `ApplyBossCheckpointIfRequested(APawn*)` checks
`GetWorld()->URL.HasOption(TEXT("BossCheckpoint"))` and, if set, teleports the pawn to
the first `ABossBase` actor's placed location. Called from both `BeginPlay()`'s
already-possessed branch and `OnPossess()`, mirroring `WireWidgetsToPawn()`'s existing
dual-call-site shape.

## Files changed

| File | Action | Contains |
|------|--------|----------|
| `LevelLifecycleSubsystem.h` | UPDATE | `bHasReachedBossCheckpoint` field, `HasReachedBossCheckpoint()` accessor, `RefreshBossCheckpointState()` declaration |
| `LevelLifecycleSubsystem.cpp` | UPDATE | `#include "BossBase.h"`; `RefreshBossCheckpointState()` implementation; call added to `Tick()` |
| `KrowdKontrolPlayerController.h` | UPDATE | `ComputeRestartOptions()`, `ApplyBossCheckpointIfRequested()` declarations, `FKrowdKontrolBossCheckpointRestartTest` friend declaration |
| `KrowdKontrolPlayerController.cpp` | UPDATE | `#include "LevelLifecycleSubsystem.h"`, `#include "BossBase.h"`; both new methods implemented; wired into `OpenLevel`'s 4th arg, `BeginPlay()`, `OnPossess()` |
| `PlayerEnergyComponent.h` | UPDATE | Appended `FKrowdKontrolBossCheckpointRestartTest` friend-class line, for deterministic `CurrentEnergy` seeding in the new test |
| `Private/Tests/BossBaseTestActor.h`/`.cpp` | UPDATE | Added a constructor that creates/sets a `USceneComponent` `RootComponent` — `ABossBase` itself has none, so a bare `SpawnActor<ABossBaseTestActor>()` previously always reported `GetActorLocation()` as the world origin |
| `Private/Tests/KrowdKontrolBossCheckpointRestartTest.cpp` | CREATE | `KrowdKontrol.Unit.BossCheckpointRestart` — three cases (no-boss, real `AdvanceToArmed()`-driven latch + `FURL` round-trip, and `ApplyBossCheckpointIfRequested()` no-op-vs-teleport) |

## Acceptance criteria

- [x] `ULevelLifecycleSubsystem` exposes `bHasReachedBossCheckpoint` / `HasReachedBossCheckpoint()`
- [x] The flag latches when a boss encounter begins (`GetBossState() != EBossState::Idle`),
      via `RefreshBossCheckpointState()` called from `Tick()`
- [x] `ComputeRestartOptions()` + `OpenLevel`'s `Options` param + `ApplyBossCheckpointIfRequested()`
      route the restart to the boss encounter's location when the checkpoint was reached
- [x] `KrowdKontrol.Unit.BossCheckpointRestart` sets the checkpoint via a real boss state
      transition (not friend-forced), and asserts the restart logic branches correctly
- [x] No regressions: `KrowdKontrol.Unit.LevelLifecycleSubsystem`,
      `KrowdKontrol.Unit.LevelRestart`, `KrowdKontrol.Unit.BossBase` all still pass
- [x] `app-source-tracked/` mirror diff contains no `LevelClearTimeSubsystem`/
      `SubscribeToLevelLifecycle`/`EnsureLevelClearTimeSubscription` lines (verified by
      diff against the live `app/` copy — see below)
- [ ] Manual PIE verification of the real `OpenLevel()` reload. **Not yet run** — see
      "Manual PIE verification not run" below.

## Manual PIE verification not run

Not automatable in-process: a `CreateNewMap()` Automation World is not a game world, so
`RequestLevelRestart()`'s real `UGameplayStatics::OpenLevel()` call never executes
inside `KrowdKontrol.Unit.BossCheckpointRestart` (same limitation issue #172's own test
documents). The test instead drives every other piece of real logic directly:
`RefreshBossCheckpointState()`'s detection off a genuine `AdvanceToArmed()` transition,
`ComputeRestartOptions()`'s output round-tripped through a real `FURL::HasOption()`
call, and `ApplyBossCheckpointIfRequested()`'s teleport behavior called in isolation.

Verification steps for whoever merges this PR, to run live in PIE:
1. Enter a level containing a placed boss; let it transition out of `Idle` (its
   `BeginPlay()` calls `AdvanceToArmed()` unconditionally, so this happens immediately).
2. Fail the run (zero player energy, e.g. via the `Cheat_ZeroPlayerEnergy` exec command).
3. Confirm the level reloads and the player pawn spawns at the boss's placed location,
   not the level's default start.
4. Separately, confirm a level with **no** boss still restarts to its normal start
   (checkpoint flag never latches without a boss).

No party in this PR's pipeline (implement, review) has Editor/PIE access to run this.
Whoever merges must run the steps above and flip the corresponding acceptance-criteria
box from `[ ]` to `[x]`, recording who ran it and when.

## Concurrent-task leakage check

At implementation start, the live `app/` copy of `LevelLifecycleSubsystem.h`/`.cpp`
already contained **both** (a) the complete boss-checkpoint implementation from the
prior, rejected attempt (PR #208) — matching this plan's spec exactly, including the
guard-clause simplification review had already requested — and (b)
`EnsureLevelClearTimeSubscription()` / a call to
`ULevelClearTimeSubsystem::SubscribeToLevelLifecycle()`, which belongs to still-open PR
#205 (issue #170-v2, confirmed open via `gh pr view 205`) and does not exist on
`origin/main` (confirmed via `git show origin/main:...`).

Per the plan, (b) was left untouched in `app/` (removing it would revert someone else's
in-flight, unrelated work) but excluded entirely from this PR's `app-source-tracked/`
mirror. Diffing the mirror's `LevelLifecycleSubsystem.h`/`.cpp` against the live `app/`
copy confirms the only remaining delta is exactly that excluded taint (the
`EnsureLevelClearTimeSubscription()` declaration/definition, its two call sites, the
`#include "LevelClearTimeSubsystem.h"`/`#include "Engine/GameInstance.h"` includes, the
`bHasWarnedMissingLevelClearTimeSubsystem` field, and one doc-comment sentence
referencing `ULevelClearTimeSubsystem::HandleLevelClear()`) — nothing else. No other
changed file in this PR (`KrowdKontrolPlayerController.*`, `PlayerEnergyComponent.h`,
`BossBaseTestActor.*`, the new test) contained any leakage from PR #205 or any other
concurrent task.

## Validation evidence

`harness/ci.py --quick` → `GATE_OK mode=quick`, `UNIT_PASSED tests=78`.

Targeted runs:
- `harness/run_ue_automation.sh KrowdKontrol.Unit.BossCheckpointRestart` → `UE_AUTOMATION_RESULT passed=1 total=1`, `UE_AUTOMATION_OK`
- `harness/run_ue_automation.sh KrowdKontrol.Unit.LevelLifecycleSubsystem` → `passed=1 total=1`, `UE_AUTOMATION_OK`
- `harness/run_ue_automation.sh KrowdKontrol.Unit.LevelRestart` → `passed=1 total=1`, `UE_AUTOMATION_OK`
- `harness/run_ue_automation.sh KrowdKontrol.Unit.BossBase` → `passed=1 total=1`, `UE_AUTOMATION_OK`
- Full `harness/run_ue_automation.sh KrowdKontrol.Unit.` → `UE_AUTOMATION_RESULT passed=78 total=78`, `UE_AUTOMATION_OK` — no regressions.

Full `python harness/ci.py` (mode=full, including build + E2E) deferred to the separate
`dark-factory-validate` node per this factory's workflow split.

## Deviations from plan

None — the live `app/` copy already contained a complete implementation matching this
plan task-for-task (a holdover from the prior rejected attempt, PR #208, whose code was
already reviewed and approved on its merits). This PR's own work was verifying that
implementation against the plan line-by-line, confirming it builds and all targeted
tests pass, and constructing the `app-source-tracked/` mirror and this changelog with
the PR #205 taint explicitly excluded (see "Concurrent-task leakage check" above).

## Self-fix pass (review findings)

Applied after the consolidated review (code-review, error-handling, test-coverage
agents):

- **HIGH (test-coverage)**: `ApplyBossCheckpointIfRequested()` was only ever exercised
  via a direct call (Case C), never through its real `BeginPlay()`/`OnPossess()`
  production call sites. Added Cases D and E to
  `KrowdKontrolBossCheckpointRestartTest.cpp`, mirroring
  `KrowdKontrolHUDWiringTest.cpp`'s opposite-ordering two-case pattern — Case D drives
  the already-possessed `BeginPlay()` branch, Case E drives the `OnPossess()` branch —
  each asserting the pawn actually lands at the boss location through the production
  wiring, not a direct method call.
- **MEDIUM (error-handling)**: `ApplyBossCheckpointIfRequested()` silently no-op'd if
  the `BossCheckpoint` URL option was set but no `ABossBase` existed in the reloaded
  world. Added a `UE_LOG(LogTemp, Warning, ...)` in that fall-through, matching this
  same file's `ResolveLevelClearTimeSubsystem()` "unexpected missing dependency"
  convention.
- **LOW (code-review)**: documented, via a header-comment addition on
  `ApplyBossCheckpointIfRequested()`, the latent single-boss-per-level assumption
  between the latch's "any non-Idle boss" condition and the teleport's "first boss"
  selection (comment-only, non-blocking, per the reviewer's own recommendation — no
  logic change to already-approved #208 behavior).
- **LOW (test-coverage, skipped)**: multi-boss teleport-target selection stays
  untested — the source agent's own recommendation was to skip, since no multi-boss
  level is currently designed (`MISSION.md` lists one boss per encounter) and the
  added test surface isn't worth it for an unreached case today.

Re-validated after the fixes: `python harness/ci.py` (mode=full) →
`UNIT_PASSED tests=78`, `UE_BUILD_OK`, `UE_AUTOMATION_OK`, `E2E_PASSED steps=1`,
`GATE_OK mode=full`. Targeted `harness/run_ue_automation.sh
KrowdKontrol.Unit.BossCheckpointRestart` → `UE_AUTOMATION_RESULT passed=1 total=1`
(now covering Cases A–E).

## Fix pass 2 (pass-1 validation feedback)

Addressed:

- **LOW (code_quality)**: `ApplyBossCheckpointIfRequested()` was gated only on the
  persistent `BossCheckpoint` `FURL` option, so a future re-possession of the same
  controller within the same reloaded world (no call site does this today) would
  re-teleport the pawn. Added `bBossCheckpointApplied`, a one-shot guard mirroring
  this file's existing `bRestartRequested` never-reset-once-set idiom, checked first
  in `ApplyBossCheckpointIfRequested()` and set right after the URL-option check
  succeeds - covers both the teleport and the missing-boss-warning paths with one
  flag.
- **LOW (code_quality)**: the missing-boss warning log's double-fire risk on the same
  re-possession edge case is resolved by the same guard - it can now only run once
  per controller.

Not addressed - escalated, not fixable inside this issue's C++ diff:

- **MEDIUM (e2e)**: the E2E holdout could not independently observe the actual
  restart-teleport (only the arming precondition), because (1) no shipped level
  places a boss encounter today, and (2) no MCP exec/console tool or settable
  property exists to force the zero-energy fail condition in a live PIE session.
  Both gaps are outside this issue's scope - (1) is level-design/content-population
  work, (2) is harness/MCP tooling work - and mirror an already-accepted limitation
  from the dependency issue #172 (`CreateNewMap()` Automation Worlds can't exercise
  the real `UGameplayStatics::OpenLevel()` reload in-process either). Per pass-1's
  own `should_escalate: true`, left for human judgment rather than looped fix
  attempts against a gap this diff cannot close.

Re-validated: `python harness/ci.py --quick` → `UNIT_PASSED tests=78`, `GATE_OK
mode=quick`.
