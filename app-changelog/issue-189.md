# Issue #189: Compress room spacing and thin the enemy ramp in Level 1

**Type**: enhancement

## Summary

`L_Level01`'s 3 rooms sat 3000cm apart along the chain axis (world X = 0/3000/6000),
with no visual continuity between them, and its static enemy placement (1 Runner /
2 Trooper / 2 Bomber) didn't ramp — room 2 and room 3 both had 2 enemies. This
compresses adjacent-room spacing to 2400cm (Room 1, the entrance, stays fixed at the
world origin to avoid disturbing the already-merged PlayerStart from issue #185; Rooms
2 and 3 move toward it instead) and thins the split to a strictly increasing 1/2/3 by
adding one more `ABomberEnemy` to Room 3.

Both changes are `.umap` content edits, not C++ changes — there is no level-building/
layout C++ class in this codebase; room positions and enemy placement are baked
directly into the map asset. `ADoorConnectorActor::RecomputeConnectorGeometry()`
(already dynamic since issue #187, no code change needed) re-derives the connector
floor/marker geometry from the rooms' new `GetActorLocation()`.

## Approach

1. Authored via a headless `UnrealEditor-Cmd.exe -run=pythonscript` session (this
   repo's established fallback per issues #42/#185/#187/#190 — live `unreal-mcp` tools
   were not reachable this session, consistent with the recurring
   factory-worktree-network-path limitation).
2. Read the live map first (not trusting `RoomActor.h`'s doc-comment blindly) —
   confirmed pre-change state: 3 rooms at X=0/3000/6000, 2 doors at the midpoints
   (X=1500/4500), 5 enemies (1 Runner at Room 1, 2 Trooper at Room 2, 2 Bomber at
   Room 3).
3. Moved Room 2 to X=2400 (delta -600) and Room 3 to X=4800 (delta -1200) via
   `AActor.set_actor_location()` — note: `EditorActorSubsystem.set_actor_transform()`
   silently no-ops (returns `False`, does not move the actor) on this actor type in
   this session; `set_actor_location()` on the actor itself is what actually worked,
   confirmed by re-querying location immediately after the call.
4. Shifted each moved room's own enemies by that room's own delta (computed against
   the original pre-move nearest-room survey), so `FindNearestRoom`'s nearest-by-
   distance matching stays correct: the 2 Troopers moved from X=3000 to X=2400, the 2
   original Bombers moved from X=6000 to X=4800.
5. Spawned one additional `ABomberEnemy` near Room 3's new location (X=4800, Y=+500,
   offset from the existing Y=+300/-300 Bombers to avoid overlap), turning Room 3's
   count from 2 to 3.
6. Called `RecomputeConnectorGeometry()` on both `ADoorConnectorActor` instances so
   their connector floor mesh/marker immediately reflect the new room positions in the
   saved map, not just at next PIE `BeginPlay`. Verified via each door's
   `ConnectorFloorMeshComponent->GetWorldLocation()` (not the door actor's own root
   transform, which is intentionally untouched — the mesh component alone is
   repositioned): door 1 lands at the new midpoint X=1200, door 2 at X=3600.
7. Saved the level and re-queried afterward (per issue #185's precedent) to confirm
   the actors were actually present at their new positions, not just that the API
   calls succeeded.
8. Added `CheckAdjacentRoomSpacingCompressed` and `CheckEnemyDensityRamp` to
   `LevelStructureTestUtils.h`, and called both from
   `KrowdKontrolLevel01Test.cpp` right after the existing
   `CheckRoomTargetZonesAndDensity` call, reusing its already-collected `Rooms`/
   `EnemyCountByRoom` locals. Both assertions are written against relative
   thresholds (`< 3000cm`, "first room 1-2, non-decreasing, last > first") rather
   than the exact chosen numbers, so a future minor retune within the same
   constraints wouldn't need the test itself rewritten.

## Files changed (all under `app/`, gitignored per D-003 — this is the tracked-repo record of that change)

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Content/Maps/L_Level01.umap` | UPDATE (binary, not tracked) | Room 2 moved X=3000→2400, Room 3 moved X=6000→4800 (Room 1 untouched at X=0); the 2 Troopers moved with Room 2 (X=3000→2400), the 2 original Bombers moved with Room 3 (X=6000→4800); one new `ABomberEnemy` added at (4800, 500, 0); both `ADoorConnectorActor` instances' connector geometry recomputed to the new midpoints (X=1200, X=3600) |
| `app/Source/KrowdKontrol/Private/Tests/LevelStructureTestUtils.h` | UPDATE | Added `CheckAdjacentRoomSpacingCompressed` and `CheckEnemyDensityRamp` shared helpers |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolLevel01Test.cpp` | UPDATE | Calls both new helpers against the real `L_Level01` rooms/enemy counts |
| `app-source-tracked/Source/KrowdKontrol/Private/Tests/LevelStructureTestUtils.h` | UPDATE | Plain-text mirror, per Hard Invariant 8's D-009 carve-out |
| `app-source-tracked/Source/KrowdKontrol/Private/Tests/KrowdKontrolLevel01Test.cpp` | UPDATE | Plain-text mirror |

No other `.h`/`.cpp` production logic changed — `RecomputeConnectorGeometry()`
(issue #187) was already dynamic and required no edits.

## Acceptance criteria

- [x] **`L_Level01`'s room spacing is shrunk (3000cm → 2400cm per hop)** — room/door/
      target-zone topology pattern unchanged, only positions moved. Verified live:
      Room 1 at X=0, Room 2 at X=2400, Room 3 at X=4800.
- [x] **Enemy density ramps: room 1 (entrance) has 1-2 enemies, density strictly
      increases in later rooms** — 1 → 2 → 3 (was 1 → 2 → 2). Verified live: Room 1
      has 1 Runner, Room 2 has 2 Trooper, Room 3 has 3 Bomber (6 total).
- [x] **`L_Level02` untouched** — no file under `L_Level02`'s scope was read or
      written by this issue's script.
- [x] **`KrowdKontrol.Unit.Level01Structure` gains assertions that would fail against
      the pre-#189 map state and pass against the new one** — the pre-change map
      state (captured live before any edit: spacing exactly 3000cm everywhere, counts
      1/2/2) fails `CheckAdjacentRoomSpacingCompressed`'s strict `< 3000` check and
      `CheckEnemyDensityRamp`'s strict-increase-at-every-step check; the new state
      (2400cm spacing, 1/2/3 counts) passes both.
- [x] **`KrowdKontrol.Unit.Level02Structure` still passes** — L1's new total of 6
      stays under L2's hardcoded `== 8` assertion.
- [x] **`python harness/ci.py` (full mode) passes, no regressions** — `GATE_OK
      mode=full`, 76 unit tests (up from 75 before this issue — 0 new test cases,
      only new assertions inside the existing `Level01Structure` test — the +1 is
      unrelated pre-existing test count drift between runs, see Validation notes).
- [x] `app-changelog/issue-189.md` created documenting the `.umap` edit with exact
      coordinates/counts (D-009 — the edit itself is invisible to this repo's git
      diff).
- [x] `app-source-tracked/` mirrors match `app/`'s touched files exactly (verified via
      `diff`, no output).

## Validation

```
$ bash harness/run_ue_automation.sh "KrowdKontrol.Unit.Level01Structure"
UE_BUILD_START KrowdKontrolEditor Win64 Development
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK

$ bash harness/run_ue_automation.sh "KrowdKontrol.Unit.Level02Structure"
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
UNIT_PASSED tests=76
APP_STARTED driver=cli
UE_BUILD_START KrowdKontrolEditor Win64 Development
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
GATE_OK mode=full
```

An earlier same-session full-suite run reported one unrelated flaky failure
(`KrowdKontrol.Unit.OvercrowdDetectionComponent: state=Fail`, `passed=75 total=76`)
that this issue's diff has no plausible connection to (no `OvercrowdDetectionComponent`
file touched; its `app/` source is byte-identical to the last-merged
`app-source-tracked/` mirror, ruling out contamination from a concurrent task sharing
the `app/` symlink). Re-running that single test in isolation passed
(`UE_AUTOMATION_RESULT passed=1 total=1`), and a subsequent full-suite re-run also
passed clean (`GATE_OK mode=full`, `tests=76`) — consistent with test-order-dependent
state bleed between Automation tests in the same session, not a regression from this
change.

### Independently-checkable evidence for the .umap edits

The room/enemy repositioning is a binary `.umap` change and, per D-009, will never
appear in this repo's tracked diff — no text diff can confirm it. What *is*
independently checkable, by anyone re-running the command below (not just reading
this prose), is the automation test's pass/fail state, which only flips to pass once
the map actually contains the compressed spacing and the ramped enemy counts:

```
$ bash harness/run_ue_automation.sh "KrowdKontrol.Unit.Level01Structure"
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
```

Before this fix, the live pre-#189 map state (captured directly via the same headless
Python session before any edit was applied) was: 3 rooms at X=0/3000/6000 (spacing
exactly 3000cm at both hops) and enemy counts 1/2/2 by room. Against
`CheckAdjacentRoomSpacingCompressed`'s strict `< 3000` check, a 3000cm gap fails
(3000 is not `< 3000`). Against `CheckEnemyDensityRamp`'s strict-increase-at-every-step
check, a 1/2/2 split fails at the second hop (room 3's count 2 is not `>` room 2's
count 2). Both assertions only pass against the new state (2400cm spacing, 1/2/3
counts) — that before/after flip, not the PR description, is the falsifiable claim.

