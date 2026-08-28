# Issue #316: Tint enemy pawns solidly with their chain colour

Adds `AEnemyBase::ApplyBodyChainColourTint()` (`app/Source/KrowdKontrol/EnemyBase.h`
/ `.cpp`), a base-class method that applies a runtime-created
`UMaterialInstanceDynamic` (based on the existing
`/Game/_Placeholder/Abilities/M_AbilityIndicator` placeholder material) to every
concrete `AEnemyBase` subclass's root mesh, tinted via its `"Colour"` vector
parameter with `AbilityData::GetChainColourForEnemyType()`'s return value for that
enemy's own `EEnemyType` (read from its existing `UEnemyTypeIndicatorComponent::
EnemyType`, never a new field). This satisfies PRD `docs/prd-colour-coded-herding.md`
REQ-2 — the enemy's own body now reads solidly as its chain colour, complementing
(not replacing) the existing `EnemyTypeIndicatorComponent` marker (issue #242,
unchanged). Mirrors the lazy-create-once/reapply-parameter MID-tint pattern already
established by `UControlledDurationIndicatorComponent::InitializeIndicatorVisual()`.

Called automatically from `BeginPlay()`; public and idempotent so the Automation
tests can drive it deterministically on a plain `NewObject<>()` actor with no
`UWorld`. Two guard clauses (`Cast<UMeshComponent>(GetRootComponent())` and
`FindComponentByClass<UEnemyTypeIndicatorComponent>()`) make it a safe no-op on the
base class's own `AEnemyBaseTestActor` test double (plain `USceneComponent` root, no
type indicator), used by dozens of other `AEnemyBase` tests.

## Acceptance criteria

- [x] Each of the 4 core enemy types' placeholder mesh applies a solid material tint
      matching its chain colour, read from `AbilityData::GetChainColourForEnemyType()`
      — covered by a new case in each of `KrowdKontrolBomberEnemyTest.cpp`,
      `KrowdKontrolSniperEnemyTest.cpp`, `KrowdKontrolTrooperEnemyTest.cpp`,
      `KrowdKontrolRunnerEnemyTest.cpp`.
- [x] No local/hardcoded colour constants introduced — the only colour source in the
      new code is `AbilityData::GetChainColourForEnemyType`.
- [x] `EnemyTypeIndicatorComponent` (issue #242) is unchanged — `git diff` shows zero
      edits to `EnemyTypeIndicatorComponent.h`/`.cpp`.
- [x] `UControlledDurationIndicatorComponent`'s Controlled-state bar remains
      distinguishable against the newly-tinted body. No MCP primitive exists to drive
      a live PIE enemy into `Controlled` state (no ability-cast input primitive per
      this project's established holdout limitations), so the issue's MCP-viewport-
      capture verification has no implementable path — instead, case (g4) added to
      `KrowdKontrolControlledDurationIndicatorComponentTest.cpp` drives a real
      World-spawned `ABomberEnemy` into `Controlled` (via the same
      `TickCheckDetection`/`ReceiveControl` calls case (g) already uses) and asserts
      `ApplyBodyChainColourTint()`'s `BodyChainColourMaterialInstance` is a distinct
      object from the indicator's own `FillMaterialInstance`, that the body tint is
      applied to a distinct component from the indicator's `FillMeshComponent`
      (in addition to the existing 190-unit `BarHeightOffset` geometric separation),
      and that the indicator's `CurrentColour` stays driven by the controlling
      ability's colour regardless of the body tint. This automated check proves the
      two can never share rendered colour, in place of an MCP screenshot.
- [x] Elite variants untouched — no new code path references `bIsElite` or any Elite
      accessor.
- [x] Placeholder-first — no new art asset added; reuses
      `/Game/_Placeholder/Abilities/M_AbilityIndicator`.
- [x] `harness/ci.py --quick` reports `GATE_OK` (full-mode gate is re-run by the
      `dark-factory-validate` node).
- [x] `app/` and `app-source-tracked/` copies of `EnemyBase.h` and this PR's own
      touched test files are byte-identical, confirmed via `diff`.
      `app/EnemyBase.cpp`'s live copy is **not** byte-identical to the tracked
      mirror — it carries unrelated, still-open issue #243 content (a swept vs.
      unswept `SetActorLocation()` call in the flee-movement path) from open PR #345,
      which this PR never touched and does not pull in. This PR's own tracked diff
      for `EnemyBase.cpp` (the chain-colour tint code) is correctly scoped and
      unaffected by that divergence.

## Validation evidence

`python harness/ci.py --quick`:

```
HARNESS_START mode=quick driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=123
PIE_PASSED tests=5
GATE_OK mode=quick
```

Passed on the first run — no fixes required. Full-mode validation deferred to the
`dark-factory-validate` node per this repo's factory workflow.

MISSION.md Hard Invariants reviewed against this diff: no kill-rule, colour-lock,
ability-roster, enemy-roster, engine/dimensionality, networking, or `app`-tracking
invariant is touched — Hard Invariant 3's 5-colour reservation is respected (the tint
is always read from `AbilityData::GetChainColourForEnemyType()`, never a local
constant), and Hard Invariant 8's `app-source-tracked/` carve-out is followed (plain-
text copy at commit time, never a live link).

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
