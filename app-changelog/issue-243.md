# Issue #243: Seal room perimeters and connector corridors so the gated door is the only route between rooms

**Type**: bug

## Summary

`ARoomActor`'s four wall mesh components (`WallNorth/South/East/WestMeshComponent`)
were constructed with `SetCollisionEnabled(ECollisionEnabled::NoCollision)`
unconditionally — every wall, on every room, in every level. The player pawn
(`AFlatCamera3DPrototypePawn`, `FloatingPawnMovement`, no gravity) could walk straight
through any room wall in any direction, not just around the gated door, defeating the
#218/#229 door-gating "clear the room to advance" mechanic entirely: the invisible
`GateBlockingComponent` box was the *only* thing that ever blocked passage.

This was a deliberate placeholder from the original greybox PR (`1fbae26`, #187/#193,
explicitly named "do not rebuild" in #243's own notes) — `ARoomActor` had no per-door
"which wall side has a door, and where along it" data, so a solid wall on all 4 sides
would have sealed off the connector paths the level also needs to be walkable. Not a
regression; a long-standing, intentionally-deferred gap that #243 finally has the data
to close.

## Fix

- **`ARoomActor::SealRoomPerimeter()`** (called from `BeginPlay()`, public/idempotent
  for `SpawnActor`-after-play test ordering): enables blocking collision
  (`QueryOnly`, block `ECC_WorldDynamic` — the channel the real player pawn presents,
  mirroring `ADoorConnectorActor::GateBlockingComponent`'s existing solve for the same
  problem) on any wall side with no connecting door. On a side that does have a door,
  leaves a matching-width gap flanked by two invisible blocking `UBoxComponent`s so the
  wall is solid except for exactly the doorway.
- **`ADoorConnectorActor`**: two new always-on `CorridorGuardRail` `UBoxComponent`s
  flank the connector strip lengthwise, so the player can't drift sideways off the
  corridor and back into open space once through a gated door.
- New test `KrowdKontrolRoomActorPerimeterSealingTest.cpp`: no-door / one-door (East) /
  two-door (mid-chain room) / idempotency coverage.
- New guard-rail collision/symmetry assertions added to
  `KrowdKontrolDoorConnectorActorTest.cpp`.
- `LevelStructureTestUtils.h`: reworded a stale comment claiming walls stay
  non-blocking, to clarify that's pre-`BeginPlay` construction-time state only.

## Acceptance Criteria

| Criterion | Status |
|---|---|
| Room walls block the player on every side without a door | Done — `SealRoomPerimeter()` enables blocking collision on all doorless sides, verified by new no-door/one-door/two-door test cases |
| The gap left for a door is exactly wide enough for the doorway, not the whole wall | Done — flanking `UBoxComponent`s on either side of the door-width gap, same collision setup as the solid sides |
| Player can't walk around a gated door via the corridor's open sides | Done — new `CorridorGuardRail` box components on `ADoorConnectorActor`, always-on (not gated), assertions added to `KrowdKontrolDoorConnectorActorTest.cpp` |
| Fix is idempotent / safe under existing dynamic-actor test ordering | Done — dedicated idempotency test case; `SealRoomPerimeter()` is safe to call more than once |
| No regression to existing room/door gating (#218/#229) | Done — `GateBlockingComponent` itself untouched; regression suite re-run (see Validation) |
| Manual PIE perimeter-walk (issue's own stated AC) | **Not met.** No live Unreal Editor/MCP connection is reachable from this factory worktree (known gap, see `project_factory_worktree_no_unreal_mcp_network_path`), so this manual-verification bullet was not performed. The issue's automation-infeasibility escape hatch is scoped to the separate "Automation check where feasible" bullet only, not to this one — the automated unit coverage above does not satisfy it. This AC remains open and requires a human (or MCP-connected) PIE pass before #243 can be considered fully closed. |

## Validation

`python harness/ci.py` (full mode) → `GATE_OK` (`UNIT_PASSED tests=108`,
`PIE_PASSED tests=5`, `E2E_PASSED steps=1`). No fixes required during validation —
passed on the first run.

Individually verified (not just via the aggregate count):
- `KrowdKontrol.Unit.RoomActorPerimeterSealing` (new) → 1/1
- `KrowdKontrol.Unit.DoorConnectorActor` (updated) → 1/1
- `KrowdKontrol.Unit.RoomActorDoorGating` (regression — `GateBlockingComponent`
  untouched) → 1/1
- `KrowdKontrol.Unit.RoomActor*` (regression) → 6/6
- `KrowdKontrol.Unit.Level0*Structure` (regression — real shipped levels) → 3/3

No `MISSION.md` hard invariants touched. No protected files (`FACTORY_RULES.md`,
`MISSION.md`, `CLAUDE.md`, `.github/`, deploy config) touched.

**Scope note**: `git diff main...HEAD` initially appeared to include an unrelated
`app-changelog/issue-45.md`. This was stale-`main` noise — local `main` lagged
`origin/main` by several already-merged commits (including PR #304, which added that
file). Diffing against `origin/main...HEAD` shows the real scope: exactly the 7 files
listed below.

## Files Changed

| File | Action |
|---|---|
| `Source/KrowdKontrol/RoomActor.h` | UPDATE |
| `Source/KrowdKontrol/RoomActor.cpp` | UPDATE |
| `Source/KrowdKontrol/DoorConnectorActor.h` | UPDATE |
| `Source/KrowdKontrol/DoorConnectorActor.cpp` | UPDATE |
| `Source/KrowdKontrol/Private/Tests/KrowdKontrolRoomActorPerimeterSealingTest.cpp` | CREATE |
| `Source/KrowdKontrol/Private/Tests/KrowdKontrolDoorConnectorActorTest.cpp` | UPDATE |
| `Source/KrowdKontrol/Private/Tests/LevelStructureTestUtils.h` | UPDATE (comment only) |
