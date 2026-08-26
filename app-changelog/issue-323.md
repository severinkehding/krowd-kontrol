# Issue #323: Add dedicated main menu map and set it as the project's default game map

## Summary

Foundational piece of `docs/prd-main-menu.md`'s REQ-1: creates the real front-end map
asset `/Game/Maps/L_MainMenu`, wires it to the already-built
`AMainMenuGameMode`/`AMainMenuPlayerController`/`UMainMenuWidget` trio (issue #324 /
PR #332), and points `GameDefaultMap` at it in `DefaultEngine.ini` so `-game`/packaged
launches land in the menu with no editor choreography. `EditorStartupMap` stays
`/Game/Maps/L_Level01`, so the normal "open a gameplay map, hit PIE" workflow is
untouched. The temporary map issue #324 created as a stand-in
(`L_MainMenuTemp`) is retired per its own documented hand-off note.

## Map authoring approach

Authored `/Game/Maps/L_MainMenu.umap` via a headless Unreal Python session
(`UnrealEditor-Cmd.exe -run=pythonscript`), consistent with issue #42/#43/#45/#185/#189/#324's
precedent - the factory worktree cannot reach a live `unreal-mcp` connection
(`project_factory_worktree_no_unreal_mcp_network_path`). Used
`unreal.LevelEditorSubsystem.new_level(...)` (the non-deprecated subsystem API, unlike
issue #324's `unreal.EditorLevelLibrary`) to create an empty level, set that level's
`WorldSettings.DefaultGameMode` to `AMainMenuGameMode` via
`unreal.load_class(None, "/Script/KrowdKontrol.MainMenuGameMode")` +
`set_editor_property("default_game_mode", ...)`, and `save_current_level()`. Verified
in a second, independent headless process: loading `/Game/Maps/L_MainMenu` and reading
back `WorldSettings.DefaultGameMode` resolves to `/Script/KrowdKontrol.MainMenuGameMode`.

## Why AMainMenuGameMode is reused, not a new class

`app-changelog/issue-324.md`'s "Temporary map / hand-off to issue #323" section is an
explicit, on-the-record instruction: move the existing GameMode override onto the real
map's WorldSettings, no new C++ needed, since `AMainMenuGameMode`/`AMainMenuPlayerController`
already work on any map. This is followed as-is; a pre-existing `web-research.md`
artifact in this run's directory recommended a Level Blueprint `BeginPlay` hook
instead, but that research predates #324's merge and is superseded by the codebase's
own, more current hand-off note.

## Why the temp map is deleted, and a `delete_asset` quirk found along the way

`/Game/Maps/L_MainMenuTemp.umap` is superseded per issue #324's own hand-off note.
Before deleting, `grep -rn "MainMenuTemp" app-source-tracked/ app/Source/` surfaced one
real hit the plan's own pre-check had missed: `KrowdKontrolMainMenuGameModeTest.cpp`'s
`FKrowdKontrolMainMenuGameModeLevelOverrideTest` loaded `/Game/Maps/L_MainMenuTemp` by
path and asserted its GameMode override. Retargeted that test (and its
`app-source-tracked/` mirror) to `/Game/Maps/L_MainMenu` instead - the override moved
with the map, so the test's assertion is still meaningful, just against the real map
now. That mirror was previously out of sync with `app/` in an unrelated way too (it
was missing this whole second test case); syncing it as part of this edit closes that
gap.

Deletion itself needed an extra step beyond the plan: `unreal.EditorAssetSubsystem.delete_asset("/Game/Maps/L_MainMenuTemp")`
returned `True` (log showed `LogUObjectGlobals: Force Deleting 1 Package(s)`) but the
`.umap` file remained on disk and `does_asset_exist` kept returning `True` in a
follow-up independent headless process - even immediately after the delete call within
the same process. No referencers were found (`AssetRegistry.get_referencers` returned
`[]`), ruling out an in-use/reference block; this looks like `ForceDeleteObjects`'s
disk-removal step not completing in this unattended `-run=pythonscript` commandlet
context specifically. Removed the file directly from the filesystem instead
(`app/Content/Maps/L_MainMenuTemp.umap`), then re-verified with a fresh headless
process that `does_asset_exist("/Game/Maps/L_MainMenuTemp")` returns `False` - it does.

## Files changed

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Content/Maps/L_MainMenu.umap` | CREATE | Binary map asset: empty level, WorldSettings -> GameMode override = `AMainMenuGameMode` |
| `app/Content/Maps/L_MainMenuTemp.umap` | DELETE | Superseded per issue #324's hand-off note |
| `app/Config/DefaultEngine.ini` | UPDATE | `GameDefaultMap` -> `/Game/Maps/L_MainMenu` (was `/Engine/Maps/Templates/OpenWorld`); `EditorStartupMap` unchanged |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolMainMenuMapTest.cpp` | CREATE | `KrowdKontrol.Unit.MainMenuMapWiring` - ini value, map load, GameMode override, no gameplay actors |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolMainMenuGameModeTest.cpp` | UPDATE | Retargeted `FKrowdKontrolMainMenuGameModeLevelOverrideTest` from `L_MainMenuTemp` to `L_MainMenu` (deviation - see above) |
| `app-source-tracked/Source/KrowdKontrol/Private/Tests/KrowdKontrolMainMenuMapTest.cpp` | CREATE (mirror) | Plain-text mirror per D-009 |
| `app-source-tracked/Source/KrowdKontrol/Private/Tests/KrowdKontrolMainMenuGameModeTest.cpp` | UPDATE (mirror) | Synced to `app/`'s retargeted test; also closed a pre-existing drift gap (mirror was missing the level-override test entirely) |
| `app-changelog/issue-323.md` | CREATE | This record |

Not mirrored: `L_MainMenu.umap` itself is a binary asset, excluded from
`app-source-tracked/` per CLAUDE.md's mirror rules.

## Acceptance criteria

- [x] `/Game/Maps/L_MainMenu` exists, contains no `AEnemyBase`/`ATargetZone`/player-controlled-pawn actors (issue AC #1) - enforced by `KrowdKontrol.Unit.MainMenuMapWiring`
- [x] `app/Config/DefaultEngine.ini`'s `GameDefaultMap` = `/Game/Maps/L_MainMenu` (issue AC #2)
- [x] `EditorStartupMap` unchanged at `/Game/Maps/L_Level01` (issue AC #3) - asserted in-diff by the new automation test; manual PIE click-through still flagged below
- [x] `L_MainMenu`'s WorldSettings carries the `AMainMenuGameMode` override (issue AC #4)
- [x] `L_MainMenuTemp.umap` deleted (superseded)
- [x] `python harness/ci.py --quick` reports `GATE_OK mode=quick` (UNIT_PASSED tests=117, PIE_PASSED tests=5)
- [x] `app-source-tracked/` mirror + this changelog written
- [ ] Real click-through / Quit-button behavior in a live PIE or packaged session - not automatable in this environment (no ability-cast/click input primitive reaches real PIE input - `holdout_no_ability_cast_input_primitive`, `holdout_no_defeat_trigger_primitive`); flagged for manual operator sign-off, matching issue #324's own precedent.

## Post-review fixes (PR #334 self-fix pass)

Two comment-accuracy fixes from review, both comment-only (no assertion logic changed):

- `KrowdKontrolMainMenuMapTest.cpp`'s file-header comment previously implied the test
  proves `AMainMenuPlayerController::BeginPlay()` shows `UMainMenuWidget` on launch;
  reworded to state that's the *design intent* behind checking the GameMode class,
  not something this test exercises (no `BeginPlay` is dispatched here - see the
  unchecked AC row above).
- `KrowdKontrolMainMenuGameModeTest.cpp:49`'s comment claiming to mirror
  `KrowdKontrolGameModeTest.cpp`'s `FKrowdKontrolGameModeLevelOverrideTest` "exactly"
  reworded to describe the actual difference (strict `TestEqual` vs. that test's
  "no override, or expected class" fallback).

A third review finding (assert `FAutomationEditorCommonUtils::LoadMap`'s return value
via `TestTrue`) was attempted and reverted: `LoadMap` returns `void` in this engine's
`AutomationEditorCommon.h` (UE 5.8), not `bool` as the finding assumed - confirmed by
a UBT compile failure (`C2665: no overloaded function could convert all the argument
types`) before reverting. The underlying pattern (discarding whatever `LoadMap` does
return) is unchanged from the original PR and is shared by 8+ other test files in this
suite; a real fix would need a different mechanism than the one proposed (e.g.
comparing `World->GetOutermost()->GetName()` against the requested map path) and is
better scoped as its own follow-up covering all call sites, not patched two-at-a-time.
Re-ran `python harness/ci.py --quick` after this pass: `GATE_OK mode=quick`
(`UNIT_PASSED tests=117`, `PIE_PASSED tests=5`).

## Validation Evidence

See `implementation.md` in the workflow run artifacts for the full validation record.
