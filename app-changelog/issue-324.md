# Issue #324: Main Menu Chrome Widget (Title, Quit Button, Mastery-Display Anchor)

## Summary

Builds `UMainMenuWidget` (`docs/prd-main-menu.md` REQ-3): a C++-authored UMG widget
providing the main menu's baseline chrome - a title, a Quit button, and an
explicitly-sized, empty, named anchor region reserved for a future Crowd Mastery
display widget. Adds a minimal `AMainMenuGameMode`/`AMainMenuPlayerController` pair
that creates and shows the widget on `BeginPlay`, and wires it onto a temporary test
map (`/Game/Maps/L_MainMenuTemp`) since issue #323 (the real `/Game/Maps/L_MainMenu`)
has not landed yet.

## AMainMenuGameMode / AMainMenuPlayerController split rationale

Deliberately NOT a reuse of `AKrowdKontrolGameMode`/`AKrowdKontrolPlayerController`.
That controller's `CreateHUDWidgets()` unconditionally creates and adds 7
gameplay-only HUD widgets (ability tray, energy meter, quest tracker, on-screen
prompt, briefing card, post-run summary, punishment debug menu) - none of which
belong on a menu screen. Branching that method on "is this the menu" would add
menu-awareness into gameplay code already exercised by 15+ existing tests, for no
benefit over a second, minimal controller class that costs two small files and
mirrors `AKrowdKontrolGameMode`'s own "one class, one job" precedent. `AMainMenuGameMode`
is assigned only via the temp map's own WorldSettings override - `Config/DefaultEngine.ini`'s
`GlobalDefaultGameMode` is untouched and stays `AKrowdKontrolGameMode`.

## UKismetSystemLibrary::QuitGame decision

`HandleQuitClicked()` calls `UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(),
EQuitPreference::Quit, false)` with no hand-rolled `GIsEditor`/`IsPlayInEditor()`
branching. Verified directly against the installed UE 5.8 engine source before
writing this:

- `Engine/Private/KismetSystemLibrary.cpp:650-671` - `QuitGame` resolves a
  `TargetPC` (falling back to `UGameplayStatics::GetPlayerController(WorldContextObject, 0)`
  if `SpecificPlayer` is null) and is a complete no-op if that resolves null.
- `Engine/Private/PlayerController.cpp:547-570` - `APlayerController::ConsoleCommand`
  additionally no-ops without a `Player` (`if (Player != nullptr) { ... } return
  TEXT("");`). A `CreateWidget<T>(World, ...)`-constructed widget's `GetOwningPlayer()`
  has no local player at all in a bare `FAutomationEditorCommonUtils::CreateNewMap()`
  world, so calling the real quit path there is doubly guaranteed safe - this is what
  lets `KrowdKontrolMainMenuWidgetTest.cpp` call `HandleQuitClicked()` directly without
  ever reaching a real "quit" console command against the Automation Testing
  Framework's own `UnrealEditor-Cmd.exe` process.
- `Engine/Private/GameEngine.cpp:1530` - "quit"/"exit" console command handling is
  engine-standard, context-aware behavior (ends PIE without touching the running
  Editor process; exits a packaged/`-game` process) - the same "Quit Game" Blueprint
  node used in effectively every published UE main-menu tutorial. No manual context
  detection is needed or was written.

## Mastery-display anchor

A `USizeBox` (`MasteryDisplayAnchor`, 320x96px), not a `UNamedSlot` - `UNamedSlot`'s
designed purpose is Widget-Blueprint-inheritance (a WBP subclass overrides a parent's
named slot), and this project builds every widget in pure C++ with no Widget
Blueprint assets, so that inheritance machinery has no consumer. `SetMasteryDisplayContent(UWidget*)`
fills the anchor via `SetContent()` with no relayout - a later PRD's widget is the
intended caller.

## Temporary map / hand-off to issue #323

