# Issue #122: Enemy movement/chase system on EnemyBase with per-type speeds

Adds real chase/approach movement to `AEnemyBase`'s state machine. Previously the state
machine was proximity-only: `TickCheckDetection` read distance to decide state
transitions, but nothing moved the enemy actor. `ABomberEnemy`'s `MovementSpeed=200.0f`
(issue #15) was declared but read by nothing. This adds a new overridable hook,
`GetMovementSpeedUnitsPerSecond()` (base default `600.0f`, matching
`UCharacterMovementComponent`'s engine default), and a private `TickChaseMovement` step
wired into `Tick()` that walks the actor straight toward the player at that speed —
only while `CurrentState == Alert` — clamped to never overshoot past the player in one
tick. `ABomberEnemy` overrides the hook to return its existing `MovementSpeed=200.0f`.
`ASniperEnemy` deliberately keeps the base default (no speed adjective in its PRD row).

## Files changed

| File | Action | What it contains |
|------|--------|-------------------|
| `EnemyBase.h`/`.cpp` | UPDATE | `GetMovementSpeedUnitsPerSecond()` hook, private `TickChaseMovement` decl+impl, wired into `Tick()` |
| `BomberEnemy.h`/`.cpp` | UPDATE | `GetMovementSpeedUnitsPerSecond()` override returns `MovementSpeed`; stale "not wired to a live nav system" comment corrected |
| `Private/Tests/EnemyBaseTestActor.h`/`.cpp` | UPDATE | Constructor adds a `USceneComponent` root so `SetActorLocation` has somewhere to write |
| `Private/Tests/KrowdKontrolEnemyBaseTest.cpp` | UPDATE | Cases (o)-(t): base default speed, Alert-only gating (Idle/Attack no-op), no-overshoot clamp, real `Tick()` wiring |
| `Private/Tests/KrowdKontrolBomberEnemyTest.cpp` | UPDATE | Cases (l2)-(l3): override returns/drives `MovementSpeed`, distinct from base default |

`ASniperEnemy` deliberately left unchanged — no override, per PRD 03 (no speed
adjective for SN-1PR).

## Acceptance criteria

- [x] Movement is built into the shared `AEnemyBase` state machine, not bolted onto one
      subclass (`TickChaseMovement`, `EnemyBase.cpp`).
- [x] Per-type speed is actually read and applied, not just declared: B0-0MR moves at
      its own `MovementSpeed=200.0f` via the `GetMovementSpeedUnitsPerSecond()`
      override, proven distinct from the `600.0f` base default (cases l2/l3).
- [x] RU-NNR-style fast-run enemies get their own rate for free via the same override
      hook — no new movement system needed per enemy type.
- [x] Movement only happens while actively pursuing (`CurrentState == Alert`); Idle and
      Attack/Controlled/Banked are no-ops (cases p/r).
- [x] No overshoot past the player within a single tick (case s).
- [x] Straight-line walk-to, no pathfinding/navmesh, per PRD 03 REQ-2.
- [x] Real `Tick()` wiring proven with a live `UWorld` (case t), not just the
      friend-tested step in isolation.
- [x] `python harness/ci.py` (full) reports `GATE_OK mode=full`, zero regressions.

## Validation evidence

```
$ python harness/ci.py --quick
UNIT_PASSED tests=31
GATE_OK mode=quick

$ python harness/ci.py
UNIT_PASSED tests=31
APP_STARTED
UE_AUTOMATION_OK (passed=1 total=1)
E2E_PASSED steps=1
GATE_OK mode=full
```

Gate was green on first run; no fixes were needed during validation. Hard invariants
reviewed by inspection: no-kill rule (#2) untouched — movement-only, no damage/kill
logic; roster (#5) untouched — no new enemy class, only an override on the existing
`ABomberEnemy`; colour lock/ability roster (#3/#4) untouched — no colour or ability code
in this diff.
