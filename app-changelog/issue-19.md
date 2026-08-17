# Issue #19: Elite enemy variants (recoloured, stat-bumped reskins of the 4 core types)

Per PRD 03 REQ-4 (P1) and MISSION.md Hard Invariant 5 ("plus elite reskins of those
same 4... no net-new enemy types"), adds an "Elite" configuration to the 4 existing
core enemy classes (`ARunnerEnemy`, `ATrooperEnemy`, `ABomberEnemy`, `ASniperEnemy`)
rather than writing 4 new enemy classes or a 5th `EEnemyType`. `AEnemyBase` gains a
shared `bIsElite` flag, an `EliteMovementSpeedMultiplier` layered on top of each
type's existing `GetMovementSpeedUnitsPerSecond()` override via a new
`GetEffectiveMovementSpeedUnitsPerSecond()` (now what `TickChaseMovement` actually
calls), and `SetIsElite(bool)` to toggle both together. Each concrete subclass gets
its own `EliteTrimLightComponent` (a second `UPointLightComponent`, mirroring the
existing `AttackTellLightComponent` pattern) and overrides a new protected virtual
`GetEliteTrimLightComponent()` so the shared `SetIsElite()` can reach it
polymorphically. A new `EliteEligibility::IsEligibleAtLevel(int32)` pure function
(mirroring `UAbilityUnlockComponent`/`UOvercrowdDetectionComponent`'s "no real
level-progression subsystem yet, caller supplies LevelIndex explicitly" pattern) is
the single source of truth for the level-4+ eligibility gate. `FWaveEntry` gets a new
`bIsElite` field so `UWaveSpawnerComponent::SpawnWave()` can actually produce an Elite
instance through the real spawn path.

## Files changed

| File | Action | What it contains |
|------|--------|-------------------|
| `EliteEligibility.h`/`.cpp` | CREATE | `IsEligibleAtLevel(int32)` pure function, `MinEligibleLevel = 4` |
| `EnemyBase.h`/`.cpp` | UPDATE | `bIsElite`, `EliteMovementSpeedMultiplier`, `EliteTrimIntensity`, `SetIsElite()`, `GetEffectiveMovementSpeedUnitsPerSecond()`, protected virtual `GetEliteTrimLightComponent()` (base default `nullptr`); `TickChaseMovement` now calls the effective-speed method |
| `RunnerEnemy.h`/`.cpp`, `TrooperEnemy.h`/`.cpp`, `BomberEnemy.h`/`.cpp`, `SniperEnemy.h`/`.cpp` | UPDATE | Each gets its own `EliteTrimLightComponent` property, created inline in the constructor (same shape as `AttackTellLightComponent`), and overrides `GetEliteTrimLightComponent()` |
| `WaveSpawnerComponent.h` | UPDATE | `FWaveEntry.bIsElite` flag |
| `WaveSpawnerComponent.cpp` | UPDATE | `SpawnWave()` casts the spawned actor to `AEnemyBase` and calls `SetIsElite(true)` when the entry is flagged, warning (not blocking) if the class isn't an `AEnemyBase` |
| `Private/Tests/EnemyBaseTestActor.h`/`.cpp` | UPDATE | Own `EliteTrimLightComponent` + override, same shape as the 4 concrete types, so base-level Elite tests can exercise it |
| `Private/Tests/KrowdKontrolEnemyBaseTest.cpp` | UPDATE | New cases: default `bIsElite`/effective speed, `SetIsElite(true/false)` toggles trim light + speed together, trim colour non-reserved |
| `Private/Tests/KrowdKontrolRunnerEnemyTest.cpp`, `.../TrooperEnemyTest.cpp`, `.../BomberEnemyTest.cpp`, `.../SniperEnemyTest.cpp` | UPDATE | One elite-trim-light smoke case each (exists, attached to `MeshComponent`, non-reserved colour) |
| `Private/Tests/KrowdKontrolWaveSpawnerComponentTest.cpp` | UPDATE | New cases: `bIsElite = true` wave entry produces a spawned `AEnemyBase` with `bIsElite == true`; `bIsElite = false` (default) leaves it unaffected |
| `Private/Tests/KrowdKontrolEliteEligibilityTest.cpp` | CREATE | Pure-function test: levels 1-3 false, 4/5/100 true, boundary at `MinEligibleLevel` |

## Design note: why `EliteTrimLightComponent` is a per-subclass property, not a shared one

The plan's original design put `EliteTrimLightComponent` as a single `UPROPERTY`
declared once on `AEnemyBase`, created via a shared
`AEnemyBase::InitializeEliteTrimLightComponent()` helper called from each of the 4
subclass constructors. This does **not** work reliably in UE 5.8: running the full
`KrowdKontrol.Unit.*` suite (many `NewObject<T>()` instances across 4+ sibling
classes constructed in the same process) intermittently produced a null component, a
wrong attach-parent, or — with the shared helper — a hard
`EXCEPTION_ACCESS_VIOLATION` crash inside the Automation Framework. Isolated
per-class runs never showed it. Splitting `EliteTrimLightComponent` into its own
`UPROPERTY` per concrete subclass (created inline in each constructor, exactly like
`AttackTellLightComponent`), with `AEnemyBase` reaching it through a protected virtual
`GetEliteTrimLightComponent()` accessor, was tried next and still intermittently
failed the same way.

The actual root cause turned out to be unrelated to the component's declaration shape
entirely: the new test cases asserted a property on the *original*, first-constructed
`Runner`/`Bomber`/`Sniper`/`Trooper` local variable at the very *end* of each test
function, after dozens more `NewObject<T>()` instances and several
`FAutomationEditorCommonUtils::CreateNewMap()` calls (which run `CollectGarbage()`)
had already executed in between. A `NewObject<T>()`-constructed actor held only by a
local C++ pointer has no GC roots; diagnostic logging confirmed the actor itself
survived (same `this` pointer at construction and at the assertion) while its
`EliteTrimLightComponent` UPROPERTY read back as an address at construction and
exactly `nullptr` at the assertion — the signature of GC reclaiming the referenced
subobject. No pre-existing assertion in this codebase checked a property this late in
a test function's lifetime, which is why this class of bug never surfaced before.
Moving each new Elite-trim-light assertion to run immediately after the existing
early-position checks (mirroring where every other `Runner`/`Bomber`/`Sniper`/
`Trooper` property is already asserted, right after construction) fixed it
completely — confirmed stable across multiple repeated full-suite runs. The
per-subclass-property design was kept (it's the more conventional shape, matching
every other type-tied component in this codebase) even though the shared-property
design would likely also work correctly now that the tests no longer trigger the
false alarm.

## Acceptance criteria

- [x] Each of the 4 core enemy types can be spawned in an "Elite" configuration
      without a new class - one `EliteTrimLightComponent` smoke test per type plus
      `UWaveSpawnerComponent`'s `bIsElite` wiring test.
- [x] Elite trim colour is visually distinct and not one of the 5 reserved colours -
      `TestFalse(...GetAll().Contains(...))` in every new test case.
- [x] Elite applies exactly one configurable stat bump (movement speed) -
      `GetEffectiveMovementSpeedUnitsPerSecond()`'s multiplier test.
- [x] Elite variants retain silhouette/attack-tell/weakness-counter - no changes to
      any of the 4 types' mesh, tell-light, or `OnControlledEntry` glow-intensify
      logic in this PR.
- [x] No 5th enemy type introduced - `EEnemyType` (`EnemyType.h`) untouched.
- [x] Spawn eligibility gated by level number, not NG+/meta-progression -
      `EliteEligibility::IsEligibleAtLevel(int32)` is the sole gate, tested directly,
      no save/meta-progression dependency.
- [x] Validation commands pass with exit 0 / `GATE_OK`.
- [x] No regressions in any pre-existing `KrowdKontrol.Unit.*` test.

## Validation evidence

```
$ python harness/ci.py --quick
UNIT_PASSED tests=46
GATE_OK mode=quick

$ bash harness/run_ue_automation.sh "KrowdKontrol.Unit."
UE_AUTOMATION_RESULT passed=46 total=46
UE_AUTOMATION_OK

$ python harness/ci.py
UNIT_PASSED tests=46
E2E_PASSED steps=1
GATE_OK mode=full
```

Rerun 3 times consecutively after the fix (the failure mode above was intermittent,
not deterministic) — all green, `passed=46 total=46` every time. Hard invariants
reviewed by inspection: no-kill rule (#2) untouched; roster (#5) untouched — no new
`EEnemyType` value, no new `AActor` subclass, only a flag + a few members on the
existing 4 classes; colour lock (#3) untouched — `EliteTrimLightComponent`'s colour is
asserted distinct from `ReservedGameplayColours::GetAll()` by test, not just comment.
