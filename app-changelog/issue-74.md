# Issue #74: Add post-run summary screen with clear time and Crowd Mastery stat

Adds `UPostRunSummaryWidget`, the project's first UMG widget — a C++-only
`UUserWidget` (no Widget Blueprint `.uasset`) that builds its own
`UBorder` → `UVerticalBox` → two `UTextBlock` layout tree via
`WidgetTree->ConstructWidget<T>()` and self-seeds with placeholder clear-time
(4:32) and Crowd Mastery (14) values on construction, so the screen's layout
and both fields are demonstrable ahead of the real tracking systems (PRD 06
REQ-2/REQ-3, separately scoped). `SetSummaryValues(float, int32)` is the
public wiring point a future real tracking system replaces the placeholder
call with. Chrome (near-black background, light-gray text) deliberately
avoids all five reserved gameplay-information colours (MISSION.md Hard
Invariant 3).

## Files changed (all under `app/`, gitignored per D-003 — this is the
tracked-repo record of that change, see the closing note below)

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/PostRunSummaryWidget.h` | CREATE | `UPostRunSummaryWidget : public UUserWidget` declaration — `SetSummaryValues()`, `GetClearTimeDisplayText()`/`GetCrowdMasteryDisplayText()` getters, `NativeOnInitialized()`/`Initialize()` overrides |
| `app/Source/KrowdKontrol/PostRunSummaryWidget.cpp` | CREATE | Widget tree construction (`BuildWidgetTree()`) and M:SS/count formatting (`SetSummaryValues()`) |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolPostRunSummaryWidgetTest.cpp` | CREATE | `KrowdKontrol.Unit.PostRunSummaryWidget` — covers placeholder-render-on-construction, explicit `SetSummaryValues` formatting, and the zero-value edge case |
| `app/Source/KrowdKontrol/KrowdKontrol.Build.cs` | UPDATE | Activated `UMG`/`Slate`/`SlateCore` private dependencies (the module's first UI dependency) |

## Acceptance criteria

- [x] **`UPostRunSummaryWidget` exists, builds its own layout tree, and
      displays both a clear-time value and a Crowd Mastery value the instant
      it's constructed.** Both `NativeOnInitialized()` (normal player-owned
      path) and an `Initialize()` safety net (player-less construction, e.g.
      the Automation test) build the tree and seed placeholder values before
      the widget's first use.
- [x] **No literal colour value matches any of the five reserved
      gameplay-information colours (Purple/Teal/Orange/Blue/White).**
      Background is `FLinearColor(0.05, 0.05, 0.05, 0.92)`, text is
      `FLinearColor(0.85, 0.85, 0.85, 1.0)` — neither is pure white nor any
      other locked colour. Reviewed by inspection, matching how the colour
      lock is enforced project-wide today.
- [x] **`KrowdKontrol.Unit.PostRunSummaryWidget` Automation Framework test
      exists and passes, confirming both fields render.** Covers (a)
      placeholder values non-empty on construction, (b) `SetSummaryValues`
      formats clear time as `M:SS` and Crowd Mastery as a plain count, (c)
      zero-value edge case (`0:00` / `0`).
- [x] **No regressions in existing `KrowdKontrol.Unit.*` tests.** 6/6 passed
      (previously 5, now includes this widget's test).

## Validation

```
$ python harness/ci.py
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=6
APP_STARTED driver=cli
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
HOLDOUT_ABSENT no .factory/holdout/run.py
MUTATIONS_ABSENT no harness/mutations/run.py
GATE_OK mode=full
```

`UNIT_PASSED tests=6` matches the implementation report's inline check (6/6,
including the new `KrowdKontrol.Unit.PostRunSummaryWidget` test, no
regressions). MISSION.md Hard Invariant 3 reviewed against
`PostRunSummaryWidget.cpp`'s two literal `FLinearColor` values — both hold.
No enemy roster, ability roster, engine/dimensionality, or multiplayer
changes in this diff; no governance files modified.

## Deviations from plan

The plan's chosen sole extension point, `NativeOnInitialized()`, doesn't fire
in the Automation test's player-less `CreateWidget(World, StaticClass())`
construction path (confirmed against UE 5.8 engine source:
`UUserWidget::Initialize()` only calls it when `PlayerContext.IsValid()`).
The plan's own documented fallback (`NativeConstruct()` + `TakeWidget()`)
turns out to be a real functional bug, not just a test workaround —
`TakeWidget()` caches `RebuildWidget()`'s result *before* `NativeConstruct()`
fires, which would permanently cache an empty widget the first time the
screen is ever shown in real gameplay. Fix applied instead: kept
`NativeOnInitialized()` for the normal-usage path and added an `Initialize()`
override as a safety net that builds the tree unconditionally on first
initialization if it wasn't already built. Everything else (file placement,
comment style, colour choice, M:SS formatting, test structure) matches the
plan as written.

## Review fixes (post-review self-fix pass)

The PR review flagged the `Initialize()`/`NativeOnInitialized()` double-build guard as
its main risk: its correctness rested on an unverified assumption about which hook
fires first, and the guard's own "already built, skip" branch had zero test coverage.
Both concerns are resolved together:

- Extracted `EnsureWidgetTreeBuilt()`, called unconditionally from both
  `NativeOnInitialized()` and `Initialize()`. Correctness no longer depends on engine
  call order between the two hooks — whichever fires first builds the tree via the one
  shared `!ClearTimeText` check, the other becomes a no-op.
- Added a friend-class (`FKrowdKontrolPostRunSummaryWidgetTest`, matching
  `PlayerEnergyComponentTest`'s existing precedent) test case that calls
  `NativeOnInitialized()` then `Initialize()` directly on a bare `NewObject()` widget,
  asserting the tree is *not* rebuilt the second time — the guard-skip branch that
  previously had no coverage.
- Added a negative-input test (`SetSummaryValues(-5.0f, -3)` floors to `0:00`/`0`) and
  a bare-construction test (getters return empty text, `SetSummaryValues()` doesn't
  crash, before any tree exists).
- Added `UE_LOG(LogTemp, Warning, ...)` to `SetSummaryValues()`'s null-text-block
  branches, matching `RoomEnemyBudgetController.cpp`'s existing precedent for
  diagnosable-but-silent failure paths.
- Fixed two stale `plan.md` comment references (that file doesn't exist in this repo)
  to point at this changelog's Deviations section instead.

`KrowdKontrol.Unit.*` still reports `UNIT_PASSED tests=6` (same 6 automation test
*cases* — the new coverage is additional assertions inside the existing
`KrowdKontrol.Unit.PostRunSummaryWidget` case, not a new case).

Two docs-impact findings (CLAUDE.md's Tech Stack section now understating this PR's
`UMG`/`Slate`/`SlateCore` deps; Repo Layout omitting `app-source-tracked/`/
`app-changelog/`) were **not** fixed here — `CLAUDE.md` is a protected path
(`FACTORY_RULES.md` §5, restated in `CLAUDE.md`) the factory cannot modify; flagged for
manual edit.

---

Source lives under `app/` (gitignored, D-003) — this file is the tracked-repo
record of that change, not a substitute for reading the actual code.
