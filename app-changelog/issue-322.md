# Issue #322: Add layout-integrity automation test for post-run summary info block and buttons

`UPostRunSummaryWidget` (`app/Source/KrowdKontrol/PostRunSummaryWidget.cpp`) now has a
centred info block (#319/PR #340), a RERUN LEVEL button (#320/PR #339), and a NEXT
LEVEL button (#321/PR #335), all merged. Each of those issues added its own narrow
test coverage for its own change, but nothing asserted the combined contract this
issue requires: the info block and both buttons must never overlap and must never
clip off-screen, at every supported resolution, as one dedicated, standing regression
test. Issue #319's own changelog (`app-changelog/issue-319.md`) explicitly notes "no
button-overlap verification (explicitly out of scope, deferred to a separate
layout-integrity issue)" — this is that issue.

The fix is a test-only addition:
`KrowdKontrolPostRunSummaryLayoutIntegrityTest.cpp`
(`KrowdKontrol.Unit.PostRunSummaryLayoutIntegrity`) proves the contract analytically,
since this project's `KrowdKontrol.Unit.*` tests run headless (`-nullrhi`,
`CreateNewMap()` Editor World, no live Slate/viewport realization —
`AddToViewport()` is a documented no-op here). Non-overlap is proven structurally:
`ClearTimeText`, `BestClearTimeText`, `CrowdMasteryText`, `RerunButton`, and
`NextLevelButton` are all direct children of the single `UVerticalBox` ("Layout")
wrapped by `RootBorder`, in an order that keeps the info block above both buttons and
`RerunButton` above `NextLevelButton` — a `UVerticalBox` stacks children into
non-overlapping rows by construction, so this ordering proof *is* the overlap
guarantee. This extends `KrowdKontrolPostRunSummaryRerunButtonTest.cpp` case (c)'s
existing `GetChildIndex()` check (which only covers `CrowdMasteryText`/`RerunButton`)
to all five elements as its own dedicated contract. On-screen containment is proven
analytically: the content block sits in a `USizeBox` capped at `ContentWidthPx` x
`ContentHeightPx`, centred via a `UCanvasPanelSlot` with anchors/alignment
`(0.5, 0.5)` (issue #319) — a rectangle of that size centred on a screen of size
`(Sw, Sh)` stays within the screen bounds iff `ContentWidthPx <= Sw` and
`ContentHeightPx <= Sh`, checked at both 1280x720 (minimum) and 3840x2160 (maximum)
target resolutions. This is a strictly stronger, explicitly-named version of
`KrowdKontrolPostRunSummaryWidgetTest.cpp` case (h)'s existing 50%-fraction margin
check.

No production code changed — `ContentWidthPx`/`ContentHeightPx`/`RootBorder`/etc. are
private members, so `PostRunSummaryWidget.h` gained one new `friend class` line for
the new test class, the same mechanism the five existing friend declarations already
use.

## Acceptance criteria

- [x] An automation test asserts the information block's bounding box and both
      buttons' bounding boxes never overlap, via the `GetChildIndex()` structural
      ordering proof described above (all five elements share one `UVerticalBox`
      parent, info block precedes both buttons, `RerunButton` precedes
      `NextLevelButton`).
- [x] The test runs the assertion at both 1280x720 and 3840x2160.
- [x] The test also asserts the information block and both buttons stay fully
      on-screen (no clipping off any edge) at both resolutions
      (`ContentWidthPx <= ScreenWidth`, `ContentHeightPx <= ScreenHeight`).
- [x] `harness/ci.py --quick` passes (`GATE_OK`), unit test count incremented by
      exactly 1 (this issue adds exactly one
      `IMPLEMENT_SIMPLE_AUTOMATION_TEST`, no other test files touched), PIE test
      count unchanged.
- [x] `app/` and `app-source-tracked/` copies are byte-identical (`diff` clean,
      verified directly).
- [x] `app-changelog/issue-322.md` written (this file).

## Deviation from investigation.md

None. Implementation shipped the plan exactly as written (Tasks 1-4): the friend
declaration, the new test file's two assertion groups (structural non-overlap +
analytical on-screen containment), the `app-source-tracked/` mirror, and this
changelog. No changes to `KrowdKontrolPostRunSummaryWidgetTest.cpp`,
`KrowdKontrolPostRunSummaryRerunButtonTest.cpp`, or
`KrowdKontrolPostRunSummaryNextLevelButtonTest.cpp` — the new test is fully additive,
as scoped.

## Validation evidence

`harness/run_ue_automation.sh KrowdKontrol.Unit.PostRunSummaryLayoutIntegrity`:
`UE_BUILD_OK`, `UE_AUTOMATION_RESULT passed=1 total=1`, `UE_AUTOMATION_OK` — the new
test passes standalone.

`python harness/ci.py --quick`: `GATE_OK` — `UNIT_PASSED tests=125`, `PIE_PASSED
tests=5` (unchanged from pre-change, confirming no `KrowdKontrol.PIE.*` coverage was
added, per scope).

MISSION.md Hard Invariants reviewed against this diff: test-only change (no
production layout code touched), does not affect colours, abilities, enemy roster, or
engine/dimensionality. `app-source-tracked/` mirror contains only the changed
`.h`/new test `.cpp` file (no `.uasset`/`.umap`/`Content/`/`Binaries/`/`Intermediate/`),
satisfying invariant #8's carve-out.

No fixes were required during validation — the new test passed standalone on the
first build, and the full gate passed on the first full quick run afterward.

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
