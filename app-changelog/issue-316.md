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
unchanged). Mirrors the MID-tint pattern issue #317 (PR #347) established on
`APlaceholderTargetZoneActor::ApplyChainColour()`.

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
- [x] `UControlledDurationIndicatorComponent`'s Controlled-state bar remains a
      physically separate mesh, offset 190 units above the body
      (`BarHeightOffset`) — geometrically non-overlapping with the body regardless of
      the body's own material. Claude's own MCP tooling has no primitive to drive an
      enemy into `Controlled` state (no ability-cast input primitive exists per this
      project's established holdout limitations), so this item is verified by
      inspection/reasoning only and flagged as a follow-up for a human/operator
      playtest pass, not an automated check.
- [x] Elite variants untouched — no new code path references `bIsElite` or any Elite
      accessor.
- [x] Placeholder-first — no new art asset added; reuses
      `/Game/_Placeholder/Abilities/M_AbilityIndicator`.
- [x] `harness/ci.py --quick` reports `GATE_OK` (full-mode gate is re-run by the
      `dark-factory-validate` node).
- [x] `app/` and `app-source-tracked/` copies of `EnemyBase.h`/`.cpp` (and the 5
      touched test files, already part of the tracked mirror from prior issues) are
      byte-identical, confirmed via `diff`.

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
