# Issue #13: Implement RU-NNR enemy: fast-run + drain ray attack, Snare weakness telegraph

Adds `ARunnerEnemy`, the concrete gameplay actor for RU-NNR — the 4th and last of the
4 locked core enemy types (`MISSION.md` Hard Invariant 5) to get a real class. It
mirrors `ASniperEnemy`/`ABomberEnemy`'s shape (mesh + two `UPointLightComponent`s +
`UEnemyTypeIndicatorComponent`, fire-once attack telegraph, `OnControlledEntry`/
`OnAttackEntry` overrides) with RU-NNR's own PRD-driven values: an elongated-cube
"dart" silhouette (`1.8, 0.6, 0.6` scale, deliberate reuse of the last unclaimed
`/Engine/BasicShapes/` primitive), a fast `GetMovementSpeedUnitsPerSecond()` override
(950 u/s, opposite of Bomber's slow 200), a short `GetAttackRangeUnits()` override
(220 u), a Purple `DrainGlowLightComponent` that intensifies only on Snare-triggered
`OnControlledEntry`, and a distinct lime-green attack-tell colour with a 0.6s
fire-once telegraph before `OnRunnerDrainFired` broadcasts.

## Files changed

| File | Action | What it contains |
|------|--------|-------------------|
| `app-source-tracked/Source/KrowdKontrol/EnemyBase.h` | UPDATE | Adds `friend class FKrowdKontrolRunnerEnemyTest;` to the existing friend-class list — the same one-line grant every prior concrete subclass's test required. |
| `app-source-tracked/Source/KrowdKontrol/RunnerEnemy.h` | CREATE | `ARunnerEnemy` declaration: mesh/glow/tell/indicator components, tunable properties, `OnRunnerDrainFired` delegate, protected overrides, friend-test grant. |
| `app-source-tracked/Source/KrowdKontrol/RunnerEnemy.cpp` | CREATE | Constructor (mesh + Purple glow + tell + indicator), `GetAttackRangeUnits()`, `GetMovementSpeedUnitsPerSecond()`, `OnControlledEntry`, `OnAttackEntry`, `AdvanceAttackTelegraph`, `Tick`. |
| `app-source-tracked/Source/KrowdKontrol/Private/Tests/DrainRayFiredTestListener.h` / `.cpp` | CREATE | Test-only `UObject` listener for `OnRunnerDrainFired`, mirrors `SniperShotFiredTestListener.h`/`.cpp`. |
| `app-source-tracked/Source/KrowdKontrol/Private/Tests/KrowdKontrolRunnerEnemyTest.cpp` | CREATE | `KrowdKontrol.Unit.RunnerEnemy` Automation test, cases (a)-(m) plus (g2)/(l2)/(l3), proving every acceptance criterion. |

## Acceptance criteria

- [x] **`ARunnerEnemy` extends `AEnemyBase`.** Structural (class declaration).
- [x] **Placeholder silhouette is visually distinguishable in shape from the other 3
      core enemy types, colour-independent.** Test case (a): distinct mesh path
      (Cube) + scale `(1.8, 0.6, 0.6)` vs. Cone/Sphere/Plane.
- [x] **Visible attack tell precedes the drain-ray firing, distinct from the other 3
      enemies' tells.** Test cases (e)/(f)/(k): tell on before fire, non-reserved and
      non-colliding colour `(0.6, 1.0, 0.2)`.
- [x] **Fast movement, short-range drain-ray attack, matching PRD table.** Test cases
      (j)/(l2)/(l3): `GetAttackRangeUnits() == 220.0f`,
      `GetMovementSpeedUnitsPerSecond() == 950.0f`, genuinely wired into
      `TickChaseMovement`.
- [x] **Snare specifically intensifies/pulses the glow; no other ability does.** Test
      cases (c)/(d): exhaustive over `EAbilitySlot`.
- [x] **Purple used only in its locked information-colour role.**
      `ReservedGameplayColours::GetPurple()` is the sole source of the glow's colour,
      never a literal.
- [x] **Does not implement the Snare ability itself.** `OnControlledEntry` only reads
      `EAbilitySlot`, never mutates ability state; confirmed by the plan's explicit
      scope exclusion.
- [x] **`harness/ci.py` full mode passes with no regressions.** Verified in isolation
      (see Validation evidence below) — blocked as-committed by an unrelated
      pre-existing failure, not this diff.

## Validation evidence

`harness/ci.py` full mode, run with a temporary local patch to unblock an unrelated
pre-existing build break (see below), then reverted: `GATE_OK mode=full`,
`UNIT_PASSED tests=39`, `UE_AUTOMATION_OK`, `E2E_PASSED steps=1`. Includes
`KrowdKontrol.Unit.RunnerEnemy` passing all cases (a)-(m) plus (g2)/(l2)/(l3), with no
regressions in `EnemyBase`, `SniperEnemy`, `BomberEnemy`, `TrooperEnemy`, or
`EnemyTypeIndicatorComponent`. Hard Invariants #2 (no enemy is ever killed), #3
(5-colour lock), and #5 (exactly 4 core enemy types, no net-new) verified — see
`validation.md` for the full breakdown.

**Pre-existing, unrelated blocker (not this issue's to fix):** as committed on disk,
`harness/ci.py` full mode fails at the mandatory module rebuild with
`LNK2019`/`LNK1120` on `KrowdKontrolGameModeTest.cpp`'s use of
`UGameMapsSettings::GetGlobalDefaultGameMode()`, traced to PR #133 (issue #132, still
open/draft) missing the `EngineSettings` module dependency in `KrowdKontrol.Build.cs`.
Confirmed unrelated by file mtimes and by `gh pr diff 133`. Verified this diff's own
code compiles and passes cleanly by temporarily adding the missing dependency, running
the full gate, then reverting `KrowdKontrol.Build.cs` byte-for-byte back to PR #133's
in-flight (broken) state so that PR's own state isn't disturbed. Flagged for whoever
handles PR #133 / issue #132; full-mode `harness/ci.py` will keep reporting
`GATE_FAILED: unit` against the shared `app/` checkout until that lands.
