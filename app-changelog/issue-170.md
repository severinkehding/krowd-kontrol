# Issue #170: Wire level clear-time tracking to level-begin/level-clear signals

**Second attempt.** PR #204 (closed, not merged) implemented this correctly in spirit
but subscribed from `AKrowdKontrolPlayerController::BeginPlay()`, which UE 5.8's own
`UWorld::BeginPlay()` dispatch order guarantees runs *after* `OnLevelBegin` has already
fired (world-subsystem `OnWorldBeginPlay()` fires before `GameMode->StartPlay()`, which
is what eventually calls `PlayerController::BeginPlay()`) — a code-review agent verified
this against engine source and rejected the PR at validation. This PR fixes the root
cause: the subscription is now triggered from inside `ULevelLifecycleSubsystem` itself
(the producer of `OnLevelBegin`/`OnLevelClear`), not from any `AActor::BeginPlay()`.

`ULevelClearTimeSubsystem::StartLevelTimer`/`StopLevelTimerAndRecordClear` were fully
implemented and unit-tested in isolation but nothing called them in production — a level
could be played from start to clear and no clear time was ever recorded.

## Files changed (all under `app/`, gitignored per D-003 — this is the tracked-repo
record of that change, see the closing note below)

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/LevelClearTimeSubsystem.h`/`.cpp` | UPDATE | New `SubscribeToLevelLifecycle(ULevelLifecycleSubsystem*)` method, bound via `AddUniqueDynamic` to `OnLevelBegin`/`OnLevelClear`; `HandleLevelBegin(FName)`/`HandleLevelClear()` handlers; `CurrentLevelID` member so the parameterless `OnLevelClear` knows which level to stop/record. Warns and no-ops if `LifecycleSubsystem` is null. Class doc comment updated to describe the new caller. |
| `app/Source/KrowdKontrol/LevelLifecycleSubsystem.h`/`.cpp` | UPDATE | New private `EnsureLevelClearTimeSubscription()` — resolves this world's `GameInstance`'s `ULevelClearTimeSubsystem` and calls `SubscribeToLevelLifecycle(this)`. Called from both `Initialize()` (mirrors `UCrowdMasterySubsystem::Initialize()`'s sibling-subscribe precedent) and `OnWorldBeginPlay()` itself, immediately before its `OnLevelBegin.Broadcast(...)` call (a same-function sequential guarantee, not a cross-object dispatch-order assumption). Silently no-ops (no warning) when `GetGameInstance()` is null — the normal case for this project's `CreateNewMap()`-based Automation test worlds. |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolLevelClearTimeWiringTest.cpp` | CREATE | `KrowdKontrol.Unit.LevelClearTimeWiring` — drives `SubscribeToLevelLifecycle()` directly against a `CreateNewMap()` world (no `GameInstance`/controller involvement): asserts the timer starts on `OnLevelBegin` and records/persists a best time on a real `OnLevelClear` (enemy spawn → detect → control → bank sequence, matching `KrowdKontrolLevelLifecycleSubsystemTest.cpp`'s shape); asserts a repeat `SubscribeToLevelLifecycle()` call does not double-bind, enforced via a `GetExecutionInfo()`/`GetWarningTotal()` snapshot around `RefreshLevelClearState()` (a double-bind would hit `StopLevelTimerAndRecordClear`'s "no active timer" warning on the second handler invocation, and this harness otherwise counts a warning-only test as a pass, not a failure); asserts `SubscribeToLevelLifecycle(nullptr)` is a defensive no-op that logs the documented warning (`AddExpectedError`). |

**No changes to `KrowdKontrolPlayerController.cpp`/`.h` or `EnemyBase.h`** — PR #204's
controller-side wiring is obsoleted by moving the subscription trigger into
`ULevelLifecycleSubsystem` itself.

## Deviations from plan

