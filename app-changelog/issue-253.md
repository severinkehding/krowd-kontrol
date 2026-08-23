# Issue #253: Fear ability — self-centered AoE circle with flee-from-robot effect

Gives Fear its own real targeting shape and Controlled-state flavour, per
`docs/prd-ability-shapes.md`'s locked table: a multi-target AoE circle centered on
the casting robot's own live position, and a new Controlled flavour where affected
enemies actively move away from the robot instead of standing immobile. Before this
change, Fear fell through `UAbilityPressHoldComponent`'s generic default branch and
cast exactly like every un-shaped ability - single nearest-enemy auto-target via
`TryCastAbility`, full stand-immobile Controlled state, and an indicator radius that
lied about the actual affected area (`CastRangeUnits`, not the real AoE).

Adds `TryCastSelfCircleAbility` to `UAbilityCastComponent` (mirroring
`TryCastThrownAbilityAtLocation`'s gate/cooldown/sweep/broadcast shape, and
`TryCastConeAbilityTowardLocation`'s "read Owner once after gates pass" idiom, but
with no aim point - the circle is always exactly centered on
`GetOwner()->GetActorLocation()` at cast time) and a new `SelfCircleRadiusUnits`
property (`EditDefaultsOnly`, default `400.0f` = 4x the placeholder robot body
diameter, the same locked value `ThrownCircleLandingRadiusUnits` already uses for
Stun/Sleep, kept as an independently-tunable property rather than reused, matching
this codebase's existing `LineHitWidthUnits`-gets-its-own-property precedent). Every
`AEnemyBase` within that radius is affected (multi-target), and the cooldown is
always consumed once gates pass, same "a cast commits the moment it's cast" contract
`TryCastThrownAbilityAtLocation`/`TryCastLineAbilityTowardLocation`/
`TryCastConeAbilityTowardLocation` already share.

`UAbilityPressHoldComponent::HandleAbilityKeyPressed` now branches on
`TargetType == EAbilityTargetType::SelfCircle` (inserted before the final generic
`else`, reachable only from the no-cursor-location side of the chain since
`CastFearAbility()` never supplies one): shows a `CircleAtActor` indicator sized to
`SelfCircleRadiusUnits` (not `CastRangeUnits`) and dispatches through
`TryCastSelfCircleAbility` instead of the old single-target `TryCastAbility`.

Adds `bFleesFromCasterWhileControlled` to `FAbilityData` (true only for Fear,
following the existing per-ability boolean-flavour-flag convention
`bAllowsAttackWhileControlled`/`bAllowsMovementWhileControlled` already established)
and a new private `AEnemyBase::TickFleeMovement(CasterLocation, DeltaSeconds)`,
mirroring `TickChaseMovement`'s straight-line movement shape but inverted: direction
is away from `CasterLocation`, not toward it; gated on
`CurrentState == Controlled && AbilityData::Get(ControllingAbility).
bFleesFromCasterWhileControlled`, not `IsMovementBehaviorActive()`/`Alert`; and no
remaining-distance clamp, since fleeing has no destination to overshoot - it moves
the full `GetEffectiveMovementSpeedUnitsPerSecond() * DeltaSeconds` away every tick
for as long as the gate holds. Wired into `Tick()` immediately after
`TickChaseMovement`, using the same live `UGameplayStatics::GetPlayerPawn` location
each tick, so a fleeing enemy tracks the robot's *current* position, not a
cast-time snapshot. No new `EEnemyState` was added: `CurrentState ==
EEnemyState::Controlled` stays the one and only bankable/herdable/threat-hot
condition, so `IsControlled()`, banking, and Crowd Mastery sampling are unaffected
for Fear.

## Design decisions

- **Fear does not consult `GetControlledSpeedMultiplier()`.** That multiplier's
  existing doc comment scopes it to `bAllowsMovementWhileControlled`/
  `bAllowsAttackWhileControlled` (Snare/Root); Fear's literal leaves it at the inert
  default `1.0f`, so `TickFleeMovement` reads
  `GetEffectiveMovementSpeedUnitsPerSecond()` directly, keeping that multiplier's
  documented scope accurate rather than silently widening it.
- **No dedicated flee movement speed** - reuses
  `GetEffectiveMovementSpeedUnitsPerSecond()` (same speed chase already uses,
  including the Elite multiplier), rather than inventing a second per-type tunable
  with no design-locked value to base it on.
- **Collision-aware / pathfinding flee is out of scope**, mirroring
  `TickChaseMovement`'s existing "straight line only, no pathfinding" scope (PRD
  REQ, project-wide) - an unswept `SetActorLocation` away from the caster is
  sufficient.
