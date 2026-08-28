# Issue #367: Build hand-authored Level 4 (Alpha, high difficulty tier, fourth of five)

Authors the fourth of krowd-kontrol's 5 hand-authored Alpha levels
(`/Game/Maps/L_Level04.umap`) on top of the already-merged `ARoomActor`/
`ADoorConnectorActor` foundation (issue #39), `L_Level03` (issue #45), the playable-spawn
pattern (issue #185), and the compressed-spacing precedent (issue #189). Level 4 is a
6-room linear chain - strictly more rooms and strictly more enemies than Level 3's 5
rooms / 10 enemies - carrying 12 static placeholder-density enemies (2 per room,
continuing Level 3's own live sliding-window type-cycling pattern), so the difficulty
ramp Levels 2-5 build on (docs/prd-levels-4-5.md REQ-1) now has a fourth real data
point, and registers it as the new final row of `DT_LevelSequenceTable` (Level 3's row
updated to point at it). Structural scope only: room count, door connectivity, and
target-zone placement - no narrative content, no new mechanics, no briefing/unlock UI.

The level was authored via a headless Unreal Python session
(`UnrealEditor-Cmd.exe -run=pythonscript`), not the MCP plugin's `ProgrammaticToolset`
sandbox, because `ARoomActor::AddTargetZone()` is a genuine `UFUNCTION` call the sandbox
cannot invoke (no `unreal` module, no method-invocation tool) - same technique issues
#42/#43/#45/#185/#189 used. The factory worktree's live `unreal-mcp` connection was not
attempted first given the recurring `project_factory_worktree_no_unreal_mcp_network_path`
limitation; the headless commandlet route was used directly, consistent with every
prior level-authoring issue.

## Approach

1. Read the live `L_Level03` map first (not trusting the plan's assumed 2500cm spacing
   or its assumed alternating-pairs enemy pattern) via a headless Python dump: confirmed
   5 rooms at X=0/2500/5000/7500/10000 (2500cm spacing, matching the plan's assumption),
   4 doors at the midpoints, 1 `ALevelLightingRigActor` at (5000,0,500), one
   `PlayerStart`+pawn at (0,0,75), and 10 enemies. The enemy pattern turned out to be a
   **sliding-window** cycle (Room N places types at cycle positions `[N, N+1] mod 4` of
   Runner->Trooper->Bomber->Sniper), not the plan's assumed "alternating pairs"
   (Room1=Runner+Trooper, Room2=Bomber+Sniper, repeat) - this plan-vs-live divergence
   was exactly what Task 1's live-verification step exists to catch, per issue #45's own
   precedent.
2. Designed Level 4 as a superset of that live pattern: 6 rooms at
   X=0/2500/5000/7500/10000/12500 (same 2500cm spacing), 5 `ADoorConnectorActor`s at the
   midpoints (1250/3750/6250/8750/11250), 1 `ALevelLightingRigActor` at (6250,0,500) -
   the chain's midpoint - one `PlayerStart`+`AFlatCamera3DPrototypePawn` at (0,0,75)
   (the entrance room), and 12 enemies (2 per room, continuing the live sliding-window
   pattern one step further: Room6 wraps to Trooper+Bomber). Resulting type totals: 3
   Runner, 4 Trooper, 3 Bomber, 2 Sniper - all four `EEnemyType` values present,
   satisfying the AC without requiring an even split.
3. Authored via `unreal.LevelEditorSubsystem.new_level()` +
   `EditorActorSubsystem.spawn_actor_from_class()` (spawning directly at each target
   location, so the documented `set_actor_transform()` no-op gotcha from issue #189
   didn't apply here - no post-spawn move was needed), `AddTargetZone()` once per
   distinct enemy type per room, `RecomputeConnectorGeometry()` per door, and
   `save_current_level()`.
4. Re-queried the saved map in a second, independent headless process (confirms actual
   on-disk persistence, not just in-memory state) - confirmed exactly 6 rooms, 5 doors,
   12 enemies, 1 lighting rig, 1 `PlayerStart`, 1 pawn, and each room's target zones
   matching its placed enemy types.
5. Updated `DT_LevelSequenceTable`: read the live rows first (confirmed unchanged from
   `create_level_sequence_table.py`'s original literal - `L_Level01->L_Level02`,
   `L_Level02->L_Level03`, `L_Level03->None`), then applied
   `fill_data_table_from_csv_string` with the full 4-row set
   (`L_Level03->L_Level04`, `L_Level04->` empty), saved, and read back in a second
   independent process to confirm exactly 4 rows with no duplicates and the correct
   values.
6. Added `KrowdKontrol.Unit.Level04Structure`
   (`KrowdKontrolLevel04Test.cpp`, mirroring `KrowdKontrolLevel03Test.cpp` exactly, plus
   one additional explicit "all four enemy types present" assertion), which loads both
   `L_Level03` and `L_Level04` in one test function and asserts the room-count/
   enemy-count comparison directly against the live `L_Level03` asset, not a hardcoded
   magic number, so it can't silently drift if Level 3 is ever revised.
7. Extended `KrowdKontrolGameplayLevelPlayableSpawnTest.cpp`'s `GameplayLevelMapPaths`
   array with `/Game/Maps/L_Level04` so its existing "every gameplay level has a
   playable spawn" coverage extends to the new map.
8. Confirmed (did not re-implement) the 4->Fear unlock: `HandleLevelBegin("L_Level04")`
   is already asserted to unlock Fear at
   `KrowdKontrolAbilityUnlockLevelSubsystemTest.cpp:96-97` (added under issue #217).
   That test is purely string-based (`ParseLevelIndexFromMapName`) and never depended
   on `/Game/Maps/L_Level04` existing, so this AC was already covered end-to-end before
   this issue - re-ran it here only to confirm no regression.

## Files changed (all under `app/`, gitignored per D-003 - this is the tracked-repo
record of that change, see the closing note below)

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Content/Maps/L_Level04.umap` | CREATE | Binary map asset: 6 `ARoomActor` instances in a linear chain (X=0/2500/5000/7500/10000/12500), 5 `ADoorConnectorActor` instances wiring adjacent rooms, two target zones per room via `AddTargetZone()` (all 4 `EEnemyType` values present across the level), 12 static placeholder-density enemy actors (3x `ARunnerEnemy`, 4x `ATrooperEnemy`, 3x `ABomberEnemy`, 2x `ASniperEnemy`), one `ALevelLightingRigActor`, one `PlayerStart` + one `AFlatCamera3DPrototypePawn` at the entrance room (0,0,75) - no live AI/GameMode wiring |
| `app/Content/Data/DT_LevelSequenceTable.uasset` | UPDATE | Added `L_Level04` row (`NextLevelMapName=""`), updated `L_Level03` row (`NextLevelMapName="L_Level04"`) - confirmed 4 rows, no duplicates, via read-back in a second independent process |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolLevel04Test.cpp` | CREATE | `KrowdKontrol.Unit.Level04Structure` - loads `L_Level03` then `L_Level04`, asserts Level04's room count (6, strictly > Level03's live count) and door count (5, each connecting two distinct rooms), walks the door adjacency graph (BFS) to confirm all 6 rooms are reachable, asserts every door has a visible marker, asserts every room has >=1 target zone and >=1 enemy placeholder with matching types, asserts the banking zones self-heal, asserts all four `EEnemyType` values are present somewhere in the level, and asserts Level04's total enemy count (12) is strictly greater than Level03's live count |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolGameplayLevelPlayableSpawnTest.cpp` | UPDATE | Appended `TEXT("/Game/Maps/L_Level04")` to `GameplayLevelMapPaths` |
| `app-source-tracked/Source/KrowdKontrol/Private/Tests/KrowdKontrolLevel04Test.cpp` | CREATE | Plain-text mirror of the new test file, per Hard Invariant 8's D-009 carve-out, so GitHub has a real diff to open a PR against |
| `app-source-tracked/Source/KrowdKontrol/Private/Tests/KrowdKontrolGameplayLevelPlayableSpawnTest.cpp` | UPDATE | Plain-text mirror of the updated test file |

Not mirrored here: `L_Level04.umap` and `DT_LevelSequenceTable.uasset` are binary
assets, excluded from `app-source-tracked/` per CLAUDE.md's mirror rules (never
`.uasset`/`.umap`/anything under `Content/`) - only the new/changed `.cpp` test files
are mirrored.

## Acceptance criteria

- [x] **`/Game/Maps/L_Level04` exists on the `ARoomActor`/`ADoorConnectorActor`
      foundation** - confirmed via two independent headless read-backs.
- [x] **Room count and total enemy count strictly greater than Level 3's (5 rooms /
      10 enemies)** - 6 rooms / 12 enemies, asserted dynamically against the live
      `L_Level03` asset by `KrowdKontrol.Unit.Level04Structure`.
- [x] **All four core enemy types appear in the level** - 3 Runner, 4 Trooper,
      3 Bomber, 2 Sniper (asserted explicitly, not just implied by the ramp).
- [x] **Every room's target zones are type-keyed matching its placed enemy types** -
      asserted per-room, checked against the actual distinct enemy types placed in each
      room via nearest-room-by-distance matching.
- [x] **Level 4 registered in `DT_LevelSequenceTable` immediately after Level 3, zero
      additional code changes needed for menu/next-level to pick it up** - confirmed via
      read-back: `L_Level03->L_Level04`, `L_Level04->` empty (final level).
- [x] **New structure test in the `KrowdKontrolLevel0NTest` lineage asserting room/
      zone/enemy-density counts and strict-excess vs. Level 3** -
      `KrowdKontrol.Unit.Level04Structure` passes, 1/1.
- [x] **Confirmed (not re-implemented) that the 4->Fear mapping fires** -
      `KrowdKontrol.Unit.AbilityUnlockLevelSubsystem` already asserts this at
      `KrowdKontrolAbilityUnlockLevelSubsystemTest.cpp:96-97`; re-ran, still passes.
- [x] **No briefing/unlock-announcement UI built or duplicated** - untouched, per
      issue #367's own Notes and issue #356's separate ownership of that regression.
- [x] **No new enemy/ability types or mechanics introduced** - only existing
      `ARunnerEnemy`/`ATrooperEnemy`/`ABomberEnemy`/`ASniperEnemy` classes placed.
- [x] **No boss encounter pre-built** - not in scope; not part of this plan.
- [x] **No regressions in existing `KrowdKontrol.Unit.*`/`KrowdKontrol.Smoke.*`/
      `KrowdKontrol.PIE.*` tests; `python harness/ci.py` reports `GATE_OK
      mode=full`** - see Validation below.

## Validation

```
$ bash harness/run_ue_automation.sh "KrowdKontrol.Unit.Level04Structure"
UE_BUILD_START KrowdKontrolEditor Win64 Development
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK

$ bash harness/run_ue_automation.sh "KrowdKontrol.Unit.GameplayLevelsHavePlayableSpawn"
UE_BUILD_START KrowdKontrolEditor Win64 Development
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK

$ bash harness/run_ue_automation.sh "KrowdKontrol.Unit.AbilityUnlockLevelSubsystem"
UE_BUILD_START KrowdKontrolEditor Win64 Development
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
```

Full-gate `python harness/ci.py` output recorded in `implementation.md` for this
workflow run.

MISSION.md Hard Invariants reviewed against the diff: no-kill rule, 5-colour lock,
5-ability roster, and engine/dimensionality lock are all N/A (no such logic touched);
4-type enemy roster (#5) is respected - only existing enemy classes
(`ARunnerEnemy`/`ATrooperEnemy`/`ABomberEnemy`/`ASniperEnemy`) are placed, no new enemy
type introduced; `app/` not tracked in git (#8) - all new files stayed under the
untracked `app/` symlink, mirrored here only as a plain-text source copy per D-009.

### Independently-checkable evidence for the .umap creation

`L_Level04.umap` is a binary asset and, per D-009, will never appear in this repo's
tracked diff - no text diff can confirm it. What *is* independently checkable, by
anyone re-running the command below (not just reading this prose), is the automation
test's pass/fail state, which only flips to pass once the map actually contains the
6-room/12-enemy structure:

```
$ bash harness/run_ue_automation.sh "KrowdKontrol.Unit.Level04Structure"
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
```

Before this fix, `/Game/Maps/L_Level04` did not exist at all, so this test (and the
`.cpp` file itself) did not exist either - there is no "before" state to run the test
against, unlike a content-edit issue.

## Notes

- This is the fourth of five hand-authored Alpha levels; docs/prd-levels-4-5.md REQ-1
  (Level 4) is what this issue builds. REQ-2 (Level 5) and REQ-3 (cross-level
  ramp-tuning pass) are explicitly out of scope, per the plan's "NOT Building" section -
  Level 5 doesn't exist yet, so a tuning pass has nothing to compare it against.
- `L_Level03`'s live enemy-placement pattern turned out to be a sliding-window cycle,
  not the alternating-pairs pattern the investigation plan assumed - caught by Task 1's
  live re-verification step, exactly the kind of plan-vs-live divergence that step
  exists to catch (see issue #45's own 2500cm-vs-3000cm precedent). Level 4 continues
  the real live pattern rather than the plan's assumed one.
- `AbilityUnlockComponent.cpp`'s `GetLevelToAbilityMap()` already maps level index 4 ->
  `EAbilitySlot::Fear`, and `AbilityUnlockLevelSubsystem`'s `ParseLevelIndexFromMapName`
  is fully generic - Level 4 unlocking Fear on entry requires zero code changes, purely
  a consequence of the map being named `L_Level04`. This was already covered by
  `KrowdKontrolAbilityUnlockLevelSubsystemTest.cpp` (issue #217), confirmed still
  passing, not re-implemented.
- `LevelBriefingData` DataTable row authoring (content assets outside `app/Source/`) is
  explicitly out of scope per the plan's "NOT Building" section and issue #356's
  separate ownership of the pre-level-briefing regression - flagged as a possible
  follow-up, not required by issue #367's stated acceptance criteria.
- Level 5 is not built here - this issue is Level 4 only.

---

Source lives under `app/` (gitignored, D-003) - this file is the tracked-repo record
of that change, not a substitute for reading the actual code.
