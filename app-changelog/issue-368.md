# Issue #368: Author Level 5 (`/Game/Maps/L_Level05`) as the demo finale

Authors the fifth and final of krowd-kontrol's 5 hand-authored Alpha levels
(`/Game/Maps/L_Level05.umap`) on top of the already-merged `ARoomActor`/
`ADoorConnectorActor` foundation (issue #39) and `L_Level04` (issue #367). Level 5 is a
7-room linear chain - strictly more rooms and strictly more enemies than Level 4's 6
rooms / 12 enemies - carrying 14 static placeholder-density enemies (2 per room,
continuing Level 4's own live sliding-window type-cycling pattern one step further), so
the difficulty ramp Levels 2-5 build on (docs/prd-levels-4-5.md REQ-2) now has its fifth
and final real data point, and registers it as the new final row of
`DT_LevelSequenceTable` (Level 4's row updated to point at it). Structural scope only:
room count, door connectivity, and target-zone placement - no narrative content, no new
mechanics, no briefing/unlock UI, no boss encounter.

The level was authored via a headless Unreal Python session
(`UnrealEditor-Cmd.exe -run=pythonscript`), not the MCP plugin's `ProgrammaticToolset`
sandbox, because `ARoomActor::AddTargetZone()` is a genuine `UFUNCTION` call the sandbox
cannot invoke (no `unreal` module, no method-invocation tool) - same technique issues
#42/#43/#45/#185/#189/#367 used. The factory worktree's live `unreal-mcp` connection was
not attempted first given the recurring
`project_factory_worktree_no_unreal_mcp_network_path` limitation; the headless
commandlet route was used directly, consistent with every prior level-authoring issue.

## Approach

1. Read the live `L_Level04` map first (not trusting the plan's assumed 2500cm spacing
   or its assumed enemy pattern) via a headless Python dump: confirmed 6 rooms at
   X=0/2500/5000/7500/10000/12500 (2500cm spacing, matching the plan's assumption), 5
   doors at the midpoints, 1 `ALevelLightingRigActor` at (6250,0,500), one
   `PlayerStart`+pawn at (0,0,75), and 12 enemies (3 Runner/4 Trooper/3 Bomber/2
   Sniper). The live enemy layout confirmed the sliding-window cycle exactly as
   issue #367 documented it (room N places cycle positions `[N, N+1] mod 4` of
   Runner->Trooper->Bomber->Sniper) - no plan-vs-live divergence this time, unlike
   issue #45 (spacing) and issue #367 (pattern shape) before it. `DT_LevelSequenceTable`
   read back as exactly 4 rows: `L_Level01->L_Level02`, `L_Level02->L_Level03`,
   `L_Level03->L_Level04`, `L_Level04->` empty.
2. Designed Level 5 as a superset of that live pattern: 7 rooms at
   X=0/2500/5000/7500/10000/12500/15000 (same 2500cm spacing), 6
   `ADoorConnectorActor`s at the midpoints (1250/3750/6250/8750/11250/13750), 1
   `ALevelLightingRigActor` at (7500,0,500) - the chain's new midpoint - one
   `PlayerStart`+`AFlatCamera3DPrototypePawn` at (0,0,75) (the entrance room), and 14
   enemies (2 per room, continuing the live sliding-window pattern one step further:
   Room6 (x=15000) places cycle positions `[6,7] mod 4` = Bomber+Sniper). Resulting
   type totals: 3 Runner, 4 Trooper, 4 Bomber, 3 Sniper - all four `EEnemyType` values
   present.
3. Authored via `unreal.LevelEditorSubsystem.new_level()` +
   `EditorActorSubsystem.spawn_actor_from_class()` (spawning directly at each target
   location, so the documented `set_actor_transform()` no-op gotcha from issue #189
   didn't apply here - no post-spawn move was needed), `AddTargetZone()` once per
   distinct enemy type per room, `RecomputeConnectorGeometry()` per door, and
   `save_current_level()`.
4. Re-queried the saved map in a second, independent headless process (confirms actual
   on-disk persistence, not just in-memory state) - confirmed exactly 7 rooms, 6 doors,
   14 enemies, 1 lighting rig, 1 `PlayerStart`, 1 pawn, matching the design exactly.
5. Updated `DT_LevelSequenceTable`: read the live rows first (confirmed unchanged from
   issue #367's own update), then applied `fill_from_csv_string` with the full 5-row
   set (`L_Level04->L_Level05`, `L_Level05->` empty), saved, and read back in a second
   independent process to confirm exactly 5 rows with no duplicates and the correct
   values.
6. Added `KrowdKontrol.Unit.Level05Structure`
   (`KrowdKontrolLevel05Test.cpp`, mirroring `KrowdKontrolLevel04Test.cpp` exactly with
   `04`->`05` and baseline `L_Level03`->`L_Level04` substitutions), which loads both
   `L_Level04` and `L_Level05` in one test function and asserts the room-count/
   enemy-count comparison directly against the live `L_Level04` asset, not a hardcoded
   magic number, so it can't silently drift if Level 4 is ever revised.
7. Extended `KrowdKontrolGameplayLevelPlayableSpawnTest.cpp`'s `GameplayLevelMapPaths`
   array with `/Game/Maps/L_Level05` so its existing "every gameplay level has a
   playable spawn" coverage extends to the new map.
8. Confirmed (did not re-implement) the 5->Snare unlock: `HandleLevelBegin("L_Level05")`
   is already asserted to unlock Snare at
   `KrowdKontrolAbilityUnlockLevelSubsystemTest.cpp:40-42,96-100` (added under issue
   #217). That test is purely string-based (`ParseLevelIndexFromMapName`) and never
   depended on `/Game/Maps/L_Level05` existing, so this AC was already covered
   end-to-end before this issue - re-ran it here only to confirm no regression.
9. Confirmed (did not re-implement) the FINISH-RUN -> main-menu routing:
   `KrowdKontrolPostRunSummaryNextLevelButtonTest.cpp:123-193` already asserts that a
   final row (`NextLevelMapName == NAME_None`) routes to
   `UGameMapsSettings::GetGameDefaultMap()`, built against a synthetic
   `BuildSequenceTable` independent of any real level name. Re-ran it here to confirm
   no regression now that `L_Level05` is the real final row.

## Files changed (all under `app/`, gitignored per D-003 - this is the tracked-repo
record of that change, see the closing note below)

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Content/Maps/L_Level05.umap` | CREATE | Binary map asset: 7 `ARoomActor` instances in a linear chain (X=0/2500/5000/7500/10000/12500/15000), 6 `ADoorConnectorActor` instances wiring adjacent rooms, two target zones per room via `AddTargetZone()` (all 4 `EEnemyType` values present across the level), 14 static placeholder-density enemy actors (3x `ARunnerEnemy`, 4x `ATrooperEnemy`, 4x `ABomberEnemy`, 3x `ASniperEnemy`), one `ALevelLightingRigActor`, one `PlayerStart` + one `AFlatCamera3DPrototypePawn` at the entrance room (0,0,75) - no live AI/GameMode wiring |
| `app/Content/Data/DT_LevelSequenceTable.uasset` | UPDATE | Added `L_Level05` row (`NextLevelMapName=""`), updated `L_Level04` row (`NextLevelMapName="L_Level05"`) - confirmed 5 rows, no duplicates, via read-back in a second independent process |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolLevel05Test.cpp` | CREATE | `KrowdKontrol.Unit.Level05Structure` - loads `L_Level04` then `L_Level05`, asserts Level05's room count (7, strictly > Level04's live count) and door count (6, each connecting two distinct rooms), walks the door adjacency graph (BFS) to confirm all 7 rooms are reachable, asserts every door has a visible marker, asserts every room has >=1 target zone and >=1 enemy placeholder with matching types, asserts the banking zones self-heal, asserts all four `EEnemyType` values are present somewhere in the level, and asserts Level05's total enemy count (14) is strictly greater than Level04's live count |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolGameplayLevelPlayableSpawnTest.cpp` | UPDATE | Appended `TEXT("/Game/Maps/L_Level05")` to `GameplayLevelMapPaths` |
| `app-source-tracked/Source/KrowdKontrol/Private/Tests/KrowdKontrolLevel05Test.cpp` | CREATE | Plain-text mirror of the new test file, per Hard Invariant 8's D-009 carve-out, so GitHub has a real diff to open a PR against |
| `app-source-tracked/Source/KrowdKontrol/Private/Tests/KrowdKontrolGameplayLevelPlayableSpawnTest.cpp` | UPDATE | Plain-text mirror of the updated test file |

Not mirrored here: `L_Level05.umap` and `DT_LevelSequenceTable.uasset` are binary
assets, excluded from `app-source-tracked/` per CLAUDE.md's mirror rules (never
`.uasset`/`.umap`/anything under `Content/`) - only the new/changed `.cpp` test files
are mirrored.

## Acceptance criteria

- [x] **`/Game/Maps/L_Level05` exists on the `ARoomActor`/`ADoorConnectorActor`
      foundation, authored via the same headless pythonscript commandlet pattern** -
      confirmed via two independent headless read-backs.
- [x] **Room count and total enemy count strictly greater than Level 4's live counts**
      (6 rooms / 12 enemies) - 7 rooms / 14 enemies, asserted dynamically against the
      live `L_Level04` asset by `KrowdKontrol.Unit.Level05Structure`.
- [x] **All four core enemy types present, with type-keyed target zones per room** -
      3 Runner, 4 Trooper, 4 Bomber, 3 Sniper (asserted explicitly, per-room target
      zones matching placed types).
- [x] **Level 5 registered in `DT_LevelSequenceTable` immediately after Level 4's row**
      - confirmed via read-back: `L_Level04->L_Level05`, `L_Level05->` empty (final
      level).
- [x] **`KrowdKontrolLevel05Test.cpp` (`KrowdKontrol.Unit.Level05Structure`) asserts
      room/zone/enemy-density counts and strict excess vs. Level 4** - passes, 1/1.
- [x] **Confirmed (not re-implemented): `UAbilityUnlockLevelSubsystem` 5->Snare mapping
      fires on reaching `L_Level05`** - `KrowdKontrol.Unit.AbilityUnlockLevelSubsystem`
      already asserts this; re-ran, still passes, 1/1.
- [x] **Confirmed (not re-implemented): clearing Level 5 exercises the FINISH-RUN ->
      main-menu routing** - `KrowdKontrol.Unit.PostRunSummaryNextLevelButton` already
      generically covers "final row routes to the main menu"; re-ran, still passes,
      1/1. No blocking finding to record.
- [x] **No unlock-announcement UI added** - untouched; issue #356 stays separately
      owned.
- [x] **No boss encounter added** - not in scope; issue #54 stays parked.
- [x] **No regressions in existing `KrowdKontrol.Unit.*`/`KrowdKontrol.Smoke.*`/
      `KrowdKontrol.PIE.*` tests; `python harness/ci.py` reports `GATE_OK
      mode=full`** - see Validation below.

## Validation

```
$ bash harness/run_ue_automation.sh "KrowdKontrol.Unit.Level05Structure"
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

$ bash harness/run_ue_automation.sh "KrowdKontrol.Unit.PostRunSummaryNextLevelButton"
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

`L_Level05.umap` is a binary asset and, per D-009, will never appear in this repo's
tracked diff - no text diff can confirm it. What *is* independently checkable, by
anyone re-running the command below (not just reading this prose), is the automation
test's pass/fail state, which only flips to pass once the map actually contains the
7-room/14-enemy structure:

```
$ bash harness/run_ue_automation.sh "KrowdKontrol.Unit.Level05Structure"
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
```

Before this fix, `/Game/Maps/L_Level05` did not exist at all, so this test (and the
`.cpp` file itself) did not exist either - there is no "before" state to run the test
against, unlike a content-edit issue.

## Notes

- This is the fifth and final of five hand-authored Alpha levels; docs/prd-levels-4-5.md
  REQ-1 (Level 4) and REQ-2 (Level 5, this issue) are both done. REQ-3 (cross-level
  ramp-tuning pass) remains separately scoped P1 work, now unblocked (Level 5 exists to
  compare against) but not attempted here, per the plan's "NOT Building" section.
- `L_Level04`'s live enemy-placement pattern matched the plan's assumption exactly this
  time (sliding-window cycle, per issue #367's own precedent) - no plan-vs-live
  divergence to record here, unlike issue #45 (spacing) and issue #367 (pattern shape).
- `AbilityUnlockComponent.cpp`'s `GetLevelToAbilityMap()` already maps level index 5 ->
  `EAbilitySlot::Snare`, and `AbilityUnlockLevelSubsystem`'s `ParseLevelIndexFromMapName`
  is fully generic - Level 5 unlocking Snare on entry requires zero code changes, purely
  a consequence of the map being named `L_Level05`. This was already covered by
  `KrowdKontrolAbilityUnlockLevelSubsystemTest.cpp` (issue #217), confirmed still
  passing, not re-implemented.
- `PostRunSummaryWidget`'s FINISH-RUN -> main-menu routing is likewise already generic
  against `NextLevelMapName == NAME_None`, confirmed still passing against the real
  `L_Level05` final row, not re-implemented.
- Onboarding encounter for Snare (issue #31) is now reopenable per the PRD's note, since
  Level 5 exists - reopening/building it is not this issue's job.
- Demo is now completable end-to-end across all 5 Alpha levels; all 5 abilities
  (Stun/Sleep/Root/Fear/Snare) are reachable in a real run for the first time.

## Pass-1 validation fix: `DT_LevelBriefingTable` row for `L_Level05`

The original pass authored this PR's "NOT Building" note above as "`LevelBriefingData`
DataTable row authoring is explicitly out of scope... issue #356's separate ownership
of the pre-level-briefing regression" - that assumption turned out to be wrong.
Checked live: `/Game/Data/DT_LevelBriefingTable` already carries real, populated rows
for `L_Level01`-`L_Level04` (issue #356's own resolution report is what's stale here -
its "the real production LevelBriefingTable content asset is still unset" note predates
whatever follow-up work populated levels 1-4; #356 itself never grew a numbered
follow-up issue for this). So the missing `L_Level05` row wasn't pre-existing,
separately-owned debt - it was this PR's own gap, exactly the kind of thing
`ULevelBriefingSubsystem::HandleLevelBegin`'s existing `L_Level01`-`L_Level04` pattern
should have been extended to cover.

The E2E holdout (pass-1 validation) caught this live: booting PIE directly in
`/Game/Maps/L_Level05` logged `ULevelBriefingSubsystem: no LevelBriefingTable row found
for map 'L_Level05' - no pre-level briefing will show`.

Fixed by adding a fifth row, `L_Level05`, to the live `DT_LevelBriefingTable` asset via
the same headless `UnrealEditor-Cmd.exe -run=pythonscript` technique used throughout
this issue - `export_to_json_string()` to read the table's exact existing JSON
(preserving `L_Level01`-`L_Level04` verbatim, including their original localisation
keys), appended a new `L_Level05` entry, and `fill_from_json_string()` to write it
back, `EditorAssetLibrary.save_loaded_asset()` to persist, then re-read in a second,
independent headless process to confirm exactly 5 rows and no changes to the first 4.
Row content follows the established pattern exactly:

- `LevelDisplayName`: `"LEVEL 5"`
- `ObjectiveLines`: `["PACIFY ALL 14 ROBOTS", "HERD THEM TO THEIR PENS"]` - `14` matches
  this level's real enemy count; the two-line, no-"STUN..."-prefix shape matches
  `L_Level03`/`L_Level04`'s rows (the "STUN..."/"STUN OR SLEEP..." prefix only appears
  on `L_Level01`/`L_Level02`, before the ability-unlock line carries that information).
- `NewAbilityUnlockLine`: `"NEW: SNARE - PRESS 5 - STRONG VS RUNNERS"` - `Snare` is
  level 5's unlock per `AbilityUnlockComponent.cpp`'s `GetLevelToAbilityMap()`; "STRONG
  VS RUNNERS" matches `RunnerEnemy.cpp`'s own documented counter (issue #65: "RU-NNR is
  specifically countered by Snare") and the exact string
  `KrowdKontrolAbilityUnlockPromptComponentTest.cpp:96` already asserts elsewhere
  (`"SNARE — PRESS 5 — STRONG VS RUNNERS"` for the on-screen prompt widget - same
  content, this table's own established hyphen convention from the `L_Level02`-
  `L_Level04` rows rather than that em-dash).

This is a binary DataTable asset under `app/Content/Data/`, excluded from
`app-source-tracked/` per the same mirror rules as `DT_LevelSequenceTable.uasset`
above - no text diff can show it; the independently-checkable evidence is the
`ULevelBriefingSubsystem` log line no longer firing for `L_Level05` on a real PIE boot.

---

Source lives under `app/` (gitignored, D-003) - this file is the tracked-repo record
of that change, not a substitute for reading the actual code.
