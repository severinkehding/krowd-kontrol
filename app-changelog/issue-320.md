# Issue #320: RERUN LEVEL button on the post-run summary screen

## Summary

Adds a `[RERUN LEVEL]` button to `UPostRunSummaryWidget` (the post-run clear screen),
positioned below the clear-time/best-time/Crowd-Mastery info block and above the
`[NEXT LEVEL]` button (issue #321). Activating it (mouse click or keyboard Enter/Space)
reloads the current level via the existing, already-proven shared reload path,
`AKrowdKontrolPlayerController::RequestLevelRestart()` (the same function
`HandleLevelFailed()`'s defeat-restart and `HandleNextLevelClicked()`'s final-level
branch already call). No new reload logic was written - this issue only wires a third
caller onto the existing one.

## Concurrent work with issue #321

`app/` is a local symlink to one real, shared Unreal project on the Windows host - it
is not scoped to this git worktree, and other factory tasks edit the same files
through their own worktrees' symlinks. At implementation time, **issue #321 ("NEXT
LEVEL button", same PRD REQ-3) was already implemented live in `app/`**, including
making `AKrowdKontrolPlayerController::RequestLevelRestart()` public and adding
`NextLevelButton`/`NextLevelButtonLabel`/`HandleNextLevelClicked()` to this same
widget. This issue's changes were added alongside #321's code without touching,
reordering, or removing any of it.

Because the `app-source-tracked/` mirror is a byte-for-byte copy of live `app/` state
(not a diff), mirroring this issue's changes into `PostRunSummaryWidget.h`/`.cpp`
necessarily also carries #321's already-landed, unrelated changes forward into the
mirror for the first time (the mirror was previously stale relative to `app/` for
those files). This is expected given the shared-symlink architecture and is not part
of this issue's own diff - reviewers should attribute `NextLevelButton`/
`HandleNextLevelClicked()`/`ResolvedNextLevelMapName`/`GetNextLevelButtonDisplayText()`
to #321, not this change. This issue touched zero lines in
`KrowdKontrolPlayerController.h`/`.cpp` - `RequestLevelRestart()` was already public
when this work started.

## SetInputMode addition

`HandleLevelClear()` now calls `OwningController->SetInputMode(FInputModeGameAndUI())`
with `SetWidgetToFocus(RerunButton->TakeWidget())`, immediately after the existing
`AddToViewport()` call. This is scoped narrowly to only fire when the clear screen
appears (i.e., only once gameplay is already over for this run) - not added globally
in `BeginPlay()` - so it cannot regress in-level WASD/ability input routing even in
the worst case. `SetInputMode()` is a safe no-op with no `GameViewportClient`/
`LocalPlayer` (verified against UE 5.8 engine source, `PlayerController.cpp:6454-6468`),
which is exactly the case in this project's headless Automation test worlds.

This is the first use of `SetInputMode` in this codebase (issue #324's Quit button
flagged the same open question and deliberately left it unadded, pending evidence).
This issue's AC explicitly requires both mouse click and keyboard activation to work,
which is a stronger requirement than #324's Quit button had, so the call is added here
with the reasoning documented inline.

## Manual PIE sign-off still required

- **Real mouse-click-through and keyboard Enter/Space activation in a live PIE
  session** are not automatable in this environment - no ability-cast/click input
  primitive reaches real PIE input (`holdout_no_ability_cast_input_primitive`). Flagged
  for manual operator sign-off, same as issues #262/#324 before it.

## Files changed

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/PostRunSummaryWidget.h` | UPDATE | `RerunButton`/`RerunButtonLabel` members, `HandleRerunClicked()` declaration, `bHasWarnedMissingOwningControllerOnRerun` guard, new test friend class (also carries #321's already-landed members forward, see "Concurrent work" above) |
| `app/Source/KrowdKontrol/PostRunSummaryWidget.cpp` | UPDATE | `RerunButton` construction in `BuildWidgetTree()`, `HandleRerunClicked()` implementation, `SetInputMode`/focus call in `HandleLevelClear()` (also carries #321's already-landed code forward, see "Concurrent work" above) |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolPostRunSummaryRerunButtonTest.cpp` | CREATE | `KrowdKontrol.Unit.PostRunSummaryRerunButton` - real click wiring (incl. the real `HandleLevelClear()`/`SetInputMode` path), no-owning-player degrade with a proven one-shot warning guard, and `RerunButton`/`NextLevelButton` layout-order assertion |
| `app-source-tracked/Source/KrowdKontrol/PostRunSummaryWidget.h` | UPDATE (mirror) | Plain-text mirror per D-009 |
| `app-source-tracked/Source/KrowdKontrol/PostRunSummaryWidget.cpp` | UPDATE (mirror) | Plain-text mirror per D-009 |
| `app-source-tracked/Source/KrowdKontrol/Private/Tests/KrowdKontrolPostRunSummaryRerunButtonTest.cpp` | CREATE (mirror) | Plain-text mirror per D-009 |

## Acceptance criteria

- [x] A `[RERUN LEVEL]` button is added to `UPostRunSummaryWidget`, positioned below the information block and above `NextLevelButton`
- [x] Activating the button reloads the current level via `AKrowdKontrolPlayerController::RequestLevelRestart()` - no second reload implementation written
- [x] The button responds to mouse click (`OnClicked` binding + `SetInputMode(FInputModeGameAndUI())`) and keyboard (initial focus via `SetWidgetToFocus`)
- [x] `KrowdKontrol.Unit.PostRunSummaryRerunButton` asserts activating the button flips `WasRestartRequested()` true via the real click handler
- [x] `KrowdKontrol.Unit.PostRunSummaryRerunButton` also exercises the real `HandleLevelClear()` path (not just `HandleRerunClicked()` directly) with a real owning player, so the `SetInputMode`/focus-on-`RerunButton` wiring actually executes under test
- [x] `KrowdKontrol.Unit.PostRunSummaryRerunButton` proves `bHasWarnedMissingOwningControllerOnRerun` is genuinely one-shot (two calls, one log line, via `AddExpectedError(..., 1, false)`) and that `RerunButton` is positioned above `NextLevelButton` in the layout
- [x] `app/` and `app-source-tracked/` copies of all touched files are byte-identical (`diff` clean)
- [x] `app-changelog/issue-320.md` written, documenting the concurrent-work interaction with #321
- [ ] Real PIE mouse-click-through and keyboard Enter/Space activation - not automatable in this environment, flagged above for manual sign-off

## Validation Evidence

See `implementation.md` in the workflow run artifacts for the full validation record.
