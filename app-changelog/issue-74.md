# Issue #74: Post-run summary screen (clear time + Crowd Mastery)

Lands `UPostRunSummaryWidget`, a UMG widget built entirely in C++ (no Widget
Blueprint) that displays a player's run clear time and "Crowd Mastery" count after a
level/run clears, per PRD 13 REQ-5. No real clear-time or Crowd Mastery tracking
exists yet (PRD 06 REQ-2/REQ-3, tracked separately), so the widget seeds itself with
self-demonstrating placeholder values (4:32 / 14) on construction;
`SetSummaryValues(float, int32)` is the real wiring point a future tracking system
will call with live data.

This code is not new. PR #89 first implemented this widget but was auto-rejected for
a CRITICAL bug: its own Automation test called `NativeOnInitialized()` directly on a
bare `NewObject()` widget before `Initialize()` ever ran, dereferencing a null
`WidgetTree` and hitting a fatal engine `check()`. That exact bug was independently
fixed in place under issue #64's PR, while that unrelated issue was getting its own
build+test cycle running (`app/` is a shared, gitignored symlink to the real Unreal
project that persists across PR rejects — the file was never deleted when PR #89
closed). `app-changelog/issue-64.md`'s "Also fixes two pre-existing bugs" section
documented the fix at the time but explicitly noted "no tracked-source mirror exists
for this file (its own issue, #74, never merged), so this fix lives only in `app/`."
This changelog is what finally lands that fix, and the widget it belongs to, under
issue #74's own name — no new C++ was needed; the existing `app/` state already
satisfied every acceptance criterion once re-verified end to end (fresh full-suite
harness run, below).

## Files changed (all under `app/`, gitignored per D-003 — this is the tracked-repo
record of that change, see the closing note below)

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/PostRunSummaryWidget.h` | VERIFY (already present, unlanded under this issue) | `UPostRunSummaryWidget` declaration: `SetSummaryValues(float, int32)`, `GetClearTimeDisplayText()`/`GetCrowdMasteryDisplayText()` accessors, `NativeOnInitialized()`/`Initialize()` double-hook with `EnsureWidgetTreeBuilt()` lazy-`WidgetTree` guard. |
| `app/Source/KrowdKontrol/PostRunSummaryWidget.cpp` | VERIFY (already present, includes the issue #64 crash fix) | Builds a `UBorder` → `UVerticalBox` → 2×`UTextBlock` tree; `EnsureWidgetTreeBuilt()` lazily allocates `WidgetTree` before use (the exact fix for PR #89's rejection reason); formats clear time as M:SS and Crowd Mastery as a plain integer count. |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolPostRunSummaryWidgetTest.cpp` | VERIFY (already present) | `KrowdKontrol.Unit.PostRunSummaryWidget` — covers placeholder render on construction, explicit `SetSummaryValues()` formatting, zero-value and negative-value (floor-at-zero) edge cases, the `Initialize()`/`NativeOnInitialized()` double-build guard, and the unbuilt-tree null-safety path. |
| `app/Source/KrowdKontrol/KrowdKontrol.Build.cs` | VERIFY only — no edit needed | `UMG`/`Slate`/`SlateCore` already present in `PrivateDependencyModuleNames` (added under this issue's name at the time PR #89 was written). |

No new or changed C++ was required — every file above was already correct in `app/`;
this issue's work was verification (re-reading all four files fresh against the
acceptance criteria below) and a real, fresh full-suite harness run to prove nothing
in the substantial unrelated work landed since PR #89 (Paper2D pawn, Room/Door
actors, boss base) broke it.

## Acceptance criteria

- [x] **A UMG screen (`UPostRunSummaryWidget`) displays after construction, showing
      (a) clear time and (b) Crowd Mastery.** `BuildWidgetTree()` constructs
      `ClearTimeText`/`CrowdMasteryText` (`PostRunSummaryWidget.cpp:48-71`) and
      `EnsureWidgetTreeBuilt()` seeds both via `SetSummaryValues()` on construction
      (`PostRunSummaryWidget.cpp:23-46`).
- [x] **Both fields are fed placeholder values, not real tracking.**
      `PlaceholderClearTimeSeconds = 272.0f` (4:32) and
      `PlaceholderCrowdMasteryCount = 14` (`PostRunSummaryWidget.h:84-85`),
      seeded unconditionally by `EnsureWidgetTreeBuilt()`
      (`PostRunSummaryWidget.cpp:44`).
- [x] **Chrome avoids all 5 reserved gameplay-information colours.** Background
      `FLinearColor(0.05, 0.05, 0.05, 0.92)` and text
      `FLinearColor(0.85, 0.85, 0.85, 1.0)` (`PostRunSummaryWidget.cpp:56,62`)
      checked by inspection against `ReservedGameplayColours::GetAll()`
      (`ReservedGameplayColours.cpp:3-31`: Purple `(0.5,0,1)`, Teal `(0,0.8,0.8)`,
      Orange `(1,0.5,0)`, Blue `(0,0.4,1)`, White `(1,1,1)`) — no collision.
- [x] **An Automation Framework test confirms the summary widget, when triggered,
      displays both a clear-time value and a Crowd Mastery value.**
      `KrowdKontrol.Unit.PostRunSummaryWidget` case (a)
      (`KrowdKontrolPostRunSummaryWidgetTest.cpp:52-57`) asserts both display texts
      are non-empty immediately after `CreateWidget()`; case (b)
      (lines 59-66) asserts `SetSummaryValues(272.0f, 14)` formats exactly
      `"Clear Time: 4:32"` / `"Crowd Mastery: 14"`. Confirmed passing for real below
      — both the scoped test alone and the full suite.

## Validation evidence

Scoped run (`harness/run_ue_automation.sh KrowdKontrol.Unit.PostRunSummaryWidget`):

```
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
```

Full-suite run (`python harness/ci.py`, full mode, against the current state of
`app/` — post Paper2D pawn, Room/Door actors, boss base, all landed since PR #89):

```
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=24
APP_STARTED driver=cli
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
HOLDOUT_ABSENT no .factory/holdout/run.py - NOTHING above the independence line ran. Every check in this gate is one the builder could read and iterate against.
MUTATIONS_ABSENT no harness/mutations/run.py - this gate has never been shown to fail. A gate that has never failed is a gate nobody has tested.
GATE_OK mode=full
```

No regression in the wider `KrowdKontrol.Unit.*` suite (24 tests passing project-wide,
including `PostRunSummaryWidget`'s own 6 sub-assertions).

## Deviations from plan

None. No new or changed C++ was needed — every file the investigation/plan flagged
for verification was already correct in `app/`, and the fresh full-suite harness run
confirms it still is.

## Closing note on `app-source-tracked/`

Not applicable to this PR — no `.h`/`.cpp`/`.Build.cs` changed, so no mirror was
created (see D-009's carve-out). For context: `app/` itself is a gitignored symlink
to the real Unreal project on the Windows host (D-003) — binary assets can't live in
this repo without LFS set up ahead of time. When source *does* change,
`app-source-tracked/` mirrors the plain-text `.h`/`.cpp` files touched by that issue
into the tracked repo so the PR carries real, reviewable source instead of just a
description of it.
