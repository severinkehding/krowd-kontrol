# Issue #45: Build hand-authored Level 3 (Alpha, high difficulty tier, third of five)

Authors the third of krowd-kontrol's 5 hand-authored Alpha levels
(`/Game/Maps/L_Level03.umap`) on top of the already-merged `ARoomActor`/
`ADoorConnectorActor` foundation (issue #39), `L_Level02` (issue #43), the playable-spawn
pattern (issue #185), and the compressed-spacing precedent (issue #189). Level 3 is a
5-room linear chain — strictly more rooms and strictly more enemies than Level 2's 4
rooms / 8 enemies — carrying 10 static placeholder-density enemies (2 per room, cycling
through all 4 `EEnemyType` values), so the difficulty ramp Levels 2-5 build on
(MISSION.md P0) now has a third real data point. Structural scope only: room count, door
connectivity, and target-zone placement — no narrative content, no new mechanics.

The level was authored via a headless Unreal Python session
(`UnrealEditor-Cmd.exe -run=pythonscript`), not the MCP plugin's `ProgrammaticToolset`
sandbox, because `ARoomActor::AddTargetZone()` is a genuine `UFUNCTION` call the sandbox
cannot invoke (no `unreal` module, no method-invocation tool) — same technique issues
#42/#43/#185/#189 used. The factory worktree's live `unreal-mcp` connection was not
attempted first given the recurring `project_factory_worktree_no_unreal_mcp_network_path`
limitation; the headless commandlet route was used directly, consistent with every prior
level-authoring issue.

## Approach

1. Read the live `L_Level02` map first (not trusting the plan's assumed 3000cm spacing)
   via a headless Python dump: confirmed 4 rooms at X=0/2500/5000/7500 (2500cm spacing,
   not 3000cm), 3 doors at the midpoints, 1 `ALevelLightingRigActor` at (3000,0,500), one
   `PlayerStart`+pawn at (0,0,75), and 8 enemies (2 per room: each room's enemies match
   its 2 target-zone types, cycling `RU_NNR→TR_UPR→B0_0MR→SN_1PR` across rooms).
2. Designed Level 3 as a superset of that pattern: 5 rooms at X=0/2500/5000/7500/10000
   (same 2500cm spacing), 4 `ADoorConnectorActor`s at the midpoints
   (1250/3750/6250/8750), 1 `ALevelLightingRigActor` at (5000,0,500) — the chain's
   midpoint — one `PlayerStart`+`AFlatCamera3DPrototypePawn` at (0,0,75) (the entrance
   room), and 10 enemies (2 per room, continuing the same type-cycling pattern — Room 5
   wraps back to `RU_NNR`/`TR_UPR`, reusing types rather than requiring a 5th enemy
   type).
3. Authored via `unreal.EditorLevelLibrary.new_level()` +
   `EditorActorSubsystem.spawn_actor_from_class()` (spawning directly at each target
   location, so the documented `set_actor_transform()` no-op gotcha from issue #189
   didn't apply here — no post-spawn move was needed), `AddTargetZone()` once per
   distinct enemy type per room, `RecomputeConnectorGeometry()` per door, and
   `save_current_level()`.
4. Re-queried the saved map in a second, independent headless process (confirms actual
   on-disk persistence, not just in-memory state) — confirmed exactly 5 rooms, 4 doors,
   10 enemies, 1 lighting rig, 1 `PlayerStart`, 1 pawn, and each room's target zones
   matching its placed enemy types.
5. Added `KrowdKontrol.Unit.Level03Structure`
   (`KrowdKontrolLevel03Test.cpp`, mirroring `KrowdKontrolLevel02Test.cpp` exactly),
   which loads both `L_Level02` and `L_Level03` in one test function and asserts the
   room-count/enemy-count comparison directly against the live `L_Level02` asset, not a
   hardcoded magic number, so it can't silently drift if Level 2 is ever revised.
6. Extended `KrowdKontrolGameplayLevelPlayableSpawnTest.cpp`'s `GameplayLevelMapPaths`
   array with `/Game/Maps/L_Level03` so its existing "every gameplay level has a
   playable spawn" coverage extends to the new map.

## Files changed (all under `app/`, gitignored per D-003 — this is the tracked-repo
record of that change, see the closing note below)

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Content/Maps/L_Level03.umap` | CREATE | Binary map asset (94578 bytes): 5 `ARoomActor` instances in a linear chain (X=0/2500/5000/7500/10000), 4 `ADoorConnectorActor` instances wiring adjacent rooms, two target zones per room via `AddTargetZone()` (cycling all 4 `EEnemyType` values), 10 static placeholder-density enemy actors (3x `ARunnerEnemy`, 3x `ATrooperEnemy`, 2x `ABomberEnemy`, 2x `ASniperEnemy`), one `ALevelLightingRigActor`, one `PlayerStart` + one `AFlatCamera3DPrototypePawn` at the entrance room (0,0,75) — no live AI/GameMode wiring |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolLevel03Test.cpp` | CREATE | `KrowdKontrol.Unit.Level03Structure` — loads `L_Level02` then `L_Level03`, asserts Level03's room count (5, strictly > Level02's live count) and door count (4, each connecting two distinct rooms), walks the door adjacency graph (BFS) to confirm all 5 rooms are reachable, asserts every door has a visible marker, asserts every room has >=1 target zone and >=1 enemy placeholder with matching types (REQ-2), asserts the banking zones self-heal, and asserts Level03's total enemy count (10) is strictly greater than Level02's live count |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolGameplayLevelPlayableSpawnTest.cpp` | UPDATE | Appended `TEXT("/Game/Maps/L_Level03")` to `GameplayLevelMapPaths` |
| `app-source-tracked/Source/KrowdKontrol/Private/Tests/KrowdKontrolLevel03Test.cpp` | CREATE | Plain-text mirror of the new test file, per Hard Invariant 8's D-009 carve-out, so GitHub has a real diff to open a PR against |
| `app-source-tracked/Source/KrowdKontrol/Private/Tests/KrowdKontrolGameplayLevelPlayableSpawnTest.cpp` | UPDATE | Plain-text mirror of the updated test file |

Not mirrored here: `L_Level03.umap` itself is a binary asset, excluded from
`app-source-tracked/` per CLAUDE.md's mirror rules (never `.uasset`/`.umap`/anything
under `Content/`) — only the new/changed `.cpp` test files are mirrored.

## Acceptance criteria

- [x] **A new level map is built using the `ARoomActor`/`ADoorConnectorActor`
      classes** — confirmed on disk, 94578 bytes.
- [x] **Room count and placeholder enemy-density strictly greater than Level 2's (4
      rooms / 8 enemies)** — 5 rooms / 10 enemies, asserted dynamically against the
      live `L_Level02` asset by `KrowdKontrol.Unit.Level03Structure`.
- [x] **Every room has >=1 target-zone marker per enemy type placeholder present in
      that room (REQ-2)** — asserted per-room, checked against the actual distinct
      enemy types placed in each room via nearest-room-by-distance matching.
- [x] **Enemy presence is placeholder markers only — no live AI/GameMode wiring** —
      `ARunnerEnemy`/`ATrooperEnemy`/`ABomberEnemy`/`ASniperEnemy` placed as static
      content only, using existing, already-tested classes.
- [x] **`KrowdKontrol.Unit.Level03Structure` confirms the level loads without errors
      and its room count exceeds Level 2's, computed dynamically against the live
      `L_Level02` asset** — passes, 1/1; the test loads both levels in the same
      function and compares live counts, not hardcoded numbers.
- [x] **`KrowdKontrolGameplayLevelPlayableSpawnTest.cpp` extended and passing for
      `L_Level03`** — `KrowdKontrol.Unit.GameplayLevelsHavePlayableSpawn` passes,
      now checking all 3 maps.
- [x] **No regressions in existing `KrowdKontrol.Unit.*`/`KrowdKontrol.Smoke.*`/
      `KrowdKontrol.PIE.*` tests; `python harness/ci.py` reports `GATE_OK
      mode=full`** — see Validation below.

## Validation

```
$ bash harness/run_ue_automation.sh "KrowdKontrol.Unit.Level03Structure"
UE_BUILD_START KrowdKontrolEditor Win64 Development
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK

$ bash harness/run_ue_automation.sh "KrowdKontrol.Unit.GameplayLevelsHavePlayableSpawn"
UE_BUILD_START KrowdKontrolEditor Win64 Development
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK

$ python harness/ci.py
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=107
PIE_PASSED tests=5
APP_STARTED driver=cli
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
HOLDOUT_ABSENT no .factory/holdout/run.py
MUTATIONS_ABSENT no harness/mutations/run.py
GATE_OK mode=full
```

`UNIT_PASSED tests=107` includes the new `KrowdKontrol.Unit.Level03Structure` test
alongside every pre-existing `KrowdKontrol.Unit.*`/`KrowdKontrol.Smoke.*` test — no
regressions. The gate passed cleanly; no fixes were required during implementation or
validation.

MISSION.md Hard Invariants reviewed against the diff: no-kill rule, 5-colour lock,
5-ability roster, and engine/dimensionality lock are all N/A (no such logic touched);
4-type enemy roster (#5) is respected — only existing enemy classes
(`ARunnerEnemy`/`ATrooperEnemy`/`ABomberEnemy`/`ASniperEnemy`) are placed, no new enemy
type introduced; `app/` not tracked in git (#8) — all new files stayed under the
untracked `app/` symlink, mirrored here only as a plain-text source copy per D-009.

### Independently-checkable evidence for the .umap creation

`L_Level03.umap` is a binary asset and, per D-009, will never appear in this repo's
tracked diff — no text diff can confirm it. What *is* independently checkable, by
anyone re-running the command below (not just reading this prose), is the automation
test's pass/fail state, which only flips to pass once the map actually contains the
5-room/10-enemy structure:

```
$ bash harness/run_ue_automation.sh "KrowdKontrol.Unit.Level03Structure"
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
```

Before this fix, `/Game/Maps/L_Level03` did not exist at all, so this test (and the
`.cpp` file itself) did not exist either — there is no "before" state to run the test
against, unlike a content-edit issue. The falsifiable claim here is that the test now
exists, references the new map, and passes against the live asset actually on disk
(confirmed via a second, independent headless re-query in step 4 of the Approach
above, not just the authoring script's own success).

## Notes

- This is the third of five hand-authored Alpha levels; MISSION.md's 5-level decision
  (2026-08-17, resolving issue #69) is respected — this changelog and the new test's
  comments say "the next Alpha level after Level 2," never "third of 3."
- `L_Level02`'s live spacing (2500cm) differs from what the plan artifact assumed
  (3000cm, based on a stale reading of issue #189, which only ever touched
  `L_Level01`) — Task 1's live re-query caught this before design, so Level 3 was
  built consistent with Level 2's actual on-disk spacing rather than a wrong
  assumption.
- `AbilityUnlockComponent.cpp`'s `GetLevelToAbilityMap()` already maps level index 3 →
  `EAbilitySlot::Root`, and `AbilityUnlockLevelSubsystem`'s `ParseLevelIndexFromMapName`
  is fully generic — Level 3 unlocking Root on entry requires zero code changes, purely
  a consequence of the map being named `L_Level03`.
- `LevelBriefingData`/`LevelSequenceData` DataTable row authoring (content assets
  outside `app/Source/`) is explicitly out of scope per the plan's "NOT Building"
  section — flagged as a possible follow-up, not required by issue #45's stated
  acceptance criteria.
- Levels 4-5 are not built here — this issue is Level 3 only.

---

Source lives under `app/` (gitignored, D-003) — this file is the tracked-repo record
of that change, not a substitute for reading the actual code.
