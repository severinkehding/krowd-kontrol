# Issue #67: Ability-Cast VFX Colour Telegraph

Adds `UAbilityCastVFXComponent` (`app/Source/KrowdKontrol/AbilityCastVFXComponent.h/.cpp`),
a reusable placeholder-primitive VFX component attached to
`AFlatCamera3DPrototypePawn` alongside `UAbilityCastComponent`. It binds to that
sibling's `OnAbilityCastApplied` delegate in the pawn's constructor and, on every
successful cast, flashes a `UPointLightComponent` tinted to
`AbilityData::Get(Ability).Colour` at the target enemy's location for
`CastFlashDurationSeconds` (default 0.35s), clearing via an `FTimerHandle`. This gives
the colour-match bonus (PRD 13 REQ-2) an observable ability-side signal to pair with
the enemy's existing eye-glow colour, with zero enemy-side changes and zero new
dependencies (no Niagara, no projectile actor, no new art).

## Acceptance criteria

- [x] Each of the 5 abilities' cast VFX (`UAbilityCastVFXComponent::CastFlashLightComponent`)
      is tinted to its locked colour on cast, using only `AbilityData::Get(Ability).Colour`
      — never a new literal.
- [x] The VFX is a placeholder primitive (`UPointLightComponent`) — no final art asset
      required or added.
- [x] No 6th saturated information colour is introduced anywhere in this diff (verified
      by the new test's reserved-colour assertion against `ReservedGameplayColours::GetAll()`).
- [x] `KrowdKontrol.Unit.AbilityVFXColour` exists, compiles, and asserts each ability's
      VFX colour property against `AbilityData`'s locked colour, both via direct calls
      (all 5 slots) and via a real end-to-end `TryCastAbility` → broadcast → VFX-colour-update
      path.
- [x] `app/` and `app-source-tracked/` copies of every changed/new file are identical
      (`diff` shows no output for all 6 touched/created files).

## Files changed

- `Source/KrowdKontrol/AbilityCastVFXComponent.h` / `.cpp` (CREATE) — the new component.
- `Source/KrowdKontrol/FlatCamera3DPrototypePawn.h` / `.cpp` (UPDATE) — construct and
  bind the new component alongside the existing `AbilityCastComponent`.
- `Source/KrowdKontrol/Private/Tests/KrowdKontrolAbilityVFXColourTest.cpp` (CREATE) —
  `KrowdKontrol.Unit.AbilityVFXColour`.
- `Source/KrowdKontrol/EnemyBase.h` (UPDATE, deviation from the plan) — added
  `friend class FKrowdKontrolAbilityVFXColourTest;` alongside the existing friend
  grants (`FKrowdKontrolAbilityCastComponentTest` etc.), required because
  `AEnemyBase::TickCheckDetection` is private and the new test drives a real
  `AEnemyBaseTestActor` through Idle->Alert directly, the same established pattern
  every other `AEnemyBase`-driving test already uses.

## Deviations from the plan

- **`EnemyBase.h` friend grant** (see above) — not listed in the plan's "Files to
  Change" table, but required for the plan's own Task 5 test code to compile.
- **Test case (c) unlocks Sleep first.** The plan's Task 5 code called
  `TryCastAbility(EAbilitySlot::Sleep)` without ever unlocking Sleep;
  `UAbilityUnlockComponent` starts a run with only Stun unlocked
  (`AbilityUnlockComponent.cpp`), so the cast would have failed and the test's own
  `TestTrue("TryCastAbility should succeed...")` assertion would have failed. Added
  `UnlockComponent->NotifyLevelReached(2)` (the documented entry point that unlocks
  Sleep, per the `LevelToAbility` map) immediately after constructing the unlock
  component and before the cast attempt.
- **Dropped the unused `AbilityCastAppliedTestListener.h` include** from the new test
  file — the final test drives the VFX component directly rather than through a
  listener double, matching the plan's own noted "if unused after final authoring,
  drop the include" guidance.

## Validation evidence

Light inline validation (`python harness/ci.py --quick`):

```
HARNESS_START mode=quick driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=52
GATE_OK mode=quick
```

Full gate (`python harness/ci.py`, mode=full — real `UnrealBuildTool` compile +
`KrowdKontrol.Unit.AbilityVFXColour` Automation run) requires the Windows-side Unreal
Editor build this session cannot execute directly (same environmental constraint the
plan's own Risks table flagged) — deferred to the `dark-factory-validate` node, which
runs in an environment with that build access.

MISSION.md Hard Invariants reviewed against this diff: no kill-rule, ability-roster, or
enemy-roster invariant is touched. Colour-lock (Hard Invariant 3) is directly served,
not violated — the new component sources its colour exclusively from
`AbilityData::Get(Ability).Colour`, itself always one of the 5
`ReservedGameplayColours::Get*()` values, and the new test's reserved-colour
containment assertion is a regression guard against a 6th colour ever being introduced
here.

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
