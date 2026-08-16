# Issue #12: Base Enemy AI state machine

Verify-and-mirror issue, not a behavior change. `AEnemyBase` — the shared
`Idle -> Alert -> Attack -> Controlled -> Banked` state machine every core enemy type
(PRD 03) extends — already exists, is already correct, and already works in `app/`.
It was first built alongside SN-1PR (issue #17) in the combined PR #115, which passed
the full validation gate (`GATE_OK`, 27/27 unit tests) but was rejected solely for
exceeding the 500-line PR cap (898 lines total). This PR does not change `AEnemyBase`'s
behavior at all — it mirrors the already-correct, already-validated `app/` source into
`app-source-tracked/` for the first time, split out from #17 so each PR fits the cap.

## Files changed

| File | Action | Contains |
|------|--------|----------|
| `EnemyBase.h` | CREATE | `EEnemyState` enum + abstract `AEnemyBase` declaration |
| `EnemyBase.cpp` | CREATE | State machine transition logic, tick-driven detection |
| `Private/Tests/EnemyBaseTestActor.h/.cpp` | CREATE | Test-only concrete subclass exposing base hooks |
| `Private/Tests/EnemyBankedTestListener.h/.cpp` | CREATE | Dynamic-delegate listener for `OnEnemyBanked` |
| `Private/Tests/KrowdKontrolEnemyBaseTest.cpp` | CREATE | `KrowdKontrol.Unit.EnemyBase`, 12 cases (a)-(l) |

## Acceptance criteria

- [x] State enum + documented transition table: `EEnemyState` (`EnemyBase.h:18-25`),
      table in the header comment (`EnemyBase.h:8-16`).
- [x] Idle->Alert via proximity check, no perception/pathfinding:
      `TickCheckDetection` (`EnemyBase.cpp:56-70`).
- [x] Alert->Attack after per-type-overridable range: `GetAttackRangeUnits()`
      (`EnemyBase.h:83`), `AdvanceToAttack` (`EnemyBase.cpp:46-54`).
- [x] Public `ReceiveControl` hook, Alert/Attack->Controlled only, no CC logic of its
      own: `EnemyBase.cpp:14-22`.
- [x] Public `TransitionToBanked` hook, Controlled->Banked only, no delivery logic of
      its own, flip-before-broadcast re-entrancy safety: `EnemyBase.cpp:24-35`.
- [x] No group tactics/flanking: not implemented anywhere in this class.
- [x] Automation test confirms Idle->Alert->Attack in isolation:
      `KrowdKontrol.Unit.EnemyBase`, case (f) and friends.

## Validation evidence

Already-validated: PR #115's full gate run (`harness/ci.py`, full mode) reported
`GATE_OK` with `UNIT_PASSED tests=27`, including `KrowdKontrol.Unit.EnemyBase`, and
all applicable MISSION.md hard invariants verified (Hard Invariant 2: no `Destroy()`
call anywhere in the state machine — `Banked` is terminal by convention only). This PR
mirrors the identical, unmodified source; re-run the full gate before merge.

## Deviations from plan

None — mirrors `app/` verbatim, split from the originally-combined #12+#17 attempt per
both issues' own Notes anticipating this exact split.
