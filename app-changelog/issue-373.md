# Issue #373: MASTERY Screen Shell

## Summary

Adds `UMasteryScreenWidget` (C++-built tree, same no-Widget-Blueprint lineage as
`UMainMenuWidget`/`UPostRunSummaryWidget`): a title ("MASTERY"), an
"UNSPENT POINTS: {0}" text block reading
`UCrowdMasteryTotalSubsystem::GetAccumulatedTotal()`, and a BACK button that
broadcasts a new `OnBackRequested` delegate with no subsystem side effects.
`UMainMenuWidget` gains a MASTERY button (built between the level-select list and
the mastery-display anchor) that lazily creates the screen on first click, binds
`OnBackRequested` once, and swaps visibility: `RootBorder->SetVisibility(Collapsed)`
+ `MasteryScreenWidgetInstance->AddToViewport()` to open, the reverse on BACK.
Reusing `AddToViewport()`'s `NativeConstruct()` re-fire gives "refreshed on screen
open" for free, mirroring `UMainMenuWidget`'s own mastery-display refresh contract.

This is `docs/prd-mastery-skill-tree.md` REQ-2's scaffolding only, per the issue
body: no node/bubble tree content, no spend/refund logic - "unspent points" is
displayed as `GetAccumulatedTotal()` verbatim since nothing can be spent yet. The
visual skill tree itself and REQ-1's spend/refund data model are separate follow-up
work.

## Why no generic modal/widget-switcher framework

This codebase has zero Widget Blueprint assets and no modal/popup widget machinery
anywhere (`MainMenuWidget.cpp`'s own RESET/CONFIRM row comment explains why), and
there is exactly one call site for a screen swap right now. The two-widget
hide/show swap (`RootBorder->SetVisibility(Collapsed)` / `AddToViewport()` +
`RemoveFromParent()`) mirrors that same "smallest addition that satisfies the AC"
precedent rather than introducing `UWidgetSwitcher` for a single caller - same
reasoning issue #329's changelog gives for not building a reusable confirm-dialog
widget.

## Files changed

| File | Action | What it contains |
|------|--------|-------------------|
| `Source/KrowdKontrol/MasteryScreenWidget.h` / `.cpp` | CREATE | `UMasteryScreenWidget` - title, points display, BACK button + `OnBackRequested` delegate |
| `Source/KrowdKontrol/MainMenuWidget.h` / `.cpp` | UPDATE | Adds `MasteryButton`/`MasteryButtonLabel`/`MasteryScreenWidgetInstance`, `HandleMasteryButtonClicked()`/`HandleMasteryScreenBackRequested()` |
| `Source/KrowdKontrol/Private/Tests/KrowdKontrolMasteryScreenWidgetTest.cpp` | CREATE | `KrowdKontrol.Unit.MasteryScreenWidget` |
| `Source/KrowdKontrol/Private/Tests/KrowdKontrolMainMenuMasteryScreenTest.cpp` | CREATE | `KrowdKontrol.Unit.MainMenuMasteryScreen` |
| `Source/KrowdKontrol/Private/Tests/MasteryScreenBackRequestedTestListener.h` / `.cpp` | CREATE | Test-only `AddDynamic` listener for `OnBackRequested` (dynamic multicast delegates need a UFUNCTION target, not a lambda) |
| `Source/KrowdKontrol/Private/Tests/KrowdKontrolReservedGameplayColoursTest.cpp` | UPDATE | Extends block (8)'s `MainMenuWidget` audit with `MasteryButtonLabel`; adds block (9) auditing `UMasteryScreenWidget`'s `RootBorder`/`TitleText`/`PointsText`/`BackButtonLabel` |

Present in both `app/Source/KrowdKontrol/...` (the real Editor-built copy) and
`app-source-tracked/Source/KrowdKontrol/...` (the plain-text mirror `create-pr`
needs to open a PR at all) - verified byte-identical for every touched file.

No `.Build.cs` change needed - `UMG`/`Slate`/`SlateCore` are already
`PrivateDependencyModuleNames` in `KrowdKontrol.Build.cs`.

## Acceptance criteria

- [x] Main menu gains a "MASTERY" entry - `MasteryButton` built in
      `UMainMenuWidget::BuildWidgetTree()`, proven by
      `KrowdKontrolMainMenuMasteryScreenTest.cpp` block (a)
- [x] Navigates to a new tree screen widget - `HandleMasteryButtonClicked()` creates
      and shows `UMasteryScreenWidget`, proven by block (b)
- [x] Displays current unspent mastery points, read from
      `UCrowdMasteryTotalSubsystem` - `RefreshPointsDisplayText()`, proven by
      `KrowdKontrolMasteryScreenWidgetTest.cpp` blocks (b)/(c) and
      `KrowdKontrolMainMenuMasteryScreenTest.cpp` block (c)
- [x] Refreshed on screen open - `NativeConstruct()` re-fire via `AddToViewport()`,
      proven by `KrowdKontrolMasteryScreenWidgetTest.cpp` block (d)
- [x] Back control returns to main menu without side effects -
      `HandleBackClicked()`/`HandleMasteryScreenBackRequested()` touch no subsystem,
      proven by `KrowdKontrolMainMenuMasteryScreenTest.cpp` block (d)
- [x] Chrome uses neutral colours only (Hard Invariant 3) -
      `HUDChromeColours::GetBackground()`/`GetText()` exclusively, mechanically
      enforced by the extended `KrowdKontrolReservedGameplayColoursTest.cpp`
- [x] Screen renders with no tree content yet - `BuildWidgetTree()` has no
      node/bubble construction, by design
- [x] `python harness/ci.py --quick` reaches `GATE_OK`
- [x] No regressions in `KrowdKontrol.Unit.MainMenuWidget`,
      `KrowdKontrol.Unit.MainMenuMasteryReset`, `KrowdKontrol.Unit.MainMenuLevelSelect`
- [x] `app/` and `app-source-tracked/` copies byte-identical for every touched file
- [x] `app-changelog/issue-373.md` written (this file)

## Known limitation on the "no duplicate bind" proof

`KrowdKontrolMainMenuMasteryScreenTest.cpp` block (e) proves a second MASTERY click
reuses the same `MasteryScreenWidgetInstance` pointer (a regression removing the
`if (!MasteryScreenWidgetInstance)` guard would return a *different* instance and
fail this assertion directly) and that a single `OnBackRequested.Broadcast()` call
reaches a test-side listener exactly once. Dynamic multicast delegates fire every
bound entry once per `Broadcast()` regardless of how many times any one entry was
bound, so this does not independently re-derive "AddDynamic was called exactly
once" as its own number - the actual guarantee is structural (`AddDynamic` only
executes inside the same first-creation branch as `CreateWidget`, directly visible
in `HandleMasteryButtonClicked()`), and the pointer-identity assertion is what
would catch a regression to that structure.

