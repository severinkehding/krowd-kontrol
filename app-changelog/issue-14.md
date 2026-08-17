# Issue #14: Implement TR-UPR enemy: rapid single rays attack, Root weakness telegraph

Adds `ATrooperEnemy` (codename TR-UPR), the 4th and final core enemy type from PRD
03's roster. It extends `AEnemyBase` (issue #12) with a medium-range attack that
fires **repeatedly** while `Attack` persists — the one mechanical trait that sets it
apart from `ASniperEnemy`/`ABomberEnemy`, which each fire exactly once per attack
episode. It gets a distinct Plane-mesh silhouette (the last unclaimed
`/Engine/BasicShapes/` primitive), a Teal glow that intensifies only on
`EAbilitySlot::Root`, and its own attack-tell colour, distinct from both siblings
and all 5 reserved gameplay colours.

## Files changed (all under `app/`, gitignored per D-003 — mirrored below per D-009)

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/TrooperEnemy.h` | CREATE | `ATrooperEnemy` declaration: mesh/glow/tell components, `FOnTrooperRayFired` delegate, telegraph state, `GetAttackRangeUnits`/`OnControlledEntry`/`OnAttackEntry`/`Tick` overrides |
| `app/Source/KrowdKontrol/TrooperEnemy.cpp` | CREATE | Implementation: Plane-mesh silhouette, Teal glow (Root-only intensify), magenta attack tell, 700.0f medium attack range, self-re-arming `AdvanceAttackTelegraph` (no fire-once guard) |
| `app/Source/KrowdKontrol/Private/Tests/TrooperRayFiredTestListener.h`/`.cpp` | CREATE | Dynamic-delegate test listener for `OnTrooperRayFired`, mirroring `SniperShotFiredTestListener` |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolTrooperEnemyTest.cpp` | CREATE | `KrowdKontrol.Unit.TrooperEnemy` — 13 cases (a-m) covering silhouette, glow, tell, rapid re-fire, range, colour non-collision, controlled-interrupt, and real-`Tick()` wiring |

No existing file needed modification — `EEnemyType::TR_UPR` already existed and
`ARoomActor`/`UWaveSpawnerComponent` already accept any `EEnemyType` value
generically.

## Acceptance criteria

- [x] `ATrooperEnemy` extends `AEnemyBase`
- [x] Placeholder Plane-mesh silhouette, scaled/rotated distinctly from
      Cube/Cylinder/Cone/Sphere (test case a)
- [x] Visible attack tell precedes each ray, colour distinct from Sniper's and
      Bomber's tells and from all 5 reserved colours (test cases e/f/k)
- [x] Medium-range, rapid single-ray attacks: `GetAttackRangeUnits()` (700.0f)
      strictly between Bomber's 150.0f and Sniper's 1400.0f; telegraph fires
      repeatedly (not once) while `Attack` persists (test cases g/j)
- [x] `ReceiveControl(EAbilitySlot::Root)` — and only Root — intensifies the Teal
      glow (test cases c/d, exhaustive over all `EAbilitySlot` values)
- [x] Teal used only via `ReservedGameplayColours::GetTeal()`, never a hardcoded
      literal (Hard Invariant 3)
- [x] Does not implement Root itself — only reads the `EAbilitySlot` parameter the
      base class's existing `ReceiveControl` hook already provides
- [x] `app-source-tracked/` mirror is byte-identical to the `app/` source

## Validation

```
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=34
APP_STARTED driver=cli
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
HOLDOUT_ABSENT no .factory/holdout/run.py - NOTHING above the independence line ran.
MUTATIONS_ABSENT no harness/mutations/run.py - this gate has never been shown to fail.
GATE_OK mode=full
```

Gate passed clean on the first run, no fixes needed. Hard invariants reviewed by
inspection: no damage/kill logic (#2), Teal used exclusively via
`ReservedGameplayColours::GetTeal()` and intensified only on Root (#3), TR-UPR fills
an already-enumerated roster slot rather than adding a 5th type (#5), and the diff
lands entirely under `app-source-tracked/` source copies (#8).

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
