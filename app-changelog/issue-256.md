# Issue #256: Stun ability — thrown-bomb AoE circle at cursor, short range

Stun was the last ability still using the pre-cursor auto-nearest-target single-enemy
cast path. This change gives it its own locked shape per the PRD: a cursor-aimed
thrown-bomb landing circle (radius = 4x body diameter) clamped to the Short range tier,
affecting every enemy inside on landing. All of the generic thrown-ability mechanism
(clamp, AoE sweep, cursor-routed press handling) was already built and tested in PR
#280 (issue #257/Sleep) specifically so this issue could reuse it with no new
production code — this is a wiring change plus closing the one remaining test gap
(Short-tier range coverage) that PR #280 deliberately deferred.

## Acceptance criteria

- [x] Casting Stun throws toward the cursor position; cursor beyond Short range clamps
      to max throw distance along that direction rather than failing the cast —
      `AFlatCamera3DPrototypePawn::CastStunAbility()` now mirrors `CastSleepAbility()`'s
      cursor-forwarding shape, calling `HandleAbilityKeyPressed(EAbilitySlot::Stun, true,
      CursorWorldPosition)` on success and falling back to the legacy 1-argument call on
      failure. No changes needed to `TryCastThrownAbilityAtLocation`/
      `ComputeClampedThrowLocation`/`ShortThrowRangeUnits` — already generic and already
      built for this (`FlatCamera3DPrototypePawn.cpp`).
- [x] AoE circle (radius = 4x placeholder body diameter) centered on landing point
      affects every enemy inside (multi-target) — existing `TryCastThrownAbilityAtLocation`
      sweep, now newly test-verified for Stun/Short via a new in-shape/out-of-shape case in
      `KrowdKontrolAbilityCastComponentTest.cpp`.
- [x] Affected enemies enter Controlled at full immobilization, baseline duration per
      `AbilityData` — existing `ReceiveControl(Ability)` call inside the AoE sweep;
      Stun's `bWakesEarlyOnOtherAbilityHit = false` is correct and already asserted in
      `KrowdKontrolAbilityDataTest.cpp`. No changes to `AbilityData`.
- [x] `IsControlled()`, banking eligibility, Crowd Mastery sampling, herd/bank chain
      unchanged — no changes to any of that code; existing `EnemyBase` tests using
      `EAbilitySlot::Stun` continue to pass unmodified.
- [x] Automation tests: in-shape/out-of-shape, throw-clamp, stunned-enemy-still-banks —
      added Stun/Short in-shape/out-of-shape and throw-clamp cases to
      `KrowdKontrolAbilityCastComponentTest.cpp`, plus a cursor-routing case to
      `KrowdKontrolAbilityPressHoldComponentTest.cpp`; "still banks" was already covered
      generically via existing `EnemyBase` tests using Stun (no new test needed).

## Scope note

This closes the gap PR #280's own changelog explicitly called out: "Stun (issue #256)
is left fully unwired to the new throw mechanism." It also closes PR #280's own deferred
test gap: `GetThrowRangeUnitsForTier`'s `EAbilityRange::Short` branch
(`AbilityCastComponent.cpp`) was previously reachable only from production code, never
from a test, since every existing thrown-ability test exercised Sleep/Long. The two new
Short-tier cases in `KrowdKontrolAbilityCastComponentTest.cpp` close that.

No changes were made to `AbilityCastComponent.h/.cpp`, `AbilityData.h/.cpp`,
`AbilityPressHoldComponent.h/.cpp`, `EnemyBase.h/.cpp`, or
`AbilityTargetingIndicatorComponent.h/.cpp` — all of that infrastructure was already
generic across abilities and already covered Stun's data shape (`ThrownCircle`/`Short`)
structurally.

## Validation evidence (from validation.md)

Pending `dark-factory-validate` — this changelog is written during `implement`, before
the full harness gate runs. See `validation.md` for `harness/ci.py --mode full` results.
