# Issue #254: Snare ability - 75 degree cursor-aimed cone with a partial slow effect

Gives Snare its locked shape+effect, the last of the 5 crowd-control abilities to get
one. Adds a new cone-shaped cast entry point, `UAbilityCastComponent::
TryCastConeAbilityTowardLocation`, built by mirroring `TryCastLineAbilityTowardLocation`'s
structure exactly (same gate chain, same "always consumes cooldown once gates pass"
contract, same `ApplyControlToEnemiesInShape` multi-target sweep), with a new
hand-rolled `IsPointInCone` shape test (apex + direction + half-angle + range, tested
on the X/Y plane only, matching this codebase's flat-top-down convention) instead of
Line's segment-distance test. `ComputeConeDirection` mirrors `ComputeLineEndLocation`'s
dead-zone-guarded direction math. `UAbilityPressHoldComponent::HandleAbilityKeyPressed`
gets a new `Cone` branch (indicator + cast dispatch), inserted before the generic
`ThrownCircle` branch so a Cone-target press never falls into the wrong shape.
`AFlatCamera3DPrototypePawn::CastSnareAbility()` now forwards the cursor world
position, mirroring `CastRootAbility()` exactly, including its cursor-unavailable
fallback to the pre-cursor single-target cast.

Also adds the first "Controlled but not fully immobilized" mechanism: a new
`bAllowsMovementWhileControlled` flag and `ControlledSpeedMultiplier` float on
`FAbilityData`, modeled directly on Root's existing `bAllowsAttackWhileControlled`/
`IsAttackBehaviorActive()` pair (issue #255). `AEnemyBase::TickChaseMovement` now
gates on the new `IsMovementBehaviorActive()` (mirroring `IsAttackBehaviorActive()`'s
shape) and scales its move distance by `GetControlledSpeedMultiplier()`; every concrete
enemy type's `AdvanceAttackTelegraph` (`BomberEnemy`/`RunnerEnemy`/`SniperEnemy`/
`TrooperEnemy`) scales its elapsed-time accumulation by the same multiplier. Snare's
row sets `bAllowsMovementWhileControlled = true` and `ControlledSpeedMultiplier = 0.5f`
(and flips `bAllowsAttackWhileControlled` to `true` too, so its attack telegraph
actually advances); every other ability's row gets the inert default (`false`/`1.0f`),
so this cannot change existing Stun/Sleep/Fear/Root behaviour.

**Explicitly out of scope**: the 75%-on-colour-match slow bonus. Issue #65 (the general
colour-match-bonus mechanism) has not landed anywhere in this repo yet - per the
issue's own instructions, only the base 50% flat slow is implemented.
`ControlledSpeedMultiplier` is deliberately a single flat float today, not
colour-match-branching logic, so a follow-up on #65 can make
`AEnemyBase::GetControlledSpeedMultiplier()` colour-match-aware without touching this
issue's shape.

## Acceptance criteria

- [x] Casting Snare resolves a 75 degree-wide cone originating at the robot, oriented
      toward the cursor world position, extending to the Medium range tier.
- [x] Every enemy inside the cone at cast time is affected (multi-target).
- [x] Affected enemies enter `Controlled` with a "snare" flavour: movement/attack speed
      reduced by 50% (base) - the 75% colour-match case is explicitly deferred, blocked
      on issue #65.
- [x] `IsControlled()`, banking eligibility, and Crowd Mastery sampling all keep working
      unchanged for snared enemies - none of them read any speed/multiplier state, only
      `CurrentState`.
- [x] Automation tests cover: in-cone-affected, out-of-cone/behind-robot-unaffected,
      cone orientation follows cursor direction, range-clamp boundary, pure-math
      (`ComputeConeDirection`/`IsPointInCone`), base-slow-percentage observably correct
      (via telegraph timing), a snared enemy still banks, a whiff still consumes the
      cooldown.
- [x] `app/` and `app-source-tracked/` are byte-identical for every changed file.
- [x] `harness/ci.py --mode=full` passes with zero regressions in the pre-existing
      automation suite.

---

The real Unreal project stays under `app/` (gitignored, D-003) - this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
