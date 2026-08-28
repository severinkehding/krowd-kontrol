# Issue #356: Pre-Level Briefing Card Missing on Menu-Entry (Regression, 2026-08-28 Playtest)

## Summary

Adds `KrowdKontrol.PIE.MenuEntryBriefing`, a real-PIE regression test that opens
`/Game/Maps/L_MainMenu` (a genuine disk load, not the currently-open-editor-level PIE
path), drives the real `UMainMenuWidget::HandleLevelSelected()` click handler into
`L_Level01`, and asserts `AKrowdKontrolPlayerController::BriefingCardWidgetInstance`
is visible after the real `UGameplayStatics::OpenLevel()` travel and the destination
world's `OnLevelBegin` fire. The test injects an in-code `UDataTable` into the
destination world's `ULevelBriefingSubsystem::LevelBriefingTable` (same
`BuildBriefingTable()` convention `KrowdKontrolLevelBriefingSubsystemTest.cpp` already
uses), so it exercises and can actually pass the real fixed path today, independent of
the still-open `LevelBriefingTable` content-authoring gap described below.

## Why no code fix

The investigation (see the workflow's `investigation.md`/`plan.md` artifacts) checked
both leads the issue named against the real UE 5.8 engine source and ruled both out:

- The `#235`-style buffered-retry race between `ULevelBriefingSubsystem::HandleLevelBegin()`
  and `AKrowdKontrolPlayerController::CreateHUDWidgets()`/`RetryPendingBriefing()` is
  identical in both entry paths - `UWorld::BeginPlay()` always calls
  `WorldSubsystem->OnWorldBeginPlay()` before `GameMode->StartPlay()` (actor
  `BeginPlay()`), and `UEngine::LoadMap()` always spawns the player controller before
  `World::BeginPlay()`, regardless of a fresh PIE load vs. an `OpenLevel()`-triggered
  travel. The existing buffer/retry mechanism already covers this.
- PR #309's `AnyKey`-dismiss input change: `AMainMenuPlayerController::BeginPlay()`
  never calls `SetInputMode()` (deliberate, per `app-changelog/issue-324.md`), so no
  stale input-mode state carries across the travel that could suppress the dismiss
  binding or the card itself.

The leading hypothesis is a content/asset gap, not a code defect:
`ULevelBriefingSubsystem::LevelBriefingTable` is an `EditDefaultsOnly` reference set
entirely outside `Source/` (Editor Class Defaults / a content DataTable), and the real
code path this issue reports as broken (menu click -> real `OpenLevel()` -> real
`OnLevelBegin` in the destination world) was never actually exercised in a live PIE
session before PR #346 merged (that PR's own body flags real click-through as
untested). This cannot be confirmed from this WSL worktree - no live Unreal MCP
connection is reachable here (see `.factory/decisions.md` and repo memory), and
`LevelBriefingTable`'s value isn't represented in any file under `Source/`.

## What this PR does and does not do

- **Does**: add the regression test plus the one friend-declaration wiring change
  (`MainMenuWidget.h`) needed for the test to call `HandleLevelSelected(FName)`
  directly, mirroring every other `KrowdKontrol.Unit.MainMenu*` test's identical
  workaround for the lack of a UMG click-simulation primitive.
- **Does**: inject an in-code `LevelBriefingTable` via
  `FWorldDelegates::OnPostWorldInitialization` so the test proves the real
  menu-entry -> `OpenLevel()` -> briefing-visible path works today, and adds
  `AddExpectedError` coverage for all three of `HandleLevelBegin()`'s diagnostic
  branches so a red run tells a future reader which one fired instead of an
  undifferentiated timeout.
- **Does not**: touch `LevelBriefingSubsystem.cpp`/`.h` or any other production code -
  no code defect was found. Live-verified via `harness/run_ue_automation.sh`
  (invokes `UnrealEditor-Cmd.exe` directly, reachable from this WSL worktree unlike
  the MCP network path): with the in-code DataTable injection in place,
  `KrowdKontrol.PIE.MenuEntryBriefing` passes (`UE_AUTOMATION_RESULT passed=7
  total=7`), and the full gate reports `GATE_OK mode=full` (unit suite unchanged,
  `UNIT_PASSED tests=127`).
- **Does not**: edit or author the real `LevelBriefingTable` DataTable content asset
  itself (binary `.uasset`, not reachable/appropriate from this workflow). The test's
  in-code injected table only exists for the duration of this one test run and never
  touches the CDO - actual players still see no pre-level briefing on any map until an
  operator sets the real asset in the Editor; this PR's `pie` gate turning green no
  longer depends on that, but the underlying content gap is unchanged and still needs
  that Editor/operator action, not a further code PR.

## Files changed

- `Private/Tests/KrowdKontrolPIEMenuEntryBriefingTest.cpp` (NEW) - the regression test.
- `MainMenuWidget.h` - added `friend class FKrowdKontrolPIEMenuEntryBriefingTest;`.
