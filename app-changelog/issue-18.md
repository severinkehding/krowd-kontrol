# Issue #18: Overcrowd recovery — landing CC on a converged enemy ends Panic Overload

Adds the recovery half of PRD 08 REQ-2 to `UOvercrowdDetectionComponent` (issue #16):
once Panic Overload is `Active`, landing any of the 5 CC abilities (Stun/Sleep/Root/
Fear/Snare, via the existing `AEnemyBase::ReceiveControl()`) on an enemy that was part
of the convergence that triggered it now immediately flips the state back to
`Inactive`, resets the arming timer, and re-broadcasts `OnPanicOverloadStateChanged`.
No new ability, input, or UI is introduced — recovery rides the same
`ReceiveControl()`/`GetEnemyState()` surface every other CC interaction already uses.

**REQ-2 reading used (per the issue's own instruction to state this explicitly):**
convergence *membership* is tracked, not "any CC hit anywhere." A new `ConvergedEnemies`
snapshot is captured at the exact instant `CurrentState` flips `Inactive -> Active`;
only CC landed on a surviving member of that snapshot ends Panic Overload. CC landed on
an enemy that joins the crowd afterward does not (Scenario 7 below).

## Files changed (all under `app/`, gitignored per D-003 — this is the tracked-repo
record of that change, see the closing note below)

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/OvercrowdDetectionComponent.h` | UPDATE | New `ConvergedEnemies` (`TArray<TWeakObjectPtr<AEnemyBase>>`) member and private `HasConvergedEnemyBeenControlled()` declaration; `CountHotUncontrolledEnemiesNearby()` renamed to `GetHotUncontrolledEnemiesNearby()` (now returns the matching actors, not just a count, so the caller can snapshot them); doc comments updated to describe the now-bidirectional state machine |
| `app/Source/KrowdKontrol/OvercrowdDetectionComponent.cpp` | UPDATE | `AdvancePanicOverloadState()` gains the `Active -> Inactive` recovery branch (checks `HasConvergedEnemyBeenControlled()`, flips state, resets `UncontrolledSeconds` to `0.0f`, clears `ConvergedEnemies`, broadcasts); `GetHotUncontrolledEnemiesNearby()` renamed/returns the snapshot array; new `HasConvergedEnemyBeenControlled()` loops the snapshot checking `GetEnemyState() == Controlled` |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolOvercrowdDetectionComponentTest.cpp` | UPDATE | Adds Scenario 6 (CC on a converged member ends Panic Overload, re-broadcasts exactly once more, resets the arming timer) and Scenario 7 (CC on an enemy that joined after the convergence snapshot does *not* end it) |

No new dependencies, no `KrowdKontrol.Build.cs` change.

**Review fix-up (post-review, same PR):** review flagged that an unrelated, unscoped
feature (`FOvercrowdLevelThreshold`/`LevelThresholds`/`NotifyLevelReached()`, attributed
in its own doc comments to issue #23) had been picked up into this PR's diff from the
shared `app/` working tree. Confirmed via `git log`/`git show` against `main` and
`archon/task-fix-issue-23` that this content is genuinely issue #23's own, already
implemented and tested on that separate branch (open as PR #147) — not part of issue
#18's scope or plan. Removed `FOvercrowdLevelThreshold`, `LevelThresholds`,
`NotifyLevelReached()`, and the `FKrowdKontrolOvercrowdLevelThresholdTest` friend grant
from `app-source-tracked/`'s `OvercrowdDetectionComponent.h`/`.cpp` so this PR's tracked
diff matches its actual scope; left untouched in the physical `app/` copy since that
code is PR #147's live, in-progress work sharing the same physical directory (deleting
it there would destroy unmerged work belonging to a different, already-open PR). Also
fixed two stale/misleading doc comments in the genuinely in-scope code
(`OnPanicOverloadStateChanged`'s firing-cap wording, `UncontrolledSeconds`'s reset-path
wording) and hardened Scenario 6's re-arm assertion to isolate the recovery reset from
the (coincidentally identical) below-threshold reset, per review findings.

## Acceptance criteria

- [x] **Landing any of the 5 CC abilities on a converged enemy immediately ends Panic
      Overload while Active.** `AdvancePanicOverloadState()`'s new early branch;
      Scenario 6 asserts the flip to `Inactive` after `ReceiveControl(EAbilitySlot::Sleep)`.
- [x] **No new ability, input, or UI control introduced.** Recovery is entirely a side
      effect of the pre-existing `ReceiveControl()`/`GetEnemyState()` surface; no new
      public API surface added beyond the private helper.
- [x] **Automation test confirms (a) trigger still works, (b) CC on a converged enemy
      ends it immediately, (c) CC on a non-converged enemy does not.** Scenario 6
      covers (a)+(b); Scenario 7 covers (c) under the membership-tracking reading.
- [x] **`python harness/ci.py --quick` and `python harness/ci.py` (full) both exit 0.**
      See Validation below.
- [x] **Code mirrors existing patterns** (flip-before-broadcast, friend-class test
      access, tick-poll detection, per-scenario fresh-`UWorld` tests) — no new patterns
      introduced.
- [x] **No regressions in `KrowdKontrol.Unit.OvercrowdAudioSubsystem` or any other
      existing test.** Full suite passed at 51/51 (49 pre-existing + 2 new scenarios).

## Validation

```
$ python harness/ci.py
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=51
APP_STARTED driver=cli
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
HOLDOUT_ABSENT no .factory/holdout/run.py
MUTATIONS_ABSENT no harness/mutations/run.py
GATE_OK mode=full
```

Re-run after the review fix-up above (post out-of-scope removal + comment fixes +
Scenario 6 hardening): `UnrealBuildTool` compile re-verified separately (`Result:
Succeeded`) before this gate run; still 51/51 `KrowdKontrol.Unit.*` tests passing,
`GATE_OK mode=full`. A `KrowdKontrol.Unit.StationPowerUpComponent` failure seen on one
earlier `--quick` run during initial development (a component this issue never
touches) was isolated, reproduced as non-deterministic cross-test ordering flakiness
unrelated to this diff, and did not recur on re-run — not investigated further as out
of scope for this issue. MISSION.md Hard Invariants reviewed by inspection: the change
is confined to `OvercrowdDetectionComponent`'s state machine and
convergence-membership tracking; `grep` for `Destroy|Kill|SetActorHidden|K2_DestroyActor`
in both changed files returned nothing, so the no-kill invariant is untouched.

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change, per
D-009. Not a substitute for reading `app-source-tracked/` directly.
