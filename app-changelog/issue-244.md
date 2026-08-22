# Issue #244: Scope enemy detection onset (Idle→Alert) to the player's own room

**Type**: bug

## Summary

`AEnemyBase::TickCheckDetection` advanced Idle→Alert on pure proximity
(`FVector::Dist` vs `DetectionRangeUnits`), with no room awareness — an enemy could
detect the player through a solid wall, before the player ever entered its room.

This adds a room-ownership back-reference from `AEnemyBase` to `ARoomActor`
(`OwningRoom`, mirroring `ARoomActor::OwnedEnemies`, issue #218) and gates the
Idle→Alert transition on the player's location resolving — via the same
nearest-room-by-distance rule `ARoomActor::FindNearestRoom` already uses for
`OwnedEnemies` auto-discovery — to that same room. Enemies with no owning room keep
the current unscoped behavior. No new perception, line-of-sight, or geometry system
was introduced; `ARoomActor::FindNearestRoom` gained an `FVector` overload (the
existing `AActor*` overload now delegates to it) so `TickCheckDetection` — which only
ever receives a player *location*, never the pawn — can resolve "which room is the
player nearest to" without a signature change that would ripple through ~150
existing test call sites.

## Acceptance criteria

- [x] An enemy with a non-null `OwningRoom` only transitions Idle→Alert while the
      player's nearest room (via `ARoomActor::FindNearestRoom`) equals `OwningRoom`,
      regardless of distance within `DetectionRangeUnits`.
- [x] An enemy with a null `OwningRoom` keeps unscoped proximity-radius behavior,
      unchanged from before this change.
- [x] Already-Alert/Attack/Controlled/Banked enemies are unaffected — no change to
      `AdvanceToAttack`, `TickChaseMovement`, or any state past Idle.
- [x] `ARoomActor::AddOwnedEnemy()` sets the new back-reference for every enemy it
      adds, including via `BeginPlay`'s existing nearest-room auto-discovery — zero
      new `.umap` authoring required.
- [x] `Source/KrowdKontrol/Private/Tests/KrowdKontrolEnemyRoomDetectionGateTest.cpp`
      exists in both `app/` and `app-source-tracked/`, passes, and covers both
      explicit AC test cases plus the no-owning-room fallback.

## Validation evidence

```
$ harness/run_ue_automation.sh "KrowdKontrol.Unit.RoomActorDoorGating+KrowdKontrol.Unit.RoomActorBankingWiring+KrowdKontrol.Unit.EnemyBase+KrowdKontrol.Unit.RoomActor+KrowdKontrol.Unit.EnemyRoomDetectionGate"
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=6 total=6
UE_AUTOMATION_OK
```

Full `KrowdKontrol.Unit.*` suite (`python harness/ci.py --quick`): `passed=90
total=92`. The 2 failures are unrelated to this diff:

- `KrowdKontrol.Unit.CrowdMasterySubsystem` — flaked once under the full-suite run,
  passed cleanly (`passed=1 total=1`) on an isolated re-run; test-order flake, not
  reproducible, no relation to `EnemyBase`/`RoomActor`.
- `KrowdKontrol.Unit.PlayerControllerShowsMouseCursor` — fails consistently even in
  isolation, but the failure traces to unrelated, unmerged in-flight work from other
  concurrent tasks (issues #246/#247/#251/#262) already sitting in the shared,
  gitignored `app/` symlink (`KrowdKontrolPlayerController.h/.cpp`,
  `FlatCamera3DPrototypePawn.h/.cpp` all differ substantially from their
  `app-source-tracked/` mirrors, which reflect `main`). Neither the failing test nor
  the files it exercises reference `AddOwnedEnemy`/`SetOwningRoom`/`OwningRoom` or any
  other symbol this change touches. Not fixed here — those files are out of scope for
  this issue and belong to other in-flight PRs.

## Post-review follow-ups (PR #274 review)

- **Performance**: `IsPlayerInOwningRoom` no longer rebuilds the full
  `TActorIterator<ARoomActor>` room list from scratch on every call — a per-frame
  cache (`GetCachedRoomList` in `EnemyBase.cpp`) now collapses what was an
  O(enemies × rooms) scan/allocation every tick into one shared scan per frame.
- **Test coverage**: added a 4th case to `KrowdKontrolEnemyRoomDetectionGateTest.cpp`
  proving an already-Alert gated enemy still reaches Attack purely on distance even
  with the player outside `OwningRoom` — directly covers the "already-Alert enemies
  are unaffected" AC above, not just the code structure that happens to guarantee it
  today.
- **Test coverage**: added `KrowdKontrol.Unit.RoomActorFindNearestRoom` to
  `KrowdKontrolRoomActorTest.cpp` — direct 3-room + empty-array coverage for
  `ARoomActor::FindNearestRoom`, which the gate test only ever exercised transitively
  with two rooms.
- **Docs**: marked `docs/prd-room-encounter-flow.md` REQ-2 as implemented (PR #274),
  matching this repo's existing `— ✅ implemented, PR #NNN` convention.

## Files

| File | Action |
|------|--------|
| `app/Source/KrowdKontrol/RoomActor.h` | UPDATE |
| `app/Source/KrowdKontrol/RoomActor.cpp` | UPDATE |
| `app/Source/KrowdKontrol/EnemyBase.h` | UPDATE |
| `app/Source/KrowdKontrol/EnemyBase.cpp` | UPDATE |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolEnemyRoomDetectionGateTest.cpp` | CREATE |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolRoomActorTest.cpp` | UPDATE |
| `docs/prd-room-encounter-flow.md` | UPDATE |

## Out of scope (follow-up)

- De-aggro / leashing for already-Alert enemies.
- A new perception, line-of-sight, or navmesh-visibility system.
- Precise room-bounds (box/volume) containment checks — no such primitive exists in
  the codebase today; nearest-room-by-distance remains the established ground truth.
- The room-entry countdown companion issue — lands independently.
