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

## Acceptance criteria

- [x] A room's exit door(s) stay impassable (real blocking collision, not just
      visual) until every enemy that room owns reaches `EEnemyState::Banked` —
      `ADoorConnectorActor::RefreshGateState()` / `GateBlockingComponent`
- [x] Door opens automatically once the last owned enemy banks —
      `ARoomActor::OnRoomClearedStateChanged` → `HandleGatingRoomClearedStateChanged()`
      → `RefreshGateState()`; covered by `KrowdKontrolRoomActorDoorGatingTest.cpp` Test 2
- [x] A door with no `GatingRoom` configured (e.g. the level's first door) stays always
      open — `RefreshGateState()`'s `GatingRoom == nullptr` short-circuit; Test setup
      relies on this default implicitly (`bIsGateOpen` inits `true`)
- [x] An enemy added to an already-cleared room later (e.g. a wave spawn) re-gates the
      door — `AddOwnedEnemy()` broadcasts `OnRoomClearedStateChanged` immediately;
      covered by Test 3
- [x] No enemy is ever destroyed/killed by this change (Hard Invariant #2) — gating
      only reads `GetEnemyState()`, no destroy/kill call anywhere in the diff
      (`validation.md` Phase 3)
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

Individually confirmed passing: `KrowdKontrol.Unit.RoomActorDoorGating` (new, 1/1),
`KrowdKontrol.Unit.RoomActor` (3/3), `KrowdKontrol.Unit.DoorConnectorActor` (1/1),
`KrowdKontrol.Unit.EnemyBase` (2/2), `KrowdKontrol.Unit.RoomActorBankingWiring`
(pre-existing #211 test in `app/`, no regression, 1/1).

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