Created `/Game/Maps/L_MainMenuTemp.umap` via a headless Unreal Python session
(`UnrealEditor-Cmd.exe -run=pythonscript`), consistent with issue #42/#43/#45/#185/#189's
precedent - the factory worktree cannot reach a live `unreal-mcp` connection
(`project_factory_worktree_no_unreal_mcp_network_path`). Used
`unreal.EditorLevelLibrary.new_level(...)` to create an empty level, then set that
level's `WorldSettings.DefaultGameMode` to `AMainMenuGameMode` via
`unreal.load_class(None, "/Script/KrowdKontrol.MainMenuGameMode")` +
`set_editor_property("default_game_mode", ...)`, and `save_current_level()`. Verified
in a second, independent headless process: loading `/Game/Maps/L_MainMenuTemp` and
reading back `WorldSettings.DefaultGameMode` resolves to `/Script/KrowdKontrol.MainMenuGameMode`.

This is a temporary map, superseded once issue #323 lands `/Game/Maps/L_MainMenu` -
move this GameMode override onto that map's WorldSettings at that point; no C++
change is needed since `AMainMenuGameMode`/`AMainMenuPlayerController` already exist
and work on any map.

## Manual PIE sign-off still required

- **Real click-through in a live PIE session** is not automatable in this
  environment - no ability-cast/click input primitive reaches real PIE input
  (`holdout_no_ability_cast_input_primitive`, `holdout_no_defeat_trigger_primitive`).
  `bShowMouseCursor = true` alone (reusing issue #262's exact mechanism, no
  `SetInputMode` added speculatively) may or may not route real mouse clicks to the
  `UButton`'s Slate hit-test - this codebase has zero prior `UButton` usage to prove
  either way. If a human PIE check shows clicks don't register, the documented fix is
  one line: `SetInputMode(FInputModeGameAndUI())` in
  `AMainMenuPlayerController::BeginPlay()` - not added here without evidence it's
  needed.
- **Real Quit behavior in a packaged/`-game` build** - same automatability gap;
  flagged for manual sign-off, not verified this run.

## Post-review fixes (self-fix pass)

Addressed all FIX-worthy findings from the multi-agent review of this PR:

- Added `KrowdKontrolMainMenuPlayerControllerTest.cpp` (`KrowdKontrol.Unit.MainMenuPlayerControllerBeginPlay`)
  - the PR's headline acceptance criterion (`AMainMenuPlayerController::BeginPlay()`
  constructing/showing the widget and enabling the cursor) was previously untested;
  now covered following `KrowdKontrolHUDWiringTest.cpp`'s `ULocalPlayer` + `DispatchBeginPlay()`
  precedent.
- Removed the vestigial `friend class FKrowdKontrolMainMenuGameModeTest;` declaration
  from `MainMenuPlayerController.h` (`MainMenuWidgetInstance` is already public - no
  friendship needed).
