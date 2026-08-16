# Issue #74: Post-run summary screen (clear time + Crowd Mastery)

Verify-and-mirror issue, not a behavior change. `UPostRunSummaryWidget` — a UMG
post-run summary screen showing a placeholder clear time and "Crowd Mastery" stat
(PRD 13 REQ-5) — already exists, is already correct, and already works in `app/`.
It was first written under PR #89, which was rejected for a real crash bug
(`EnsureWidgetTreeBuilt()` dereferencing a null `WidgetTree` when
`NativeOnInitialized()` ran before `Initialize()` lazily created one); that crash
was independently fixed as a side effect of unrelated issue #64 work, but the fix
was never re-landed under issue #74's own name, and no tracked-repo record of the
widget's source ever existed (`app-changelog/issue-64.md`'s own closing note flags
this gap by name). A second attempt, PR #109, tried to close the issue with a
changelog-only diff and no `app-source-tracked/` mirror, so the review pipeline had
no code to independently verify claims against and rejected it. This PR does not
change `UPostRunSummaryWidget`'s behavior at all — it mirrors the already-correct,
crash-fixed `app/` source into `app-source-tracked/` for the first time, so a
reviewer and the validation pipeline both have real code to check.

## Files changed

| File | Action | What it contains |
|------|--------|-------------------|
| `app-source-tracked/Source/KrowdKontrol/PostRunSummaryWidget.h` | CREATE | Verbatim mirror of `app/Source/KrowdKontrol/PostRunSummaryWidget.h` — `UPostRunSummaryWidget` declaration, builds its own UI tree in C++ (no Widget Blueprint asset). |
| `app-source-tracked/Source/KrowdKontrol/PostRunSummaryWidget.cpp` | CREATE | Verbatim mirror of `app/Source/KrowdKontrol/PostRunSummaryWidget.cpp` — includes the `Initialize()`/`EnsureWidgetTreeBuilt()` lazy-`WidgetTree`-creation guard that resolved PR #89's crash rejection. |
| `app-source-tracked/Source/KrowdKontrol/Private/Tests/KrowdKontrolPostRunSummaryWidgetTest.cpp` | CREATE | Verbatim mirror of the Automation Framework test — covers placeholder seeding, `SetSummaryValues()` formatting, zero/negative-input clamping, the `Initialize()` guard's "already built, skip" branch, and unbuilt-tree null-safety. |
| `app-source-tracked/Source/KrowdKontrol/KrowdKontrol.Build.cs` | UPDATE | Applied the scoped `UMG`/`Slate`/`SlateCore` `PrivateDependencyModuleNames` hunk (with its explanatory comment) and removed the now-redundant "Uncomment if you are using Slate UI" placeholder, matching `app/`'s live dependency list. The pre-existing, unrelated Paper2D comment-wording drift (issue #55) at lines 20-21 was left untouched — out of scope for this issue. |
| `app-source-tracked/Source/KrowdKontrol/Private/Tests/KrowdKontrolReservedGameplayColoursTest.cpp` | UPDATE | Added `UPostRunSummaryWidget` to the existing MISSION.md Hard Invariant #3 chrome-colour audit (background border + both text fields), closing the "checked by inspection only" gap from the review pass on this PR. |

## Acceptance criteria

- [x] **A new UMG screen displays after a level/run clears, showing (a) clear time
      and (b) Crowd Mastery.** `UPostRunSummaryWidget::BuildWidgetTree()` constructs
      a bordered `UVerticalBox` with two `UTextBlock` fields; `SetSummaryValues()`
      formats and sets both. Confirmed passing via `KrowdKontrol.Unit.PostRunSummaryWidget`.
- [x] **Both fields are wired to placeholder/test values sufficient to demonstrate
      layout and rendering.** `PlaceholderClearTimeSeconds = 272.0f`,
      `PlaceholderCrowdMasteryCount = 14`, seeded in `EnsureWidgetTreeBuilt()` on
      first `Initialize()`/`NativeOnInitialized()`. Real clear-time/Crowd Mastery
      tracking is explicitly out of scope (PRD 06 REQ-2/REQ-3).