## Manual PIE sign-off still required

Same standing limitation as every prior main-menu PR (#324, #329, #346): no
ability-cast/click input primitive reaches real PIE input in this environment
(`holdout_no_ability_cast_input_primitive`). Automation tests call
`HandleMasteryButtonClicked()`/`HandleBackClicked()`/`OnBackRequested.Broadcast()`
directly rather than through a real Slate click. Manual PIE click-through is
recommended before merge, same as those PRs.

## Validation evidence

`python harness/ci.py --quick` → `GATE_OK mode=quick` (`UNIT_PASSED tests=129`,
`PIE_PASSED tests=8`).

Targeted reruns (all `UE_AUTOMATION_OK`, `passed=1 total=1` each):
- `harness/run_ue_automation.sh KrowdKontrol.Unit.MasteryScreenWidget`
- `harness/run_ue_automation.sh KrowdKontrol.Unit.MainMenuMasteryScreen`
- `harness/run_ue_automation.sh KrowdKontrol.Unit.MainMenuWidget` (regression)
- `harness/run_ue_automation.sh KrowdKontrol.Unit.MainMenuMasteryReset` (regression)
- `harness/run_ue_automation.sh KrowdKontrol.Unit.MainMenuLevelSelect` (regression)
- `harness/run_ue_automation.sh KrowdKontrol.Unit.ReservedGameplayColours` (regression)

Hard invariants (`MISSION.md`'s 8): reviewed. Invariant #3 (5 reserved gameplay
colours) explicitly re-audited by extending
`KrowdKontrolReservedGameplayColoursTest.cpp` with the new widget's 4 chrome
colours plus `MainMenuWidget`'s new button label - no collision found. Invariant #8
(Unreal project not git-tracked) satisfied by construction - `app/` stays the
gitignored symlink; only this changelog and the `app-source-tracked/` mirror
(`.h`/`.cpp`/test files only) are tracked, per D-009.

## Scope limits (not built here)

- **The node/bubble tree visualization itself** (PRD REQ-2's visual half) -
  explicitly a separate follow-up issue per this issue's own body.
- **Spend/refund tracking on `UCrowdMasteryTotalSubsystem`** (PRD REQ-1) - not
  built here; the follow-up issue that adds spend tracking will need to change
  `RefreshPointsDisplayText()`'s read to subtract a real "spent" query.
- **Skill effects, modifier slots, respec-as-full-tree-clear** (PRD REQ-3/4/5) -
  depend on the data model from REQ-1, out of scope here.
- **Pan/zoom, bespoke art, controller navigation** - explicitly P2/later per the
  PRD's "Out of scope" section.
- **A generic modal/widget-switcher framework** - see rationale above.

---

The real Unreal project stays under `app/` (gitignored, D-003) - this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
