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

## Why construction-time-only, no tick/delegate refresh

`AMainMenuPlayerController::BeginPlay()` constructs a brand-new `UMainMenuWidget`
every time the main menu level loads - returning to the menu after a run means a real
`UGameplayStatics::OpenLevel()` travel, which destroys the current World (widget
included) and spins up a fresh one, triggering `BeginPlay()` and a fresh
`BuildWidgetTree()` call again. `UCrowdMasteryTotalSubsystem`, by contrast, is
GameInstance-scoped and survives that level transition. So "read once at
construction" and "refresh every time the menu is shown" are the same event in this
codebase - there is no code path where an existing `UMainMenuWidget` instance stays
alive while the accumulated total changes underneath it. No `NativeOnActivated`/tick
machinery was added for a case that cannot occur.

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
- [x] Display reflects the current total on every main-menu visit (structural: fresh widget construction + fresh subsystem read on every `L_MainMenu` load)
- [x] Chrome styling reuses `HUDChromeColours::GetText()` - no new colours introduced
- [x] `KrowdKontrolReservedGameplayColoursTest.cpp` audits the new element against the five reserved gameplay colours
- [x] `python harness/ci.py --quick` reports `GATE_OK mode=quick`
- [x] `app-source-tracked/` mirror and this changelog written

## Validation Evidence

`python harness/ci.py --quick` -> `UNIT_PASSED tests=124`, `PIE_PASSED tests=5`,
`GATE_OK mode=quick` (includes `KrowdKontrol.Unit.MainMenuWidget` and
`KrowdKontrol.Unit.ReservedGameplayColours`, both green).
