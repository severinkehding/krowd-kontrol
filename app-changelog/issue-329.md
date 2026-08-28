# Issue #329: Reset control for accumulated Crowd Mastery total

Adds a `RESET` control to the main menu (`UMainMenuWidget`,
`app/Source/KrowdKontrol/MainMenuWidget.h/.cpp`) that zeroes the GameInstance-scoped
Crowd Mastery total via `UCrowdMasteryTotalSubsystem::ResetAccumulatedTotal()` (issue
#327/PR #331, already merged). Since resetting the total is destructive, this is gated
behind an inline confirm/cancel step: clicking `MasteryResetButton` ("RESET") swaps to a
`MasteryResetConfirmButton`/`MasteryResetCancelButton` pair ("CONFIRM RESET"/"CANCEL")
in the same `MasteryResetBox` (`UHorizontalBox`), inserted directly after the existing
`MasteryDisplayAnchor` in `Layout`. Only one state is visible at a time
(`RefreshMasteryResetVisibility()`, driven by `bMasteryResetConfirmPending`). `CANCEL`
reverts to the RESET-only state without touching the subsystem at all; `CONFIRM RESET`
calls `ResolveMasteryTotalSubsystem()` (mirrors `UPostRunSummaryWidget::
ResolveLevelClearTimeSubsystem()`'s exact lazy-cache/warn-once idiom) and then
`ResetAccumulatedTotal()`, disarming the confirm step first so a warning-logging reset
call can never leave the UI stuck.

No generic/reusable confirmation-dialog widget was built - this codebase has zero
Widget Blueprint assets and no modal/popup machinery, and there is exactly one call
site for this pattern right now. See `MainMenuWidget.cpp`'s inline comment and the
plan's "Alternatives Rejected" section for the full reasoning.

This is REQ-3 of `docs/prd-crowd-mastery-persistence.md`.

## Acceptance criteria

- [x] A `RESET` control is added to the main menu, positioned directly adjacent to
      `MasteryDisplayAnchor`
- [x] Activating it shows an explicit confirm/cancel step; declining (`CANCEL`) leaves
      the total untouched
- [x] Confirming (`CONFIRM RESET`) zeroes the total via `UCrowdMasteryTotalSubsystem::
      ResetAccumulatedTotal()` - no partial resets
- [x] `python harness/ci.py` reaches `GATE_OK`
- [x] Hard Invariant #3 (5 reserved gameplay colours) and #8 (Unreal project not
      git-tracked) explicitly verified
- [x] No regressions in `KrowdKontrol.Unit.MainMenuWidget` or
      `KrowdKontrol.Unit.ReservedGameplayColours`
- [x] `app/` and `app-source-tracked/` copies byte-identical for every touched file
- [x] `app-changelog/issue-329.md` written (this file)

## Known, deliberately-not-blocking gap

Issue #328 ("Display accumulated Crowd Mastery total on the main menu", REQ-2) is
still OPEN/unimplemented as of this PR. `MasteryDisplayAnchor` remains exactly as-is -
this issue does not call `SetMasteryDisplayContent()`. The issue body explicitly
allows this reset control to be built in parallel with #328's display; the only
consequence is that AC "the menu's Crowd Mastery display immediately reflects the
reset value" cannot be visually demonstrated in a live PIE session yet, since there is
nothing on screen to show a number at all. `docs/prd-crowd-mastery-persistence.md`
has been updated to mark REQ-3 satisfied and cross-reference this gap, mirroring how
PR #346 flagged the empty-`LevelSequenceTable` gap for issue #325.

## Validation evidence

`python harness/ci.py` (quick mode, per this workflow's Phase 6): `GATE_OK` -
`UNIT_PASSED tests=124`, `PIE_PASSED tests=5`.

Targeted reruns:
- `harness/run_ue_automation.sh KrowdKontrol.Unit.MainMenuMasteryReset` -> `UE_AUTOMATION_RESULT passed=1 total=1`, `UE_AUTOMATION_OK`
- `harness/run_ue_automation.sh KrowdKontrol.Unit.ReservedGameplayColours` -> `UE_AUTOMATION_RESULT passed=1 total=1`, `UE_AUTOMATION_OK`
- `harness/run_ue_automation.sh KrowdKontrol.Unit.MainMenuWidget` -> `UE_AUTOMATION_RESULT passed=1 total=1`, `UE_AUTOMATION_OK`

Post-review addendum: `KrowdKontrolMainMenuMasteryResetTest.cpp` was extended with
`OnClicked.IsBound()` assertions for all three new buttons, a `GetChildIndex`-based
ordering assertion proving `MasteryResetBox` sits directly after `MasteryDisplayAnchor`
in `Layout`, a pre-reset sanity assertion in case (e), and an unbuilt-widget guard case
for `RefreshMasteryResetVisibility()` - closing the four test-coverage gaps flagged by
this PR's review. Rerun after the change: `harness/run_ue_automation.sh
KrowdKontrol.Unit.MainMenuMasteryReset` -> `UE_AUTOMATION_RESULT passed=1 total=1`,
`UE_AUTOMATION_OK`; full `KrowdKontrol.Unit.` -> `passed=124 total=124`; full
`KrowdKontrol.PIE.` -> `passed=5 total=5`.

Hard invariants (MISSION.md's 8): reviewed. Invariant #3 (5 reserved gameplay colours)
explicitly re-audited by extending `KrowdKontrolReservedGameplayColoursTest.cpp`'s
existing main-menu section with the 3 new label colours - no collision found.
Invariant #8 (Unreal project not git-tracked) satisfied by construction - `app/` stays
the gitignored symlink; only this changelog and the `app-source-tracked/` mirror
(`.h`/`.cpp`/test files only, no `.uasset`/`.umap`/`Content/`/`Binaries/`/`Intermediate/`)
are tracked, per D-009.

## Scope limits (not built here)

- **The Crowd Mastery display itself** (issue #328, still open) - `MasteryDisplayAnchor`
  stays exactly as-is; this issue does not call `SetMasteryDisplayContent()`.
- **A generic/reusable confirmation-dialog widget class** - no such pattern exists in
  this codebase and there is exactly one call site for it right now.
- **An `OnTotalChanged` delegate on `UCrowdMasteryTotalSubsystem`** - not needed by this
  issue and would be speculative for #328 to consume without a concrete second caller
  today.
- **Save-file/cross-launch persistence of the reset** (PRD REQ-4) - explicitly a
  separate, deferred requirement.
- **Auto-hiding/timing-out the confirm-pending state** - the PRD only requires an
  explicit confirm/cancel, not a timeout.
- **Real click-through in a live PIE session** - same standing limitation as PR #332
  (Quit button) and PR #346 (level-select buttons): no ability-cast/click input
  primitive reaches real PIE input in this environment. Automation tests call the
  handler functions directly; manual PIE sign-off is recommended before merge, same as
  those PRs.

---

The real Unreal project stays under `app/` (gitignored, D-003) - this changelog and its
matching `app-source-tracked/` copy are the tracked-repo record of that change, per
D-009. Not a substitute for reading `app-source-tracked/` directly.
