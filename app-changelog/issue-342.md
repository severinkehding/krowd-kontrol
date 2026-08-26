# Issue #342: Post-clear RERUN/FINISH RUN reuse the defeat-restart path wholesale

`AKrowdKontrolPlayerController::RequestLevelRestart()` was built exclusively for the
defeat-restart flow (issue #172/#173): it unconditionally (1) flips `bRestartRequested`
true and (2) reloads with `"BossCheckpoint"` options whenever the world's boss-checkpoint
latch (`ULevelLifecycleSubsystem::HasReachedBossCheckpoint()`) is set. PRs #339 (RERUN
LEVEL) and #335 (NEXT LEVEL's final-level FINISH RUN fallback) added two new *voluntary*
post-clear buttons that reused this exact function with no way to opt out — so a player
who clears a boss level and clicks either button respawned at the boss instead of the
level start, and `WasRestartRequested()` (documented as a defeat-only signal) incorrectly
flipped true.

## Fix

`RequestLevelRestart()` gained a `bool bFreshRun = false` parameter (default keeps
`HandleLevelFailed()`'s existing defeat-path call site byte-for-byte unchanged):

- `bFreshRun = false` (defeat-restart, unchanged behavior): sets `bRestartRequested`,
  honours a latched boss checkpoint via `ComputeRestartOptions()`.
- `bFreshRun = true` (voluntary post-clear rerun, new): leaves `bRestartRequested`
  false, always reloads with empty options (level start), regardless of a latched boss
  checkpoint. Does **not** reset the underlying `HasReachedBossCheckpoint()` latch
  itself — a later genuine defeat in the same reloaded world still correctly restores
  the checkpoint.

Both voluntary click handlers (`UPostRunSummaryWidget::HandleRerunClicked()`,
`HandleNextLevelClicked()`'s final-level branch) now call
`RequestLevelRestart(/*bFreshRun=*/true)`.

Added `AKrowdKontrolPlayerController::WasFreshRunRequested()` (mirrors
`WasRestartRequested()`'s existing "what the Automation test asserts instead of the
real reload" role) so the fresh-run mode is observable from an Automation test World,
since a fresh run no longer flips `bRestartRequested` and the real `OpenLevel()` call
is unreachable in-process.

## Bundled minor fix

`HandleLevelClear()`'s keyboard-focus target moved from `RerunButton` to
`NextLevelButton` — continuing forward (NEXT LEVEL / FINISH RUN) is the more-primary
action on the post-clear screen than retrying, and until this fix NEXT LEVEL was
keyboard-unreachable without relying on Tab order. Trade-off (explicitly accepted, not
an oversight): `RerunButton` is now itself Tab-only for keyboard focus.

## Tests

- `KrowdKontrolBossCheckpointRestartTest.cpp`: new Case F — after latching a boss
  checkpoint, a fresh-run restart (`bFreshRun=true`) leaves `WasRestartRequested()`
  false and reports `WasFreshRunRequested()` true; a defeat-mode restart against the
  same latched checkpoint (`bFreshRun=false`, the default) is unaffected — still flips
  `WasRestartRequested()` true and reports `WasFreshRunRequested()` false. This is the
  issue's explicitly requested pin test.
- `KrowdKontrolPostRunSummaryRerunButtonTest.cpp`: the pre-fix assertion ("clicking
  RERUN LEVEL flips `WasRestartRequested()` true") directly encoded the bug — inverted
  to assert `WasFreshRunRequested()` true and `WasRestartRequested()` stays false.
- `KrowdKontrolPostRunSummaryNextLevelButtonTest.cpp`: same correction for the
  final-level FINISH RUN branch.
- `KrowdKontrolLevelRestartTest.cpp`/existing `KrowdKontrolBossCheckpointRestartTest.cpp`
  cases A-E confirmed unaffected — they only ever call `RequestLevelRestart()`/trigger
  it via `HandleLevelFailed()` with no second argument, defaulting to `bFreshRun=false`.

## Review follow-up (PR #343, self-fix pass)

The PR #343 review's test-coverage agent flagged (HIGH, Finding 1) that Case F only
proved `bLastRestartWasFreshRun`/`bRestartRequested` bookkeeping, not the actual
behavioral crux of this fix: the `bFreshRun ? FString() : ComputeRestartOptions()`
ternary that decides whether the real `OpenLevel()` reload honours a latched boss
checkpoint. That line only ran inside `RequestLevelRestart()`'s `World->IsGameWorld()`
guard, which is never true in Case F's `CreateNewMap()` Automation World — so a future
refactor of the ternary could silently reintroduce this issue's exact bug with no test
failing.

Closed via the review's own recommended Option B: `LastComputedRestartOptions` is now
computed unconditionally (ahead of the `IsGameWorld()` guard) and exposed via a new
`GetLastComputedRestartOptions()` Blueprint-pure accessor, mirroring
`WasFreshRunRequested()`'s existing "test-observability seam" pattern. Case F now also
asserts `GetLastComputedRestartOptions()` is empty for the fresh-run call and
`"BossCheckpoint"` for the defeat-mode call against the same latched checkpoint. The
review's two LOW findings (`WasFreshRunRequested()`'s Blueprint-facing "last call"
semantics; the pre-existing, disclosed `RerunButton`→`NextLevelButton` focus-test gap)
were both left as-is — both reporting agents recommended no code change.

## Scope boundaries — explicitly not done here

- `HasReachedBossCheckpoint()`'s never-reset-once-set latch semantics are unchanged —
  out of scope per the issue's own fix-shape guidance ("skips `ComputeRestartOptions()`",
  not "resets the checkpoint state").
- No main-menu destination for FINISH RUN — already deferred to
  `docs/prd-main-menu.md` per `PostRunSummaryWidget.h`'s own comment.
- No Blueprint/widget-asset changes — this widget builds its tree entirely in C++.
- Real PIE click-through verification (boss-level RERUN/FINISH RUN spawning at the
  level start rather than the boss checkpoint) flagged for manual sign-off — this
  environment cannot drive real Slate input end-to-end.

## Acceptance criteria

- [x] `RequestLevelRestart()` takes a `bFreshRun` parameter; defeat-path call site
      (`HandleLevelFailed()`) unchanged (default `false`)
- [x] Both voluntary post-clear buttons (RERUN LEVEL, final-level FINISH RUN) call
      `RequestLevelRestart(/*bFreshRun=*/true)`
- [x] A fresh-run restart never flips `WasRestartRequested()`, even with a latched boss
      checkpoint — `KrowdKontrol.Unit.BossCheckpointRestart` Case F
- [x] A fresh-run restart always targets the level's own start (empty reload options),
      ignoring a latched boss checkpoint — Case F
- [x] A defeat-mode restart's existing behavior (checkpoint honoured,
      `WasRestartRequested()` flips true) is completely unaffected — Case F,
      `KrowdKontrolLevelRestartTest.cpp`
- [x] Bundled: post-clear keyboard focus moves to `NextLevelButton`
- [x] `python harness/ci.py --quick` reports `GATE_OK`
- [x] `app-source-tracked/` mirror + this changelog written

## Validation evidence

Quick gate (`python harness/ci.py --quick`):

```
HARNESS_START mode=quick driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=121
PIE_PASSED tests=5
GATE_OK mode=quick
```

`UNIT_PASSED tests=121` (up from a pre-change baseline of 119 in the most recent prior
changelog, issue #321) reflects Case F's new assertions running as part of the existing
`KrowdKontrol.Unit.BossCheckpointRestart` test, not a new top-level test name. Full
`--mode full` validation (including a real headless PIE/Editor build) deferred to the
separate `dark-factory-validate` node.

Full gate re-run after the review follow-up above (`python harness/ci.py --mode full`):

```
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=121
PIE_PASSED tests=5
APP_STARTED driver=cli
UE_BUILD_START KrowdKontrolEditor Win64 Development
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
GATE_OK mode=full
```

`UNIT_PASSED tests=121` unchanged — `GetLastComputedRestartOptions()`'s two new
`TestEqual()` calls are additional assertions inside Case F, not new top-level tests.

MISSION.md Hard Invariants reviewed against this diff: no kill-rule, colour-lock,
ability-roster, enemy-roster, engine/dimensionality, networking, or `app`-tracking
invariant is touched.

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