- **Cleaned up concurrent-task leakage from PR #204 in the shared `app/` symlink.**
  PR #204 was closed/rejected but never merged, and its uncommitted edits — the exact
  rejected `AKrowdKontrolPlayerController::BeginPlay()`-based wiring this issue's plan
  explicitly says not to reintroduce — were still sitting in `app/`'s copies of
  `KrowdKontrolPlayerController.cpp`/`.h`, `KrowdKontrolLevelFailedTest.cpp` (reordered
  for that now-obsolete wiring), and `KrowdKontrolLevelClearTimeWiringTest.cpp` (an
  end-to-end test asserting the rejected controller path). All three were reverted back
  to their `app-source-tracked/` (last-committed) baseline before this PR's real changes
  were applied, so this diff only contains issue #170's actual fix. `LevelClearTimeSubsystem.h`/`.cpp`
  already had PR #204's `SubscribeToLevelLifecycle`/`HandleLevelBegin`/`HandleLevelClear`/
  `CurrentLevelID` shape sitting in `app/` too — that part of PR #204's design was correct
  and unaffected by its ordering bug, so it was kept and only its doc comments (and the
  null-check warning, previously silent) were brought in line with the new caller.
- A second, unrelated diff between `app/` and `app-source-tracked/` exists in
  `FlatCamera3DPrototypePawn.h`/`.cpp`, `Paper2DPrototypePawn.h`/`.cpp`, and two smoke
  test files — this is another in-flight task's uncommitted work (issue #179, Punishment
  System) sharing this machine's `app/` target, not authored by this PR. Left untouched.

## Acceptance criteria

- [x] `ULevelClearTimeSubsystem` subscribes to `ULevelLifecycleSubsystem`'s
      `OnLevelBegin`/`OnLevelClear` via `SubscribeToLevelLifecycle`, called from
      `ULevelLifecycleSubsystem` itself, not from any actor's `BeginPlay()`.
- [x] `OnLevelBegin(mapName)` calls `StartLevelTimer` for the current map.
- [x] `OnLevelClear` calls `StopLevelTimerAndRecordClear`, recording the clear and
      persisting the personal best.
- [x] `KrowdKontrol.Unit.LevelClearTimeWiring` asserts both of the above.
- [ ] **PR body must describe a real PIE playthrough manually verified to produce a
      save file, or honestly document why that could not be done this session** — see
      Manual Verification below.
- [x] Level 1-2 validation commands pass (Level 3 full-suite deferred to
      `dark-factory-validate`).
- [x] No regressions in `KrowdKontrol.Unit.LevelLifecycleSubsystem`,
      `KrowdKontrol.Unit.CrowdMasterySubsystem`, `KrowdKontrol.Unit.LevelClearTimeSubsystem`,
      `KrowdKontrol.Unit.LevelFailed` (all 76 `KrowdKontrol.Unit.*` tests passed, up from
      75 — the +1 is the new wiring test).
- [x] The specific ordering bug that sank PR #204 (subscribing from
      `PlayerController::BeginPlay()`) is not reintroduced — no code path in this PR
      depends on any `AActor::BeginPlay()` running before `OnLevelBegin` broadcasts;
      the subscribe call is inside `ULevelLifecycleSubsystem::OnWorldBeginPlay()` itself,
      before its own `Broadcast()` — a same-function sequential guarantee.

## Manual Verification Required Before Merge (AC #5)

**Could not be performed this session.** This requires the Unreal Editor open on the
Windows host with a live MCP connection to drive a real PIE playthrough (start a level,
clear it, confirm `Saved/SaveGames/KrowdKontrol_LevelClearTimes.sav` is produced/updated
with a plausible recorded time) — the exact gap PR #204 also hit and stated honestly
rather than fabricating. **Before merging, please play a level in PIE to a clear and
confirm the save file exists with a plausible time**, then note the result here or in a
follow-up comment.

## Validation

```
$ python harness/ci.py --quick
HARNESS_START mode=quick driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=76
GATE_OK mode=quick
```

Full `harness/ci.py` (build + hard invariants + E2E) deferred to the
`dark-factory-validate` node per this workflow's own phase split.

---

Source lives under `app/` (gitignored, D-003) — this file is the tracked-repo record
of that change, not a substitute for reading the actual code.
