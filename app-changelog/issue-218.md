# Issue #218: Gate room exit doors until every owned enemy is Banked

**Type**: bug

## Summary

`ARoomActor` and `ADoorConnectorActor` had no gating mechanic at all — doors were
purely decorative (a walkable floor strip + a visual marker light), so a player could
walk through any door regardless of what was still alive in the room behind them. This
caused a live snowball-aggro incident (Bombers converging on the player from across the
level). Fixed by adding owned-enemy tracking to `ARoomActor` (`OwnedEnemies`,
`AddOwnedEnemy()`, `IsRoomCleared()`, `OnRoomClearedStateChanged`) and a real physical
gate to `ADoorConnectorActor` (`GatingRoom`, `bIsGateOpen`, `GateBlockingComponent`,
`RefreshGateState()`). A door with no `GatingRoom` assigned defaults open (untouched
entry doors); a gated door's `GateBlockingComponent` collision flips between
`QueryOnly` (closed) and `NoCollision` (open) as the room's owned enemies bank.

`app/`'s `RoomActor.cpp`/`.h` and `EnemyBase.h` already carried issue #211's unmerged
changes (`EnsureBankingZonesWired()`, `IHerdable`, an existing `BeginPlay()` override).
This issue's binding logic was integrated into that existing `BeginPlay()` rather than
adding a second override, and the `app-source-tracked/` mirror in this PR was
hand-reconciled to contain only this issue's (#218) diff — #211's code is excluded.

Post-review follow-up (self-fix pass): `GateBlockingComponent` now resets to
`ECR_Ignore` on all channels before scoping `ECC_Pawn` to `ECR_Block`, so a closed gate
no longer leaves every other channel (camera, visibility, world-dynamic traces, etc.)
at the engine's default `BlockAllDynamic` response. Owned enemies also now bind
`AActor::OnDestroyed` (in addition to `OnEnemyBanked`), so a room whose last un-banked
enemy is destroyed by something other than banking still re-broadcasts
`OnRoomClearedStateChanged` and its door actually opens, instead of soft-locking on the
read path only. See `KrowdKontrolRoomActorDoorGatingTest.cpp` Tests 1/3 (channel
scoping) and Test 6 (destroyed-enemy re-open) for coverage.

`IsRoomCleared()` also now checks `Enemy->IsActorBeingDestroyed()`, not just
`IsValid(Enemy)` — caught by Test 6 initially failing: `AActor::OnDestroyed` broadcasts
synchronously from inside `UWorld::DestroyActor()` *before* the actor is marked
garbage, so `HandleOwnedEnemyDestroyed()`'s re-broadcast was triggering a re-check of
`IsRoomCleared()` while the dying enemy was still `IsValid()` and still un-banked —
the door's `RefreshGateState()` read stale "still blocked" state and never got another
chance to re-run. `IsActorBeingDestroyed()` is true for the whole synchronous duration
of `Destroy()`, so it correctly excludes the enemy from blocking even at that point.

### Interpretation calls

