# Issue #215: Minimal per-follower separation for controlled-enemy trains

`AEnemyBase::TickFollowMovement` (issue #214, PR #300) makes every `Controlled`
enemy trail the player pawn, stopping `FollowDistanceUnits` short. With two or more
enemies `Controlled` simultaneously, each one moved toward the exact same
`PlayerLocation` with no separation between them, so at higher crowd density they
visually merged into one blob - undermining the herd mechanic's readability at the
"20+ simultaneous enemies" scale `MISSION.md` calls for. This issue (REQ-2 P1 of the
Herd Mechanic PRD) adds a placeholder-quality radial offset so multiple simultaneous
followers spread into a visually legible train instead of stacking.

## Design decision: `ComputeFollowSeparationOffset()`

A new private `AEnemyBase::ComputeFollowSeparationOffset() const` iterates all
`AEnemyBase` actors in the world via `TActorIterator<AEnemyBase>`, mirroring
`UCrowdMasterySubsystem::SampleControlledCount()`'s exact "iterate + filter by
`Controlled` state" shape - but as an independent, parallel iteration, not a call
into that subsystem (the issue explicitly forbids touching its sampling logic). It
narrows further with the identical Snare/Fear movement-conflict gate
`TickFollowMovement` itself already applies
(`Data.bAllowsMovementWhileControlled || Data.bFleesFromCasterWhileControlled`), so
a Snare- or Fear-controlled enemy - which moves via `TickChaseMovement`/
`TickFleeMovement` instead - never consumes a follow slot or shifts another
follower's assigned angle.

When fewer than 2 such followers exist (including the common `GetWorld() ==
nullptr` case in this file's own `NewObject`-only Automation tests), the offset is
always `FVector::ZeroVector` - a solo controlled enemy behaves byte-for-byte
identically to #214, unmodified. With 2+ followers, each is assigned a distinct
angle (`2*PI*SlotIndex / NumFollowers`) around a new `FollowSeparationRadiusUnits`
circle centered on the player, using `FMath::Cos`/`FMath::Sin` - the same
circular-offset math already used elsewhere in this module
(`AbilityCastComponent.cpp`, `BomberEnemy.cpp`). `TickFollowMovement` now targets
`PlayerLocation + ComputeFollowSeparationOffset()` instead of `PlayerLocation`
directly; its stop-short/clamp logic is otherwise untouched.

`FollowSeparationRadiusUnits` defaults to `150.0f` - large enough to visibly
separate followers at `FollowDistanceUnits`' default trailing gap (200.0f) without
spreading the herd so wide it reads as scattered rather than a train.

**No stable per-enemy slot assignment.** Slot order is simply
`TActorIterator`'s own encounter order for that tick, re-derived fresh every tick -
no Unreal API guarantees `TActorIterator`/`GetUniqueID()` ordering stability across
spawn/destroy churn or level reloads, so relying on one would be an undocumented
assumption. The issue's AC only requires distinct positions *this* tick among
current followers, not a persistent "enemy A always holds slot 2" contract; mild
slot reassignment as the controlled set's membership changes tick-to-tick is
accepted placeholder behavior.

**`UCrowdMasterySubsystem` is untouched.** This issue's offset logic lives entirely
in `AEnemyBase` and reuses `TActorIterator<AEnemyBase>` independently -
`CrowdMasterySubsystem.h`/`.cpp` have zero diff, per the issue's AC.

## Files changed

| File | Action | What it contains |
|------|--------|-------------------|
| `EnemyBase.h` | UPDATE | New `FollowSeparationRadiusUnits` `EditDefaultsOnly` property; private `ComputeFollowSeparationOffset() const` declaration |
| `EnemyBase.cpp` | UPDATE | `ComputeFollowSeparationOffset()` implementation; `TickFollowMovement` now targets `PlayerLocation + ComputeFollowSeparationOffset()` instead of `PlayerLocation` directly |
| `Private/Tests/KrowdKontrolEnemyBaseTest.cpp` | UPDATE | New cases `(j-follow)`/`(j2-follow)`/`(j3-follow)`/`(j4-follow)`: distinct offsets/targets for N>=2 simultaneous followers, Snare-controlled bystanders excluded from slot assignment, zero offset for a solo follower in a real (non-null) World |

## Notes

- No flocking/steering/avoidance system - operator-scoped explicitly out of scope
  for this issue; `ComputeFollowSeparationOffset()` is a pure, stateless per-tick
  computation.
- No obstacle-aware/pathfinding approach routing - a separation-offset target may
  sit inside geometry; accepted placeholder behavior, same tradeoff
  `TickFollowMovement`'s existing straight-line movement already makes for the
  player-direct case (separately tracked under closed issue #83).
- Every pre-existing `(a-follow)`-`(i-follow)` test case is either `NewObject`-only
  (`GetWorld() == nullptr`) or spawns exactly one follower per fresh
  `CreateNewMap()` world - both paths deterministically resolve
  `ComputeFollowSeparationOffset()` to `FVector::ZeroVector`, so their asserted
  distances are unchanged by this issue.

## Acceptance criteria

- [x] Two or more simultaneously `Controlled`-and-following enemies resolve
      distinct follow-target positions.
- [x] The separation mechanism is placeholder-only radial offset - no
      steering/flocking/avoidance algorithm.
- [x] `UCrowdMasterySubsystem`'s existing sampling logic is byte-for-byte
      unchanged.
- [x] A solo `Controlled`-and-following enemy resolves a zero offset, behaving
      identically to pre-#215.
- [x] Automation tests confirm N>=2 simultaneously controlled enemies resolve
      distinct follow-target positions, and that Snare-controlled bystanders don't
      consume a follow slot.
- [x] `app-source-tracked/` mirror and this changelog both written, so the PR has
      a real, openable diff.