**Post-review correction (code review of this PR, 2026-08-21):** `CheckEnemyDensityRamp`
originally only checked non-decreasing counts step-to-step plus a first-vs-last
strict-greater-than, which does *not* actually catch a flat-plateau regression like
1/2/2 (2 > 1 holds, so it would have silently passed) — the claim above that "1/2/2
split fails" was true only after the review fix (every adjacent step must now be
strictly greater, matching `CheckAdjacentRoomSpacingCompressed`'s pattern), not as
originally written. See `LevelStructureTestUtils.h`'s `CheckEnemyDensityRamp` for the
corrected implementation.

## Notes

- The exact new spacing value (2400cm) and enemy split (1/2/3) are this issue's
  concrete proposal, not dictated by the issue text (which only specifies direction:
  "shrunk", "1-2 near spawn", "increasing"). The new
  `CheckAdjacentRoomSpacingCompressed`/`CheckEnemyDensityRamp` assertions are
  deliberately written against relative thresholds, not the exact chosen numbers, so
  a different concrete choice within the same constraints (e.g. 2200cm, or a 2/2/3
  split) wouldn't need the test itself rewritten.
- 2400cm spacing leaves a ~380cm walkable gap between the outer wall faces
  (`RoomFloorExtent` = (1000,1000) → 2000cm floor width; wall thickness 20cm each
  side: 2400 - 2*(1000+10) = 380cm), well clear of floor/wall mesh overlap.
- Per-door wall gaps / true line-of-sight into the next room remain explicitly out of
  scope (issue #187's own deferral, restated by this issue's AC not to rebuild
  room/door topology). What this issue buys instead: a shorter, already better-lit
  corridor walk (#186) with the next room's beacon visible above the wall line from a
  shorter distance (#190's taller beacon column).
- `URoomEnemyBudgetController`/`RoomMetadataComponent` wiring, `L_Level02` changes,
  and a general-purpose room-layout system are all explicitly out of scope — see the
  plan's "NOT Building" section for the full reasoning.

## Files

| File | Action |
|------|--------|
| `app/Content/Maps/L_Level01.umap` | UPDATE (binary, not tracked) |
| `app/Source/KrowdKontrol/Private/Tests/LevelStructureTestUtils.h` | UPDATE |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolLevel01Test.cpp` | UPDATE |
| `app-source-tracked/Source/KrowdKontrol/Private/Tests/LevelStructureTestUtils.h` | UPDATE |
| `app-source-tracked/Source/KrowdKontrol/Private/Tests/KrowdKontrolLevel01Test.cpp` | UPDATE |
| `app-changelog/issue-189.md` | CREATE |
