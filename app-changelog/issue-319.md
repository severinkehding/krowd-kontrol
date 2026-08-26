# Issue #319: Centre the post-run summary widget's content block

`UPostRunSummaryWidget::BuildWidgetTree()` (`app/Source/KrowdKontrol/PostRunSummaryWidget.cpp`)
set `RootBorder` directly as `WidgetTree->RootWidget`, with no `UCanvasPanel`/
`UCanvasPanelSlot` wrapping it. Slate's viewport-relative anchoring/alignment is a
property of the *slot* a widget occupies inside a `UCanvasPanel`, not a property of
the widget itself, so this made it structurally impossible to centre the widget —
it rendered pinned to the screen's top-left corner instead of centred on both axes,
per the Post-Run Progression PRD's locked 2026-08-26 operator design decision (REQ-1).

The fix roots the tree in a `UCanvasPanel`, wraps the existing `RootBorder` in a new
`USizeBox` width cap (`ContentWidthPx = 480.0f`), and centres that `USizeBox`'s
`UCanvasPanelSlot` on both axes (anchors/alignment `0.5,0.5`, auto-size) — mirroring
`UPunishmentDebugMenuWidget::BuildWidgetTree()`'s already-proven centering mechanism
and `UQuestTrackerWidget`'s issue #310 width-cap + auto-wrap idiom. All four text
fields (`ClearTimeText`, `BestClearTimeText`, `CrowdMasteryText`, `RerunButtonLabel`)
now have `AutoWrapText(true)` set, so overflow grows the box taller rather than
clipping off-screen.

## Acceptance criteria

- [x] Content block centred on both axes via `UCanvasPanel`/`UCanvasPanelSlot` —
      `ContentSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f))` +
      `SetAlignment(FVector2D(0.5f, 0.5f))` + `SetAutoSize(true)`.
- [x] Resolution-safety test coverage added at 1280x720 (minimum) and 3840x2160
      (maximum) target resolutions, mirroring issue #310's envelope-assertion
      pattern (`KrowdKontrolPostRunSummaryWidgetTest.cpp` case (h)).
- [x] Centred-anchoring assertions added (case (g)): root is a `UCanvasPanel` with
      exactly one child, that child's slot is a `UCanvasPanelSlot` with the correct
      anchors/alignment/auto-size, the child is the width-capping `USizeBox` holding
      `ContentWidthPx` and wrapping the live `RootBorder`, and all three summary text
      fields plus the rerun button label auto-wrap.
- [x] `RootBorder->GetContent()`/`GetBrushColor()` unaffected — verified by re-running
      `KrowdKontrolPostRunSummaryRerunButtonTest` and `KrowdKontrolReservedGameplayColoursTest`
      unmodified (both still pass; not edited).
- [x] No change to `RerunButton`'s wiring/behaviour, no change to `HUDChromeColours`
      values, no button-overlap verification (explicitly out of scope, deferred to a
      separate layout-integrity issue).
- [x] `app/` and `app-source-tracked/` copies identical (`diff` clean, verified
      directly, not just via the gate).
- [x] `app-changelog/issue-319.md` written (this file).

## Deviation from investigation.md

None. Implementation shipped investigation's plan exactly as written (Steps 1-3),
including the exact constant value (`ContentWidthPx = 480.0f`), width-cap/auto-wrap
mechanism, and test assertion shapes.

One addition not explicitly spelled out in the plan's code listing: the test file
needed `#include "Blueprint/WidgetTree.h"` and `#include "Components/TextBlock.h"`
added (alongside the `CanvasPanel`/`CanvasPanelSlot`/`SizeBox`/`Border` includes the
plan did list) for `UWidgetTree`/`UTextBlock` to be complete types at the new
assertions' point of use — a real compile error caught locally before commit, not a
design change.

## Validation evidence

`python harness/ci.py --quick`: `GATE_OK` — `UNIT_PASSED tests=119`, `PIE_PASSED tests=5`.

Rebuilt `KrowdKontrolEditor` (UnrealBuildTool) after the includes fix above —
`Result: Succeeded`, confirming the widget and test code compile clean before the
gate ran.

MISSION.md Hard Invariants reviewed against this diff: UI-only change (post-run
summary widget layout/anchoring), does not touch colours (`HUDChromeColours` values
untouched), abilities, enemy roster, or engine/dimensionality.
`app-source-tracked/` mirror contains only the changed `.h`/`.cpp`/test files (no
`.uasset`/`.umap`/`Content/`/`Binaries/`/`Intermediate/`), satisfying invariant #8's
carve-out.

No fixes were required during validation beyond the includes correction above — the
gate passed on the first full quick run afterward.

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
