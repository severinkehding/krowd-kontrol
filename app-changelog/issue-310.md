# Issue #310: Quest tracker widget is partially cut off in the top-right corner during PIE

`UQuestTrackerWidget`'s top-right HUD panel (`app/Source/KrowdKontrol/QuestTrackerWidget.h/.cpp`)
used a fixed `TrackerWidthPx`/`TrackerHeightPx` (160x80px) `CanvasPanelSlot` with
`SetAutoSize(false)`. Its content (three unconstrained text rows in a `VerticalBox`)
was never clamped to that box, and PR #301's directional cue widened the room-state
line further, so the panel rendered partially off-screen during a real Level 1 PIE
clear (confirmed by the operator's own playtest). The fix caps the panel's *width*
only (preserving issue #247's ~15%-of-screen-width envelope via a new `USizeBox`),
wraps all three text rows, and lets the canvas slot auto-size — so the panel now
grows downward instead of clipping off the right edge.

## Acceptance criteria

- [x] Quest tracker panel is fully visible in PIE regardless of content width —
      `TrackerSlot->SetAutoSize(true)` around a width-capping `USizeBox` plus
      `SetAutoWrapText(true)` on all three rows converts "too wide" into "taller."
- [x] Issue #247's ~15%-of-screen-width ceiling preserved, not removed —
      `TrackerWidthPx` (160px) kept as the `USizeBox`'s `WidthOverride`; only the
      now-unused `TrackerHeightPx` was deleted.
- [x] No change to directional-cue computation logic (`ComputeCompassDirection`,
      `ResolveObjectiveDirectionTarget`) — only the container that renders its
      output was touched.
- [x] Automation test updated to assert the new sizing mechanism (auto-size +
      width-cap) instead of the old static footprint
      (`KrowdKontrolQuestTrackerWidgetTest.cpp`).
- [x] `app/` and `app-source-tracked/` copies identical (`diff` clean, verified
      directly, not just via the gate).
- [x] `app-changelog/issue-310.md` written (this file).

## Deviation from investigation.md

None of substance — implementation shipped investigation's own root-cause fix
directly. One correction versus investigation's plan text: investigation's own
"Edge Cases & Risks" table flagged the `~15%` width ceiling as an accepted loss if
both constants were deleted; the shipped fix avoids that loss entirely by keeping
`TrackerWidthPx` as a hard cap via the new `USizeBox`, deleting only the truly
unused `TrackerHeightPx`.

## Validation evidence

`python harness/ci.py --mode full` (per `validation.md`): `GATE_OK` —
`UNIT_PASSED tests=110`, `UE_AUTOMATION_OK passed=1 total=1`, `E2E_PASSED steps=1`.

MISSION.md Hard Invariants reviewed against this diff: UI-only change (quest
tracker widget sizing), does not touch colours, abilities, enemy roster, or
engine/dimensionality. `app-source-tracked/` mirror contains only the changed
`.h`/`.cpp`/test files (no `.uasset`/`.umap`/`Content/`/`Binaries/`/`Intermediate/`),
satisfying invariant #8's carve-out.

No fixes were required during validation — the gate passed on the first full run.

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
