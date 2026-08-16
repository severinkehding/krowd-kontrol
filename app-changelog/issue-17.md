# Issue #17: SN-1PR sniper enemy

Verify-and-mirror issue, not a new design. `ASniperEnemy` (SN-1PR), the first
concrete `AEnemyBase` (issue #12) subclass, already exists, is already correct, and
already works in `app/`. It first landed alongside `AEnemyBase` in the combined PR
#115 (rejected for exceeding the 500-line cap, 898 lines), then alone in PR #117
(rejected for a real test bug: case g2 asserted exact IEEE-754 float32 equality that
float32 arithmetic does not produce). That bug is fixed in the source mirrored here -
the accumulation test now sums three partial advances (0.5f + 0.4f + 0.5f = 1.4)
safely past the 1.2s boundary instead of landing exactly on it. This PR mirrors the
already-correct, already-validated `app/` source into `app-source-tracked/`, split
from `EnemyBase.h`/`KrowdKontrolEnemyBaseTest.cpp` (already merged via #116) to stay
under the cap.

## Files changed

| File | Action | Contains |
|------|--------|----------|
| `SniperEnemy.h` | CREATE | `ASniperEnemy` declaration |
| `SniperEnemy.cpp` | CREATE | Cone silhouette, Blue eye-glow, attack tell + telegraph |
| `Private/Tests/KrowdKontrolSniperEnemyTest.cpp` | CREATE | `KrowdKontrol.Unit.SniperEnemy`, 13 cases (a)-(m) |
| `Private/Tests/SniperShotFiredTestListener.h/.cpp` | CREATE | Dynamic-delegate listener for `OnSniperShotFired` |

## Acceptance criteria

- [x] Extends `AEnemyBase`: `SniperEnemy.h:21`.
- [x] Distinct cone silhouette, independent of colour: `SniperEnemy.cpp:9-25`, case (a).
- [x] Attack tell precedes the shot, distinct from other enemies' tells:
      `SniperEnemy.cpp:77-100`, cases (e)-(h).
- [x] Long-range attack behavior: `GetAttackRangeUnits() = 1400.0f`
      (`SniperEnemy.cpp:48-57`), case (j).
- [x] Sleep-specific glow intensify via `OnControlledEntry`, no response to any other
      ability: `SniperEnemy.cpp:59-75`, cases (c)-(d).
- [x] Blue used only as the locked information colour: `SniperEnemy.cpp:29-31`,
      `ReservedGameplayColours::GetBlue()`.
- [x] Does not implement Sleep itself - only reads `EAbilitySlot Ability` via the base
      class's existing hook.

## Validation evidence

`harness/ci.py` full mode: `GATE_OK`, `UNIT_PASSED tests=27`,
`UE_AUTOMATION_RESULT passed=1 total=1` for `KrowdKontrol.Unit.SniperEnemy`. Hard
Invariant 3 (Blue reserved for information only) and Hard Invariant 5 (no group
tactics) verified by inspection - this class contains neither flanking nor
coordination logic.

## Deviations from plan

None - mirrors `app/`'s already-validated source verbatim.
