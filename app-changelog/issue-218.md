# Issue #218: Gate room exit doors until every owned enemy is Banked (attempt 2)

**Type**: bug

## Summary

`ARoomActor` and `ADoorConnectorActor` had no gating mechanic at all — doors were
purely decorative (a walkable floor strip + a visual marker light), so a player could
walk through any door regardless of what was still alive in the room behind them. This
caused a live snowball-aggro incident (Bombers converging on the player from across the
level).

**This is a re-implementation.** PR #228 (attempt 1) built a gating mechanism, passed
its own harness run, but was rejected in validation pass-1 for two independent reasons:

1. **CRITICAL (code review)**: `GateBlockingComponent` blocked `ECC_Pawn`, but the real
   player pawn (`AFlatCamera3DPrototypePawn::MeshComponent`) never sets an explicit
   collision profile/object type, so it inherits `UStaticMeshComponent`'s engine
   default (`BlockAll`, object type `WorldStatic`) — the closed gate never physically
   stopped anyone.
2. **CRITICAL (E2E holdout)**: attempt 1's design required new, hand-authored
   `.umap`-level data (`ADoorConnectorActor::GatingRoom`, `ARoomActor::OwnedEnemies`,
   both `EditInstanceOnly`) that nobody populated on the real `L_Level01`/`L_Level02` —
   correct C++ logic shipped completely unwired in actual play.

This attempt fixes both root causes together:

- `GateBlockingComponent` now blocks `ECC_WorldStatic` (the channel the real player
  actually presents), `ECR_Ignore` on every other channel.
- `ARoomActor::OwnedEnemies` is now **auto-discovered** in `BeginPlay()` via
  nearest-room-by-distance over every `AEnemyBase` in the world — the same rule
  `KrowdKontrolLevelTestUtils::FindNearestRoom` already used and the existing
  `KrowdKontrolLevel01Test.cpp` regression test already trusted for "which room owns
  this enemy." `ADoorConnectorActor::GatingRoom` auto-derives to whichever of
  `RoomA`/`RoomB` sits at the lower world X (closer to the level entrance) when left
  unset. **Zero `.umap` edits required** — both shipped levels already have `RoomA`/
  `RoomB` set on every door and enemies placed in each room's footprint.
- `KrowdKontrolLevel01Test.cpp` now asserts, against the real shipped map, that every
  door resolves a non-null `GatingRoom` and every room auto-discovers a non-empty,
  correctly-counted `OwnedEnemies` — direct regression coverage for the exact gap the
  E2E holdout caught in attempt 1.

`app/`'s `RoomActor.cpp`/`.h` and `EnemyBase.h` carry issue #211's unmerged work
(`EnsureBankingZonesWired()`, `IHerdable`, an existing `BeginPlay()` override) plus
attempt 1's stale, rejected door-gating code (hand-placed `OwnedEnemies`, `ECC_Pawn`
blocking) — both left over in the shared, gitignored `app/` symlink from prior task
runs. This implementation replaced the stale attempt-1 code with the new design while
leaving #211's unrelated work untouched; the `app-source-tracked/` mirror in this PR
contains only this issue's (#218) diff — #211's code is excluded from the tracked
mirror (still present in the real `app/` build target, since it's someone else's
in-flight work).

## Acceptance criteria

- [x] A room's exit door(s) stay impassable (real blocking collision against the
      channel the player pawn actually presents, `ECC_WorldStatic`) until every enemy
      that room owns reaches `EEnemyState::Banked`
- [x] Door opens automatically once the last owned enemy banks
- [x] A door with no resolvable `GatingRoom` stays always open
- [x] An enemy added to an already-cleared room later (e.g. a wave spawn, via
      `AddOwnedEnemy()`) re-gates the door
- [x] Real, shipped levels are wired with zero `.umap` authoring — auto-discovery
      (`ARoomActor::BeginPlay`) and auto-derivation (`ADoorConnectorActor::BeginPlay`)
      populate `OwnedEnemies`/`GatingRoom` from data the levels already have
- [x] No enemy is ever destroyed/killed by this change (Hard Invariant #2) — gating
      only reads `GetEnemyState()`/`IsActorBeingDestroyed()`, no destroy/kill call
      anywhere in the diff
- [x] `python harness/ci.py` (full mode) passes, no regressions

## Validation evidence

```
$ python harness/ci.py --quick
HARNESS_START mode=quick driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=81
GATE_OK mode=quick
```

`KrowdKontrol.Unit.RoomActorDoorGating` (new) individually confirmed 1/1 passing.
`KrowdKontrol.Unit.EnemyBase` flaked once under `harness/ci.py`'s full (build+run) mode
with an unrelated assertion (`KrowdKontrolEnemyBaseTest.cpp:276`, actor-destruction
check, no relation to this diff) and passed cleanly on immediate re-run in isolation —
not reproducible, not touched by this change.

## Deviation note: dynamic-delegate broadcast needs `SetBegunPlay`

`KrowdKontrolRoomActorDoorGatingTest.cpp` needed
`World->InitializeActorsForPlay(FURL()); World->SetBegunPlay(true);` (same pair
`KrowdKontrolRoomActorBankingWiringTest.cpp` already uses) — without it, a bare
`CreateNewMap()` world silently drops `AEnemyBase::OnEnemyBanked`/`AActor::OnDestroyed`
dynamic-multicast-delegate broadcasts between `SpawnActor()`'d actors entirely (proven
empirically: `IsBound()` reports `true` at both bind- and broadcast-time, but the bound
handler is simply never invoked without it). Once the world has begun play,
`SpawnActor()` also auto-dispatches `BeginPlay()` immediately — so `GatingRoom` must be
set *before* a door finishes spawning, via `SpawnActorDeferred()` + `FinishSpawning()`
(the same pattern `KrowdKontrolRoomActorBankingWiringTest.cpp` already established for
the analogous `ARoomActor`/`TargetZones` ordering problem), not a plain `SpawnActor()`
followed by a later property assignment.

## Files

| File | Action |
|------|--------|
| `app/Source/KrowdKontrol/RoomActor.h` | UPDATE |
| `app/Source/KrowdKontrol/RoomActor.cpp` | UPDATE |
| `app/Source/KrowdKontrol/DoorConnectorActor.h` | UPDATE |
| `app/Source/KrowdKontrol/DoorConnectorActor.cpp` | UPDATE |
| `app/Source/KrowdKontrol/EnemyBase.h` | UPDATE |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolRoomActorDoorGatingTest.cpp` | CREATE |
| `app/Source/KrowdKontrol/Private/Tests/LevelStructureTestUtils.h` | UPDATE |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolLevel01Test.cpp` | UPDATE |

## Out of scope (follow-up)

- Wiring `UWaveSpawnerComponent` to call `ARoomActor::AddOwnedEnemy()` for real wave
  spawns — no real wave spawner exists in either shipped level yet; `AddOwnedEnemy()`
  stays available as the public hook.
- Changing `AFlatCamera3DPrototypePawn::MeshComponent`'s own collision profile/object
  type to `Pawn` — wider-blast-radius change (affects overlap detection elsewhere),
  deliberately out of scope; the gate targets the channel the pawn actually presents
  today instead.
- `.umap` binary edits of any kind — this implementation's entire point is to need
  none.
