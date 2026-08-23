# Issue #255: Root ability — cursor-aimed line trace, attack-capable immobilize

Wires Root into a real cursor-aimed line cast, and gives the `Controlled` state a
second flavour where the target's own attack behaviour keeps running.

Adds `TryCastLineAbilityTowardLocation` to `UAbilityCastComponent` (mirroring
`TryCastThrownAbilityAtLocation`'s gate/cooldown/scan/broadcast shape, but with
line/segment geometry): the line always extends the full Long-range tier
(`LongThrowRangeUnits`, via the existing `GetThrowRangeUnitsForTier` helper) from
the Owner toward the cursor - never clamped down like `ThrownCircle`'s click-to-land
targeting - and every `AEnemyBase` within `LineHitWidthUnits` of that segment is
affected (hand-rolled point-to-segment distance test, matching this module's
existing "small, directly-testable, hand-rolled vector math" precedent). Cooldown is
always consumed once gates pass, same "a cast commits the moment it's fired"
contract `TryCastThrownAbilityAtLocation` already uses.

`UAbilityPressHoldComponent::HandleAbilityKeyPressed` now branches on
`AbilityData::Get(Ability).TargetType`: Line-target abilities (Root) get a `Line`
indicator shape (`Origin` at the Owner, not the cursor) and route into the new cast
method, instead of being wrongly treated as `ThrownCircle`.
`AFlatCamera3DPrototypePawn::CastRootAbility()` now fetches the cursor world position
and forwards it, mirroring `CastStunAbility()`/`CastSleepAbility()`'s
fetch-with-fallback pattern exactly (falls back to the pre-cursor auto-nearest cast
if no cursor position is available this frame).

Adds `bAllowsAttackWhileControlled` to `FAbilityData` (true only for Root, following
the existing per-ability boolean-flag convention `bWakesEarlyOnOtherAbilityHit`
already established) and a new `AEnemyBase::IsAttackBehaviorActive()` protected
helper (true during `Attack`, or during `Controlled` when the controlling ability
flags the new bool). All four concrete enemy types' `AdvanceAttackTelegraph` gates
switch from `GetEnemyState() != Attack` to `!IsAttackBehaviorActive()`, and their
`OnControlledEntry` skips clearing `AttackTellLightComponent` specifically when the
flag is set - so a Root-controlled enemy's in-progress or newly-entered attack
telegraph (light, timer, per-type fire delegate/damage call) keeps running exactly
as it would in `Attack`. No new enemy state was added: `CurrentState ==
EEnemyState::Controlled` stays the one and only bankable/herdable/threat-hot
condition, so `IsControlled()`, banking, and Crowd Mastery sampling are unaffected
for Root. Movement needed no change - `TickChaseMovement` is already a no-op outside
`Alert`, so "movement fully prevented" during `Controlled` was already true for
every ability.

## Design decisions

- **Target choice: Root hits every enemy within `LineHitWidthUnits`, not just the
  first (piercing).** Consistent with every other locked ability shape in
  `docs/prd-ability-shapes.md` already hitting everything inside it; the issue's own
  "whatever it hits" reads more naturally as plural; and a first-hit-only line would
  make Root strictly worse than a wider-reach AoE at the same range for zero extra
  risk.
- **Cooldown consumption: always consumed once gates pass**, whether or not the line
  connects - a second deliberate place `TryCastLineAbilityTowardLocation` does NOT
  mirror `TryCastAbility`'s "a whiff never consumes the cooldown" contract, same as
  `TryCastThrownAbilityAtLocation` already doesn't.
- **Herd/bank delivery for a rooted enemy is identical to every other ability
  today**: banking is purely "the enemy's actor location ends up inside an
  `ATargetZone`'s bounds", already proven for a Root-controlled Trooper by
  `KrowdKontrolRoomActorBankingWiringTest.cpp` (issue #242). A real herd/push-the-
  crowd mechanic depends on issue #214 ("follow-the-player"), which does not exist
  yet - not built here.
- **Known future item, explicitly out of scope**: the upgraded Root (±45° side
  lines) - the upgrade system does not exist yet.
- **Known gap**: a rooted enemy whose one-shot fire guard (Sniper/Bomber/Runner's
  `bShotFiredForCurrentAttack`/`bExplodedForCurrentAttack`/`bDrainFiredForCurrentAttack`)
  already latched before Root lands stays silent until its next natural
  Alert->Attack cycle (Root does not re-invoke `OnAttackEntry()` to re-arm it, by
  design - see the plan's Architect Decisions). Trooper (no fire-once guard) always
  keeps firing while Rooted.

## Acceptance criteria

- [x] Casting Root resolves a line trace from the robot toward the cursor, extending
      to the Long range tier already defined in `AbilityData` -
      `UAbilityCastComponent::TryCastLineAbilityTowardLocation`/`ComputeLineEndLocation`.
- [x] PR body states and justifies the first-vs-all target-choice decision - see
      "Design decisions" above.
- [x] Hit enemy/enemies enter `Controlled` with movement fully prevented (unchanged,
      `TickChaseMovement` already a no-op outside `Alert`) and their own attack
      behaviour continues to function normally, unlike Stun/Sleep -
      `AEnemyBase::IsAttackBehaviorActive()`, exercised by
      `KrowdKontrol.Unit.TrooperEnemy` case (l2).
- [x] `IsControlled()`, banking eligibility, Crowd Mastery sampling, and the
      herd/bank chain keep working unchanged for rooted enemies - no `CurrentState`
      fork was introduced.
- [x] PR body states how herd-delivery works for a rooted enemy - see "Design
      decisions" above, citing `KrowdKontrolRoomActorBankingWiringTest.cpp` (issue
      #242).
- [x] Automation tests: enemy directly in the line's path affected, enemy off the
      line not affected, range clamps at the Long tier, piercing/multi-hit, a rooted
      enemy can still perform its attack tell/behaviour while Controlled - covered by
      `KrowdKontrol.Unit.AbilityCastComponent` cases (p-root)/(q-root)/(r-root)/
      (s-root), `KrowdKontrol.Unit.AbilityPressHoldComponent` case (j), and
      `KrowdKontrol.Unit.TrooperEnemy` case (l2). A rooted enemy still banks once
      herded - existing coverage, `KrowdKontrolRoomActorBankingWiringTest.cpp` (issue
      #242), cited not re-built.
- [x] PR body records the upgraded-Root (±45° side lines) future item without
      building any part of it - see "Design decisions" above.
- [x] Level 1-3 validation commands pass with exit 0.
- [x] Code mirrors existing patterns exactly (naming, structure, logging,
      doc-comment density).
- [x] No regressions in existing tests - `KrowdKontrolTrooperEnemyTest.cpp`'s two
      former Root-interrupt cases were repointed to Stun (still proving "a
      full-immobilize ability stops the attack telegraph"), and
      `KrowdKontrolSniperEnemyTest.cpp`/`BomberEnemyTest.cpp`/`RunnerEnemyTest.cpp`
      needed zero edits (their own Root usages are glow-baseline checks unaffected
      by the `AdvanceAttackTelegraph` gate swap).

---

The real Unreal project stays under `app/` (gitignored, D-003) - this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
