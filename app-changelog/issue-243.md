# Issue #243: Seal room perimeters and connector corridors so the gated door is the only route between rooms

**Second attempt on this branch.** PR #305 built the initial version of this fix,
went through two review rounds and one live operator playtest, and was closed
unmerged after the final automated review found a critical, confirmed correctness
bug ("two doors on one wall side seal each other's doorway shut") with no test
catching it. This PR carries forward everything PR #305 got right (including the
operator's own 806dd0b guard-rail-clamp playtest fix, already present in `app/`) and
fixes the specific defects that closed it.

## Summary

`ARoomActor`'s four wall meshes had collision unconditionally disabled — a
deliberate placeholder from the original greybox pass (#187/PR #193) — so the
player could walk through any room wall in any direction, completely bypassing
#218/#229's "clear the room to advance" door-gating mechanic. This fix makes walls,
corridor guard rails, and per-door wall-gap flanks genuinely solid to
`ECC_WorldDynamic` (the player's own presented channel), while leaving each door's
own gap open, so the gated door becomes the only route between two connected rooms.

## Changes

- **`RoomActor.h`/`.cpp`**: added two shared static helpers —
  `ComputeAxisExitDistance` (ray-exit distance from an axis-aligned box's centre
  along an arbitrary direction, replacing an incorrect support-function formula)
  and `ConfigureWorldDynamicBlockingCollision` (the shared #218 Block-only-channel
  recipe, deduping 3 prior copies). Restructured `SealRoomPerimeter()`/wall-flank
  building from a per-door algorithm to a per-wall-side algorithm: gaps on the same
  wall side are now collected, sorted, and merged before building N+1 solid
  segments — fixing the critical bug that closed PR #305, where two doors on one
  wall side each built a flank that fully covered the other door's gap. Also
  corrected the doorway gap-center math to solve for where the line between the two
  rooms' origins actually crosses the wall plane, instead of using the room-centres
  midpoint (only correct for equal-extent, perfectly collinear pairs).
- **`DoorConnectorActor.h`/`.cpp`**: corridor guard rails now use
  `ComputeAxisExitDistance` instead of the box support function to find each room's
  true wall-perimeter crossing, fixing overshoot for non-axis-aligned connectors
  (identical output to the old formula for every currently shipped axis-aligned
  level, so no existing test's expected values change). Deduped guard-rail and gate
  collision setup onto the shared helper. Fixed a stale comment claiming guard rails
  "run the corridor's full length" (superseded by the operator's own 806dd0b
  perimeter-clamp fix).
- **`EnemyBase.cpp`**: `TickFleeMovement`'s `SetActorLocation` now sweeps
  (`bSweep=true`). Sealing the walls introduced a new soft-lock where a
  Fear-controlled fleeing enemy could be pushed straight through a now-solid wall
  into unreachable space, permanently blocking room-clear. Sweeping alone wasn't
  sufficient — the enemy's own root collision response narrows `ECC_WorldDynamic`
  to `Overlap` for target-zone banking (#211), so the blocking volumes also needed
  `SetCollisionObjectType(ECC_WorldStatic)` (in
  `ConfigureWorldDynamicBlockingCollision`) since the enemy's response to
  `WorldStatic` was never narrowed. Verified empirically: an unmodified enemy
  passed through a sealed wall at a uniform rate before this fix, and stops exactly
  at the wall surface after it.
- **Tests**: added real walkability assertions to the existing multi-door-same-side
  test (proving each door's own gap stays open, not just flank count/channel);
  added a diagonal/asymmetric-extent case proving the corrected gap-center formula;
  added a non-axis-aligned guard-rail case proving the corrected exit-distance
  formula; added an enemy-flee-into-solid-wall regression test. Adjusted one
  pre-existing banking-wiring test's spawn offsets, which had relied on walls being
  fully see-through to reach a target zone through what is now solid geometry.

## Acceptance criteria

| Criterion | Status |
|---|---|
| Room walls block player movement (no walking through any wall) | Done — walls default to solid `ECC_WorldDynamic`-blocking; only genuine door gaps stay open. |
| The gated door is the only route between two connected rooms | Done — per-wall-side flank segmentation closes the same-side-doors bug that let two doors seal each other's gap; each door's own gap is now assertion-verified open. |
| No new soft-locks introduced by sealing the perimeter | Done — swept, `WorldStatic`-blocking fleeing-enemy fix prevents enemies from being pushed through sealed walls into unreachable space. |
| Corridor guard rails fence the door-to-door gap correctly | Done (carries forward operator's live 806dd0b fix, not redone) + non-axis-aligned overshoot fixed for future non-linear room chains. |
| Manual PIE perimeter-walk (issue's own AC) | **Not performable from this factory worktree** — no reachable Editor/MCP connection (see `project_factory_worktree_no_unreal_mcp_network_path`). Disclosed, not silently skipped, same as PR #305. |

## Validation evidence

`harness/ci.py` full gate: `UNIT_PASSED tests=121`, `PIE_PASSED tests=5`,
`E2E_PASSED steps=1`, `GATE_OK mode=full` — clean on the first attempt, no fixes
required. Scope check confirmed the diff touches only
`app-source-tracked/Source/KrowdKontrol/{RoomActor,DoorConnectorActor}.{h,cpp}`,
`EnemyBase.cpp`, and four test files. Cross-checked every touched file byte-for-byte
against `app/`'s live state: all identical except `EnemyBase.cpp`, whose only
difference is pre-existing, already-merged issue #313 content ahead of this
branch's stale fork point (not part of this fix). No concurrent-task leakage found.
No Hard Invariant implicated (collision-only change; no damage/death/destroy path
touched).

Fixes #243
