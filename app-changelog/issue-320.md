# Issue #320: Rerun-level button on the post-run summary screen

Adds a `[RERUN LEVEL]` button to `UPostRunSummaryWidget` (the post-run clear
screen), positioned below the info block and above `NextLevelButton`, that reloads
the current level by calling `AKrowdKontrolPlayerController::RequestLevelRestart()`
directly — the same shared reload path the existing defeat-restart flow (issue
#223) already uses, so no second reload implementation exists. `HandleLevelClear()`
now sets `FInputModeGameAndUI` with focus on `RerunButton`, so keyboard Enter/Space
activates it without a prior Tab press, in addition to mouse click.

This is a re-submission of a previously-rejected PR (#338). That PR's feature code
was correct and complete but did not compile standalone: `RequestLevelRestart()` was
still `private` in the PR's own tracked `KrowdKontrolPlayerController.h`, with no
friend grant, so the diff depended on sibling PR #335 landing first to even build.
This PR fixes that by carrying the `private:` → `public:` move for
`RequestLevelRestart()` in its own diff, so it compiles independently of #335's merge
status or order.

## Scope disclosures

- **`PostRunSummaryWidget.h`/`.cpp`'s diff also carries sibling issue #321's NEXT
  LEVEL button code** (`NextLevelButton`, `NextLevelButtonLabel`,
  `HandleNextLevelClicked()`, `ResolvedNextLevelMapName`,
  `GetNextLevelButtonDisplayText()`, and the `FKrowdKontrolPostRunSummaryNextLevelButtonTest`
  friend grant). Both features share the same widget file and were built in the same
  shared, gitignored `app/` tree window. **That code belongs to #321, not this
  change** — this PR does not claim it as its own work, and does not add #321's own
  test file (`KrowdKontrolPostRunSummaryNextLevelButtonTest.cpp`) to the mirror.
- **This PR deliberately excludes an unrelated, unmerged fix** present in the live
  `app/Source/KrowdKontrol/KrowdKontrolPlayerController.cpp`:
  `SetupInputComponent()`'s `bConsumeInput = false` / `bExecuteWhenPaused = true` fix
  on the briefing-dismiss `AnyKey` binding (fixing "all input dead" regressions from
  the 2026-08-24/26 playtests). That fix belongs to a different, currently in-flight
  task. `KrowdKontrolPlayerController.cpp` is untouched by this PR — only
  `KrowdKontrolPlayerController.h`'s access specifier changed.

## Acceptance criteria

- [x] `[RERUN LEVEL]` button exists on `UPostRunSummaryWidget`, positioned below the
      info block and above `NextLevelButton`.
- [x] Activating it calls `AKrowdKontrolPlayerController::RequestLevelRestart()`
      directly — the same function `HandleLevelFailed()`'s defeat-restart uses. No
      second reload implementation exists.
- [x] `RequestLevelRestart()` is `public` in this PR's own diff of
      `KrowdKontrolPlayerController.h` — not dependent on sibling PR #335/#321
      merging first. This directly addresses the prior rejection's CRITICAL finding.
- [x] Keyboard (Enter/Space via focus + `FInputModeGameAndUI`) and mouse click both
      reach the button at the code level; real-input confirmation in a live PIE
      session is a manual step (no MCP primitive reaches real PIE mouse/keyboard
      input — see prior precedent on #262/#324/the original #338).
- [x] `KrowdKontrol.Unit.PostRunSummaryRerunButton` exists, compiles, and passes:
      real click wiring via a spawned `AKrowdKontrolPlayerController` +
      `SetAsLocalPlayerController()` asserting `WasRestartRequested()` flips true; a
      no-owning-player degrade path with a one-shot warning guard
      (`AddExpectedError(..., 1, false)`); and a layout-order assertion
      (`RerunButton` renders before `NextLevelButton`).
- [x] `app/` and `app-source-tracked/` copies of every touched/added file are
      byte-identical (`diff`, no output): `PostRunSummaryWidget.h/.cpp`,
      `KrowdKontrolPlayerController.h`, the new test file.
- [x] `KrowdKontrolPlayerController.cpp` is untouched in this PR — mirror and live
      diverge only on the unrelated concurrent-task fix described above, confirmed
      still present (i.e. correctly left alone) after this PR's copy step.
- [x] `docs/prd-post-run-progression.md` REQ-2 annotated as implemented.
- [x] `python harness/ci.py` reports `GATE_OK` (full mode).

## Validation evidence

Full gate (`python harness/ci.py`, mode=full):

```
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=120
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

`UNIT_PASSED tests=120` already includes `KrowdKontrol.Unit.PostRunSummaryRerunButton`
(and sibling #321's own new test) since `app/` is the live, shared tree both were
built and compiled against.

MISSION.md Hard Invariants reviewed against this diff: no kill-rule, colour-lock,
ability-roster, enemy-roster, engine/dimensionality, networking, or `app`-tracking
invariant is touched. `RequestLevelRestart()`'s function body is byte-identical
between live and mirror — only its header access specifier changed.

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
