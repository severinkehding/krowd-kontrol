# Issue #325: Data-driven level-select list on the main menu

## Summary

`UMainMenuWidget` (issue #324) now lists the shipped levels as selectable buttons and
loads whichever one the player picks, generated from data rather than a hardcoded
if-chain, per `docs/prd-main-menu.md` REQ-2. This migrates onto the shared "what
levels exist and in what order" authority issue #321 already established
(`LevelSequenceData.h`'s `FLevelSequenceRow` + `ULevelSequenceSubsystem`'s
`LevelSequenceTable`), rather than introducing a second, parallel data source, exactly
as issue #325's own body anticipated when it flagged the race with #321.

## What changed

- `ULevelSequenceSubsystem::GetShippedLevelMapNames()` (new): returns
  `LevelSequenceTable->GetRowNames()`, or an empty array if the table is unset. The
  only new level-list data source added by this issue.
- `UMainMenuLevelButtonWidget` (new): a small composite widget wrapping one `UButton`
  + label, re-broadcasting its click as a `FName`-payload dynamic multicast delegate
  (`OnLevelSelected`). Needed because `UButton::OnClicked` carries no parameters — a
  runtime-sized list of buttons otherwise has no way to tell one shared click handler
  which button fired. Mirrors `ULevelLifecycleSubsystem::FOnLevelBegin`'s existing
  `FName`-payload shape.
- `UMainMenuWidget`: builds one `UMainMenuLevelButtonWidget` per
  `GetShippedLevelMapNames()` entry into a new `LevelSelectBox` (`UVerticalBox`)
  between the title and the mastery-display anchor, inside the existing
  `EnsureWidgetTreeBuilt()`/`BuildWidgetTree()` guarded-build-once flow so population
  runs exactly once regardless of `NativeOnInitialized()`/`Initialize()` call order.
  `HandleLevelSelected(FName)` records the target in `LastSelectedLevelMapName` (a
  test-observability seam mirroring `ULevelSequenceSubsystem::
  LastAdvanceAttemptedMapName`) then calls `UGameplayStatics::OpenLevel()` directly —
  no confirmation screen — guarded by the same `World->IsGameWorld()` check
  `AdvanceToNextLevel()` already uses, so Automation's `CreateNewMap()` Editor Worlds
  never hang on a real level load.

Adding a future L4/L5 level is a content-only change (a new `LevelSequenceTable` row)
— zero code change to `MainMenuWidget`/`MainMenuLevelButtonWidget`.

## Known pre-existing gap (not fixed here, by design)

The real `LevelSequenceTable` **content** `DataTable` asset does not exist yet
(`app/Content` has no such asset). PR #335 (issue #321) shipped its NEXT LEVEL button
against this same gap and explicitly deferred populating the table as Alpha
content-authoring work, out of scope for either issue. This plan follows the same
precedent: automation tests inject their own in-code `UDataTable` (established pattern,
`KrowdKontrolLevelSequenceSubsystemTest.cpp`'s `BuildSequenceTable()`), but **in real
PIE today, with no content authored, the menu's level-select list will render with
zero buttons** until a follow-up populates `LevelSequenceTable`. Not silently worked
around with a hardcoded fallback map list — that would violate this issue's own
"data, not hardcoded" acceptance criterion.

## Scope limits (explicitly not building)

- Populating the real `LevelSequenceTable` content asset (see gap above).
- Lock/unlock gating on level buttons — every button is uniformly selectable, no
  per-level enabled/disabled state, per the issue's own AC and the PRD's Alpha stance.
- A confirmation/"are you sure" screen before loading a level — explicitly excluded
  by the AC.
- Keyboard navigation of the level list — PRD REQ-3 marks it P2; mouse-first only,
  matching #324's own precedent of leaving `SetInputMode()` unset.

## Files changed

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/LevelSequenceSubsystem.h` / `.cpp` | UPDATE | `GetShippedLevelMapNames()` |
| `app/Source/KrowdKontrol/MainMenuLevelButtonWidget.h` / `.cpp` | CREATE | `UMainMenuLevelButtonWidget` - composite per-level button |
| `app/Source/KrowdKontrol/MainMenuWidget.h` / `.cpp` | UPDATE | `LevelSelectBox`, `LevelSelectButtons`, `LastSelectedLevelMapName`, `PopulateLevelSelectButtons()`, `HandleLevelSelected()` |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolMainMenuLevelSelectTest.cpp` | CREATE | `KrowdKontrol.Unit.MainMenuLevelSelect` |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolLevelSequenceSubsystemTest.cpp` | UPDATE | Case (e): `GetShippedLevelMapNames()` direct coverage |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolReservedGameplayColoursTest.cpp` | UPDATE | Extended section 8 to audit the new level-button label colour |
| `app-source-tracked/Source/KrowdKontrol/...` (mirrors of every file above) | CREATE/UPDATE | D-009 carve-out - plain-text mirror so GitHub has real tracked source to hang a PR on |
| `app-changelog/issue-325.md` | CREATE | This file |

## Acceptance criteria

- [x] `UMainMenuWidget` lists the shipped levels (driven by `GetShippedLevelMapNames()`,
      not a hardcoded if-chain) as selectable buttons
- [x] A future L4/L5 `LevelSequenceTable` row requires zero code change to
      `MainMenuWidget`/`MainMenuLevelButtonWidget`
- [x] Activating a level's button loads that level directly via `OpenLevel()` — no
      confirmation screen
- [x] Every button is uniformly selectable — no lock/unlock gating logic
- [x] `KrowdKontrol.Unit.MainMenuLevelSelect` confirms button count for an injected
      table and that activating one targets the expected map
- [x] `ULevelSequenceSubsystem::GetShippedLevelMapNames()` is the only new level-list
      data source
- [x] `python harness/ci.py --quick` reports `GATE_OK`
- [ ] Real click-through in a live PIE session — **not automatable in this
      environment** (no ability-cast/click input primitive reaches real PIE input,
      same limitation `MainMenuWidgetTest.cpp`'s own header comment and PR #332's AC
      checklist already flagged for the Quit button) — flagged for manual sign-off.
- [x] PR body explicitly flags the empty-`LevelSequenceTable`-content gap (see above)
