# Issue #328: Display Accumulated Crowd Mastery Total on the Main Menu

## Summary

Fills the main menu's already-reserved, previously-empty `MasteryDisplayAnchor`
(`UMainMenuWidget`, #324/PR #332) with a `MasteryDisplayText` `UTextBlock` reading
`"CROWD MASTERY: <total>"`, sourced from the GameInstance-scoped
`UCrowdMasteryTotalSubsystem::GetAccumulatedTotal()` (#327/PR #331). The text block is
built and styled exactly like every other chrome text element on this widget (shared
`HUDChromeColours::GetText()` colour), slotted into the anchor via the widget's own
already-public `SetMasteryDisplayContent()` API, and populated once from
`BuildWidgetTree()` via a new private `RefreshMasteryDisplayText()`.

## Refresh-on-show: `NativeConstruct()`, not just construction time

Originally this relied on the fact that `AMainMenuPlayerController::BeginPlay()`
constructs a brand-new `UMainMenuWidget` every time the main menu level loads, making
"read once at construction" and "refresh every time the menu is shown" the same event
in this codebase. PR #350 review (pass 1) correctly flagged that as an unverified
assumption rather than a structural guarantee - nothing in `UMainMenuWidget` itself
enforced the refresh if that controller-level behavior ever changed. Fixed by
overriding `NativeConstruct()` (fired by `AddToViewport()`, and by any future re-add of
the same widget instance) to call `RefreshMasteryDisplayText()` again, in addition to
the existing construction-time read in `BuildWidgetTree()`. This makes the refresh a
property of `UMainMenuWidget` itself, independent of how/when the owning controller
chooses to create or reuse the widget instance.

## Why not reuse `ResolveMasteryTotalSubsystem()`

`UMainMenuWidget` already has (from the in-flight, not-yet-merged issue #329 / PR
#349 mastery-reset feature living in the shared `app/` Unreal project) a
`ResolveMasteryTotalSubsystem()` lazy-cache-plus-warn-once resolver. This change
deliberately does **not** call it from the new display-refresh path: that resolver's
warn-on-missing log is scoped to the reset flow, where a missing subsystem means a
real user-initiated reset attempt silently failed. Here, a missing subsystem during
construction just means "no GameInstance yet" - true for every
`KrowdKontrol.Unit.*` test that constructs this widget via `CreateNewMap()`, not a
noteworthy condition. Reusing the resolver would consume its warn-once flag at
construction time and break `KrowdKontrolMainMenuMasteryResetTest.cpp`'s existing
`AddExpectedError(..., 1)` assertion (which expects the warning to fire exactly once,
at the reset-confirm click), and would force `KrowdKontrolMainMenuLevelSelectTest.cpp`
/ `KrowdKontrolReservedGameplayColoursTest.cpp` to start failing on an unrelated,
previously-unregistered warning. `RefreshMasteryDisplayText()` instead does its own
silent `GetGameInstance()->GetSubsystem<UCrowdMasteryTotalSubsystem>()` resolution,
sharing the same `CachedMasteryTotalSubsystem` cache slot as the reset flow (so
whichever of the two runs first warms the cache for the other) but never logging.

## Deviation: `app-source-tracked/` mirror written against a pre-#329 base

`app/` (the shared, gitignored symlink to the live Unreal project) already contains
issue #329's mastery-reset UI (`MasteryResetBox`, `ResolveMasteryTotalSubsystem()`,
`CachedMasteryTotalSubsystem`, `bHasWarnedMissingMasteryTotalSubsystem`, etc.), since
that work was implemented directly against the same live project. However PR #349
(issue #329) is still **open, unmerged** as of this change - `app-source-tracked/`'s
tracked mirror (which reflects `main`) correctly does not yet contain it. Copying
`app/`'s current full state into the mirror would have pulled #329's unrelated,
unmerged scope into this PR. Instead, the mirror edits below apply *only* this
issue's own diff on top of the mirror's actual pre-#329 content - including adding a
minimal `CachedMasteryTotalSubsystem` member and a `UCrowdMasteryTotalSubsystem`
forward declaration to the mirror (since neither pre-exists there), scoped to exactly
what `RefreshMasteryDisplayText()` needs. When #329 merges, expect a normal, resolvable
merge conflict on `MainMenuWidget.h`/`.cpp` from both PRs independently introducing
`CachedMasteryTotalSubsystem`- this is a merge-order artifact, not a bug in either
change. The real, compiled `app/` state (which the harness validates against) already
has both features working together correctly.

## Post-review fixes

Review of this PR (code-review, error-handling, test-coverage, docs-impact agents)
surfaced one HIGH and two MEDIUM/LOW findings, all addressed here:

- **Silent subsystem-resolution failure (HIGH, error-handling).**
  `RefreshMasteryDisplayText()`'s cold-path `GetGameInstance()->
  GetSubsystem<UCrowdMasteryTotalSubsystem>()` lookup fell through to `0` with no log
  line when a real `GameInstance` was present but the subsystem wasn't resolvable -
  indistinguishable from a legitimate new-player zero. Added a local, independent
  warn-once flag (`bHasWarnedMissingMasteryTotalSubsystemOnDisplay`) that logs once in
  that specific case only; the "no `GameInstance` yet" test-construction case (every
  `KrowdKontrol.Unit.*` test) stays silent as before. Deliberately not shared with
  `ResolveMasteryTotalSubsystem()`'s own warn-once flag (#329/PR #349, unmerged) to
  avoid coupling this PR's fix to that PR's resolver or test-file budget.
- **Test (d2) overstated its own coverage (MEDIUM, code-review + test-coverage,
  converged).** Softened the block's comment to accurately scope the claim to the
  cached-fast-path only - the cold `GetSubsystem<>()` resolution itself has no
  `CreateNewMap()`-World test precedent in this suite and remains unverified by unit
  tests (a repo-wide, pre-existing gap, not unique to this change).
- **PRD REQ-2 not marked implemented (MEDIUM, docs-impact).** Added
  `— ✅ implemented, issue #328` to `docs/prd-crowd-mastery-persistence.md` REQ-2,
  matching `docs/prd-main-menu.md`'s existing convention for shipped requirements.
- **Null-`MasteryDisplayText` guard untested (LOW, test-coverage).** Added a 3-line
  case alongside block (f)'s existing `UnbuiltWidget` pattern, calling
  `RefreshMasteryDisplayText()` on an unbuilt widget and asserting it degrades safely.

Skipped: test-coverage's Option A (a new `KrowdKontrol.PIE.*` main-menu test covering
real `GameInstance` subsystem resolution end-to-end) - this is a repo-wide blind spot
shared by every `Cached*Subsystem` friend-injection test, not introduced by this PR,
and is a genuinely new test surface rather than a fix to what this PR touches. Filed
as a follow-up instead of built here.

Re-validated after these changes: `harness/run_ue_automation.sh
KrowdKontrol.Unit.` -> `UE_AUTOMATION_RESULT passed=124 total=124`;
`harness/run_ue_automation.sh KrowdKontrol.PIE.` -> `UE_AUTOMATION_RESULT passed=5
total=5` - both unchanged from the pre-fix baseline, confirming the new warn-once
branch isn't exercised by any existing test (as expected: block (d)/(d2)/(g) all
construct via a `GameInstance`-less World or bare `NewObject()`).

## Pass-2 fixes (dark-factory validation)

Pass-1 validation (behavioral-validation + the independent E2E holdout, both agreeing)
found the diff satisfied 3 of 4 acceptance criteria, but that the "refreshes whenever
the menu is shown" requirement was only backed by the unverified assumption above, not
a structural mechanism. `REQUEST_CHANGES` with three findings:

- **Refresh-on-show not structurally implemented (HIGH, behavioral).** Fixed by adding
  `UMainMenuWidget::NativeConstruct()` (see "Refresh-on-show" section above) - covered
  by a new test case (d3) that injects a subsystem, reads the display, changes the
  subsystem's total, then calls `NativeConstruct()` directly and asserts the display
  picked up the new total.
- **No evidence of checking for an existing post-run-summary Crowd Mastery style
  (LOW, behavioral).** Confirmed: `UPostRunSummaryWidget::CrowdMasteryText`
  (`PostRunSummaryWidget.cpp`) uses the same `HUDChromeColours::GetText()` chrome
  colour this widget's `MasteryDisplayText` already used - no new style needed, no
  discrepancy to fix. Added an explicit code comment cross-referencing the two so this
  isn't only visible by diffing colour values.
- **Locale-grouped number formatting (LOW, code_quality).** `RefreshMasteryDisplayText()`
  used bare `FText::AsNumber(AccumulatedTotal)`, which would render `"1,000"` once the
  total exceeds 999 - inconsistent with `QuestTrackerWidget.cpp`'s established
  `FNumberFormattingOptions` `NoGrouping` convention for count-like displays. Matched it.

Re-validated: `python harness/ci.py --quick` -> `UNIT_PASSED tests=124`,
`PIE_PASSED tests=5`, `GATE_OK mode=quick` (same test/pass counts as before - the new
(d3) case lives inside the existing `KrowdKontrol.Unit.MainMenuWidget` test function,
not a new top-level test).

## Files changed

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/MainMenuWidget.h` | UPDATE | `MasteryDisplayText` member, `GetMasteryDisplayText()`, `RefreshMasteryDisplayText()` declarations |
| `app/Source/KrowdKontrol/MainMenuWidget.cpp` | UPDATE | Builds/styles/slots `MasteryDisplayText` in `BuildWidgetTree()`; implements `RefreshMasteryDisplayText()` / `GetMasteryDisplayText()` |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolMainMenuWidgetTest.cpp` | UPDATE | Block (d) rewritten for the new auto-filled-at-construction behavior; block (d2) added, proving a real injected total is read |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolReservedGameplayColoursTest.cpp` | UPDATE | New colour-audit line for `MasteryDisplayText`; stale "nothing to audit there" comment removed |
| `app-source-tracked/Source/KrowdKontrol/MainMenuWidget.h` / `.cpp` | UPDATE (mirror) | This issue's diff only, applied against the pre-#329 mirror base (see Deviation above) |
| `app-source-tracked/Source/KrowdKontrol/Private/Tests/KrowdKontrolMainMenuWidgetTest.cpp` | UPDATE (mirror) | Same diff as the real test file |
| `app-source-tracked/Source/KrowdKontrol/Private/Tests/KrowdKontrolReservedGameplayColoursTest.cpp` | UPDATE (mirror) | Same diff as the real test file |

## Acceptance criteria

- [x] Main menu's `MasteryDisplayAnchor` shows `"CROWD MASTERY: <total>"`, `<total>` read from `UCrowdMasteryTotalSubsystem::GetAccumulatedTotal()`
- [x] Display reflects the current total on every main-menu visit (`RefreshMasteryDisplayText()` runs from both `BuildWidgetTree()` at construction and `NativeConstruct()` on every show)
- [x] Chrome styling reuses `HUDChromeColours::GetText()` - no new colours introduced
- [x] `KrowdKontrolReservedGameplayColoursTest.cpp` audits the new element against the five reserved gameplay colours
- [x] `python harness/ci.py --quick` reports `GATE_OK mode=quick`
- [x] `app-source-tracked/` mirror and this changelog written

## Validation Evidence

`python harness/ci.py --quick` -> `UNIT_PASSED tests=124`, `PIE_PASSED tests=5`,
`GATE_OK mode=quick` (includes `KrowdKontrol.Unit.MainMenuWidget` and
`KrowdKontrol.Unit.ReservedGameplayColours`, both green).
