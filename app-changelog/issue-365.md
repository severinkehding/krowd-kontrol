# Issue #365: Add ground-ring visual indicator showing each pylon's real banking-zone radius

## Summary

Adds a persistent, non-occluding translucent ring on the ground around each pylon
(`APlaceholderTargetZoneActor`), showing the real banking-overlap radius read live from
`ATargetZone::GetBankingRadiusUnits()`. The ring reuses `UAbilityTargetingIndicatorComponent`
(the same component driving the 5 abilities' cast-preview circles) rather than adding new
decal/mesh-rendering plumbing, and is coloured with the pylon's existing chain colour for
type-keyed zones or a new `ReservedGameplayColours::GetNeutralChrome()` for any-type zones
(Hard Invariant 3: never one of the 5 reserved saturated colours).

## Acceptance Criteria

- [x] Ring spawned/attached at each pylon marker, on the ground plane, centered on the pylon
      — `BankingRadiusIndicatorComponent` added to `APlaceholderTargetZoneActor`, shown via
      `ShowBankingRadiusIndicator()`.
- [x] Ring radius is `ATargetZone::GetBankingRadiusUnits()`, read live from
      `ZoneCollisionComponent`'s real box extent — no hand-tuned duplicate constant.
- [x] Type-keyed zones use `AbilityData::GetChainColourForEnemyType()`; any-type zones use
      `ReservedGameplayColours::GetNeutralChrome()` — never a reserved colour.
- [x] Ring shown via `Show()`, never `Hide()`-den — visible during normal play, no debug gate.
- [x] Non-occluding — inherits `M_AbilityIndicator`'s existing depth-tested translucent blend.
- [x] Reuses `UAbilityTargetingIndicatorComponent` verbatim — no new rendering plumbing.
- [x] REQ-3 automation coverage: ring radius provably tracks the zone's real extent, including
      after a runtime `SetBoxExtent` change (`KrowdKontrolTargetZoneTest.cpp`).
- [x] `harness/run_ue_automation.sh KrowdKontrol.Unit.` passes with zero failures.

## Validation

Full gate (`harness/ci.py`, mode=full):

```
UNIT_PASSED tests=127
PIE_PASSED tests=6
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
GATE_OK mode=full
```

Hard Invariant 3 (5-colour lock) verified by inspection: `GetNeutralChrome()` is a
desaturated blue-grey excluded from `GetAll()`, and `ApplyBankingRadiusIndicatorToMarker()`
only falls back to it for `bAcceptAnyEnemyType` zones — typed zones still resolve through
`AbilityData::GetChainColourForEnemyType()`. No regressions found; no fixes were needed
during validation.