📋 **AC3 vs AC4/Test 3**: AC3 ("entry doors, once opened, never re-close behind the
player") and AC4/Test 3 ("wave-spawned addition to an already-open room re-gates the
door") read as contradictory in isolation. Resolved as: gate state is driven by exactly
one signal (room ownership/`OnRoomClearedStateChanged`) and nothing else — no
player-position tracking exists in this codebase, and building one is out of scope. AC3
is read as restating AC4 from the opposite angle ("nothing *other than* ownership
re-closes a door"), not as a competing invariant. Carried forward here from the issue
#218 investigation comment for reviewer visibility; flagged explicitly in case the
intended reading was a real traversal-latch — that would be materially larger scope.

## Acceptance criteria

- [x] A room's exit door(s) stay impassable (real blocking collision, not just
      visual) until every enemy that room owns reaches `EEnemyState::Banked` —
      `ADoorConnectorActor::RefreshGateState()` / `GateBlockingComponent`
- [x] Door opens automatically once the last owned enemy banks —
      `ARoomActor::OnRoomClearedStateChanged` → `HandleGatingRoomClearedStateChanged()`
      → `RefreshGateState()`; covered by `KrowdKontrolRoomActorDoorGatingTest.cpp` Test 2
- [x] A door with no `GatingRoom` configured (e.g. the level's first door) stays always
      open — `RefreshGateState()`'s `GatingRoom == nullptr` short-circuit; covered
      directly by Test 4 (spawns a door with `GatingRoom` left `nullptr` and asserts
      both `bIsGateOpen` and the real collision state after a live `BeginPlay()` pass)
- [x] An enemy added to an already-cleared room later (e.g. a wave spawn) re-gates the
      door — `AddOwnedEnemy()` broadcasts `OnRoomClearedStateChanged` immediately;
      covered by Test 3
- [x] The primary real-usage path — enemies hand-placed in `OwnedEnemies` before play
      starts, bound via `RoomActor::BeginPlay()`'s loop rather than `AddOwnedEnemy()` —
      is gated correctly; covered by Test 5 (`SpawnActorDeferred`+pre-populated
      `OwnedEnemies`+`FinishSpawning()`)
- [x] A destroyed (not banked) owned enemy doesn't block `IsRoomCleared()` and doesn't
      soft-lock the door — `IsValid()` guard in `IsRoomCleared()` plus the new
      `OnDestroyed` re-broadcast in `BindOwnedEnemyDelegate()`; covered by Test 6
- [x] No enemy is ever destroyed/killed by this change (Hard Invariant #2) — gating
      only reads `GetEnemyState()`, no destroy/kill call anywhere in the diff
      (`validation.md` Phase 3; `Enemy->Destroy()` in Test 6 is test-only simulation of
      an external destruction source, not gameplay code destroying an enemy)
- [x] `python harness/ci.py` (full mode) passes, no regressions — `GATE_OK mode=full`,
      `UNIT_PASSED tests=81`

## Validation evidence

```
$ python harness/ci.py
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=81
APP_STARTED driver=cli
UE_BUILD_START KrowdKontrolEditor Win64 Development
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
HOLDOUT_ABSENT no .factory/holdout/run.py
MUTATIONS_ABSENT no harness/mutations/run.py
GATE_OK mode=full
```

Individually confirmed passing: `KrowdKontrol.Unit.RoomActorDoorGating` (new, now 6
sub-tests in 1 automation test, 1/1), `KrowdKontrol.Unit.RoomActor` (3/3),
`KrowdKontrol.Unit.DoorConnectorActor` (1/1), `KrowdKontrol.Unit.EnemyBase` (2/2),
`KrowdKontrol.Unit.RoomActorBankingWiring` (pre-existing #211 test in `app/`, no
regression, 1/1). Re-ran `python harness/ci.py` after the self-fix pass above —
`GATE_OK mode=full`, `UNIT_PASSED tests=81`, `UE_AUTOMATION_RESULT passed=1 total=1`.

Full detail in `implementation.md` and `validation.md` for this run
(`artifacts/runs/264ad40b592a41eba61d28c97be10c68/`).

## Files

| File | Action |
|------|--------|
| `app/Source/KrowdKontrol/RoomActor.h` | UPDATE |
| `app/Source/KrowdKontrol/RoomActor.cpp` | UPDATE |
| `app/Source/KrowdKontrol/DoorConnectorActor.h` | UPDATE |
| `app/Source/KrowdKontrol/DoorConnectorActor.cpp` | UPDATE |
| `app/Source/KrowdKontrol/EnemyBase.h` | UPDATE |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolRoomActorDoorGatingTest.cpp` | CREATE |

## Out of scope (follow-up)

- Wiring `UWaveSpawnerComponent` to call `ARoomActor::AddOwnedEnemy()` for real wave
  spawns — satisfied at the `ARoomActor`/`ADoorConnectorActor` level; connecting the
  real spawner is separate follow-up work.
- `.umap` level-data changes to actually set `GatingRoom` on real doors in
  `L_Level01`/`L_Level02` — binary asset edits, not part of this C++ change.