- [x] **The screen's chrome does not use any of the five reserved
      gameplay-information colours.** `BuildWidgetTree()`'s background
      (`FLinearColor(0.05, 0.05, 0.05, 0.92)`) and text colour
      (`FLinearColor(0.85, 0.85, 0.85, 1.0)`) don't match
      `ReservedGameplayColours::GetAll()` (Purple `(0.5,0,1,1)`, Teal `(0,0.8,0.8,1)`,
      Orange `(1,0.5,0,1)`, Blue `(0,0.4,1,1)`, White `(1,1,1,1)`) — now covered by a
      regression test (`KrowdKontrol.Unit.ReservedGameplayColours`'s widget (3)),
      not just checked by inspection.
- [x] **An Automation Framework test confirms the widget, when triggered, displays
      both a clear-time value and a Crowd Mastery value.**
      `KrowdKontrol.Unit.PostRunSummaryWidget` — ran explicitly, see validation
      evidence below.

## Validation evidence

Full gate, re-run after the review self-fix pass below:

```
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=24
APP_STARTED driver=cli
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
HOLDOUT_ABSENT no .factory/holdout/run.py
MUTATIONS_ABSENT no harness/mutations/run.py
GATE_OK mode=full
```

Scoped test, run explicitly as extra evidence for AC #4:

```
$ harness/run_ue_automation.sh KrowdKontrol.Unit.PostRunSummaryWidget
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
```

Full mode, first run, no fixes needed. Hard invariants checked by inspection
(5-colour lock, no-kill rule, no networking) — no regressions found.

**Review self-fix pass**: a multi-agent review of this PR found two HIGH-severity gaps
(broken "Deviations from plan" comment pointers; chrome colours not in the automated
`ReservedGameplayColours` audit) plus a MEDIUM inaccurate comment and two LOW gaps
(missing `checkf` on the root panel attach; guard tested in only one call order). All
were fixed directly in both `app/` and `app-source-tracked/` (kept byte-identical, per
D-009) and the full gate above was re-run afterward — still `GATE_OK` on the first
re-run, `tests=24` unchanged in count (two existing test files extended, not new ones
added).

## Deviations from plan

No Widget Blueprint asset is used — `UPostRunSummaryWidget::BuildWidgetTree()` constructs
its `UBorder`/`UVerticalBox`/`UTextBlock` tree directly in C++ via
`WidgetTree->ConstructWidget<T>()`. This keeps the widget's structure in
`app-source-tracked/`-reviewable C++ rather than a binary `.uasset` that can't be
mirrored under the D-009 carve-out — same pattern as `UAbilityCooldownTrayWidget` and
`UEnergyMeterWidget`.

Both `NativeOnInitialized()` and `Initialize()` independently call
`EnsureWidgetTreeBuilt()` (idempotent via the `!ClearTimeText` guard) rather than
relying on just one hook, because `UUserWidget::Initialize()` only invokes
`NativeOnInitialized()` when the widget has a valid `PlayerContext` — which doesn't
hold for `CreateWidget(World, Class)` with no owning player/controller, exactly how
this widget's own Automation test (and the headless `-nullrhi` automation runner)
constructs it. Same pattern as `UAbilityCooldownTrayWidget::EnsureWidgetTreeBuilt()`
(issue #66); this PR's test now exercises both call orders (cases (e) and (e2)) to
prove the guard is symmetric, not just correct for the order that crashed PR #89.

## Closing note on `app-source-tracked/`

`app/` itself is a gitignored symlink to the real Unreal project on the Windows
host (D-003) — binary assets can't live in this repo without LFS set up ahead of
time. Unlike issue #64's changelog (which touched `app/` files with no matching
mirror at the time) and unlike PR #109's rejected attempt on this same issue (which
shipped a changelog with no mirror at all), **this PR's entire purpose is landing
the `app-source-tracked/` mirror** — all 3 source files plus the `Build.cs` hunk
listed above are new/changed under `app-source-tracked/Source/KrowdKontrol/` in
this diff, not just described in prose. `app/` stays exactly as-is; this is a copy
for review, not a new live link, and it's the specific gap that sank PR #109.