- Added `UMainMenuWidget::EnsureWidgetTreeBuilt()`'s idempotency-guard rationale
  comment, matching `UPostRunSummaryWidget`/`UOnScreenPromptWidget`/`UBriefingCardWidget`'s
  identical pattern (issue #66 precedent).
- Added test coverage for `EnsureWidgetTreeBuilt()`'s double-init guard and the
  bare-`NewObject()` degrade-safe path, and for `SetMasteryDisplayContent(nullptr)`'s
  documented no-op behavior, ported from `KrowdKontrolPostRunSummaryWidgetTest.cpp`/
  `KrowdKontrolBriefingCardWidgetTest.cpp`'s equivalent cases.
- Strengthened the title-text test to assert the exact string ("KROWD KONTROL"),
  not just non-empty.
- Reordered `KrowdKontrolReservedGameplayColoursTest.cpp`'s new "(8) Main menu widget
  audit" block to after the pre-existing "(7)" canary block, restoring the file's
  number-matches-reading-order convention.
- Dropped three "see the plan's ___ section" comment references
  (`MainMenuPlayerController.cpp`, `MainMenuWidget.h`, `KrowdKontrolMainMenuWidgetTest.cpp`)
  that pointed at a workflow-run artifact not tracked in this git repository; the
  surrounding prose already carries the same rationale inline.
- Not fixed (flagged to operator, both advisory-only and outside this PR's reach):
  `CLAUDE.md`'s Repo Layout tree omitting `app-source-tracked/`/`app-changelog/`, and
  its Conventions section staying "TBD" - `CLAUDE.md` is a protected path the factory
  cannot self-edit.

## Files changed

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/MainMenuWidget.h` / `.cpp` | CREATE | `UMainMenuWidget` - title, Quit button, mastery-display anchor |
| `app/Source/KrowdKontrol/MainMenuGameMode.h` / `.cpp` | CREATE | `AMainMenuGameMode` - points `PlayerControllerClass` at `AMainMenuPlayerController` |
| `app/Source/KrowdKontrol/MainMenuPlayerController.h` / `.cpp` | CREATE | `AMainMenuPlayerController` - shows `UMainMenuWidget` on `BeginPlay` |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolMainMenuWidgetTest.cpp` | CREATE | `KrowdKontrol.Unit.MainMenuWidget` |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolMainMenuGameModeTest.cpp` | CREATE | `KrowdKontrol.Unit.MainMenuGameModeSetsPlayerController` |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolMainMenuPlayerControllerTest.cpp` | CREATE | `KrowdKontrol.Unit.MainMenuPlayerControllerBeginPlay` (post-review fix) |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolReservedGameplayColoursTest.cpp` | UPDATE | Added "(8) Main menu widget audit" section |
| `app/Content/Maps/L_MainMenuTemp.umap` | CREATE | Binary map asset: empty level, WorldSettings -> GameMode Override = `AMainMenuGameMode` |
| `app-source-tracked/Source/KrowdKontrol/MainMenuWidget.h` / `.cpp` | CREATE (mirror) | Plain-text mirror per D-009 |
| `app-source-tracked/Source/KrowdKontrol/MainMenuGameMode.h` / `.cpp` | CREATE (mirror) | Plain-text mirror per D-009 |
| `app-source-tracked/Source/KrowdKontrol/MainMenuPlayerController.h` / `.cpp` | CREATE (mirror) | Plain-text mirror per D-009 |
| `app-source-tracked/Source/KrowdKontrol/Private/Tests/KrowdKontrolMainMenuWidgetTest.cpp` | CREATE (mirror) | Plain-text mirror per D-009 |
| `app-source-tracked/Source/KrowdKontrol/Private/Tests/KrowdKontrolMainMenuGameModeTest.cpp` | CREATE (mirror) | Plain-text mirror per D-009 |
| `app-source-tracked/Source/KrowdKontrol/Private/Tests/KrowdKontrolMainMenuPlayerControllerTest.cpp` | CREATE (mirror) | Plain-text mirror per D-009 (post-review fix) |
| `app-source-tracked/Source/KrowdKontrol/Private/Tests/KrowdKontrolReservedGameplayColoursTest.cpp` | UPDATE (mirror) | Plain-text mirror per D-009 |

Not mirrored: `L_MainMenuTemp.umap` itself is a binary asset, excluded from
`app-source-tracked/` per CLAUDE.md's mirror rules (never `.uasset`/`.umap`/anything
under `Content/`).

## Acceptance criteria

- [x] `UMainMenuWidget` builds its tree in C++, matching `UPostRunSummaryWidget`/`UBriefingCardWidget`'s pattern
- [x] A title text element displays the game name ("KROWD KONTROL")
- [x] A Quit button is present; `HandleQuitClicked()` calls `UKismetSystemLibrary::QuitGame(...)`, verified against engine source
- [x] An explicitly-sized, named, empty `USizeBox` region is reserved for the future mastery display, fillable via `SetMasteryDisplayContent()`
- [x] No reserved gameplay colour appears anywhere in this widget's chrome - enforced by the extended `KrowdKontrolReservedGameplayColoursTest`
- [x] `AMainMenuGameMode`/`AMainMenuPlayerController` display the widget on `BeginPlay`, reusing the existing `bShowMouseCursor = true` mechanism, no new input-handling code invented - now directly verified by `KrowdKontrol.Unit.MainMenuPlayerControllerBeginPlay` (post-review fix)
- [x] Automation tests confirm construction, title/Quit-button presence, and that activating Quit safely reaches the correct engine call
- [x] `python harness/ci.py --quick` reports `GATE_OK mode=quick`
- [x] `app-source-tracked/` mirror + this changelog written
- [x] Temp-map content step completed via headless pythonscript, verified in a second independent process; hand-off to issue #323 documented above
- [ ] Real click-through in a live PIE session and real Quit in a packaged build - not automatable in this environment, flagged above for manual sign-off

## Validation Evidence

See `implementation.md` in the workflow run artifacts for the full validation record.