- **Fear-flee vs. follow-the-player precedence is not applicable yet.** Issue #214
  (follow-the-player) has not landed, so there is no follow behaviour to override -
  no speculative precedence logic was built against a system that doesn't exist.
- **A feared enemy that flees toward or through its own type-keyed zone/pen still
  banks "by accident."** This is the acceptance criterion's own required behaviour
  (proven by the new `KrowdKontrolRoomActorBankingWiringTest.cpp` Fear/Bomber case),
  not a bug - banking is purely "the enemy's actor location ends up inside its
  `ATargetZone`'s bounds," unchanged for Fear.
- **Snare (Cone, issue #254) has already landed** by the time this issue was
  implemented - the generic `else` branch in `HandleAbilityKeyPressed` is now
  unreachable for all 5 abilities in practice (Stun/Sleep -> `CircleAtCursor`, Root
  -> `Line`, Snare -> `Cone`, Fear -> `SelfCircle`), but is left in place unchanged
  as a defensive fallback rather than removed, since no plan step called for
  deleting it and it costs nothing to keep.

## Acceptance criteria

- [x] Casting Fear resolves a circle centered on the caster's own position, radius =
      `SelfCircleRadiusUnits` (400.0f = 4x the placeholder body diameter) -
      `UAbilityCastComponent::TryCastSelfCircleAbility`.
- [x] Every enemy inside the circle at cast time is affected (multi-target), not
      just the nearest - `KrowdKontrol.Unit.AbilityCastComponent` case (bb-fear).
- [x] Affected enemies enter Controlled with the fear flavour: they actively move
      away from the robot's live position for the Controlled duration -
      `AEnemyBase::TickFleeMovement`, `KrowdKontrol.Unit.EnemyBase` cases
      (v-fear)/(z-fear).
- [x] `IsControlled()`, banking eligibility, Crowd Mastery sampling, and the
      herd/bank chain continue to work unchanged for feared enemies - no
      `CurrentState` fork was introduced;
      `KrowdKontrolRoomActorBankingWiringTest.cpp`'s new Fear/Bomber case proves
      banking specifically.
- [x] Uses the shared press/hold indicator semantics (`CircleAtActor` kind) and the
      existing `EAbilityTargetType::SelfCircle` target-type value -
      `KrowdKontrol.Unit.AbilityPressHoldComponent` case (l).
- [x] Automation tests cover: in-circle affected, out-of-circle not, flee direction
      (away, not toward or idle), and a feared enemy still banks when delivered to
      its type-keyed zone - `KrowdKontrol.Unit.AbilityCastComponent` cases
      (aa-fear)-(gg-fear), `KrowdKontrol.Unit.EnemyBase` cases (v-fear)-(z-fear),
      `KrowdKontrolRoomActorBankingWiringTest.cpp`'s Fear/Bomber case.
- [x] PR body states the Fear-flee vs. follow-the-player (#214) precedence question
      is not applicable yet - see "Design decisions" above.
- [x] Level 1-3 validation commands pass with exit 0.
- [x] Code mirrors existing patterns exactly (naming, structure, logging,
      doc-comment density).
- [x] No regressions in existing tests - `KrowdKontrolAbilityPressHoldComponentTest.cpp`
      case (d) (Fear through the old generic path) still passes unmodified, since its
      single-target assertion holds regardless of which branch produced the cast.
- [x] `app-source-tracked/` mirror is byte-identical to `app/`'s changed files
      (D-009 carve-out).

---

The real Unreal project stays under `app/` (gitignored, D-003) - this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
