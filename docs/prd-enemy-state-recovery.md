# PRD: Enemy State Recovery — No Robot Ever Freezes Permanently

**Author**: operator (Severin), from the 2026-08-26 solo playtests (L1 and L2,
first full runs on the fixed input build).
**Feeds**: `dark-factory-prd-to-issues`. Grounded in MISSION.md's core loop
(control enemies, herd them to pens) — a robot that stops responding to the
world breaks that loop outright.

## Problem

Observed in BOTH Level 1 and Level 2, same shape each time: a robot crashed
into the player and then **froze in place indefinitely**. It stayed inert until
the operator applied a fresh control effect (Stun or Sleep) to it — at which
point it recovered completely and resumed normal behaviour (moved, followed as
a controlled enemy). Nothing on screen explained the freeze; it reads as a bug,
not a mechanic.

The positive part of the same observation, for the record: the per-effect
status/duration indication on robots is working — applying different skills
visibly changes the robot's indicated state. The defect is strictly the
unrecoverable-without-player-action freeze after player contact.

## Operator design decision (2026-08-26, locked — do not re-litigate at triage)

**No enemy state may be inert without an exit.** Every state an enemy can enter
(including whatever state player-contact currently puts it in) must have at
least one guaranteed exit that does not depend on the player spending an
ability on it: a timer, an effect expiry, or an event that provably fires.
Re-applying a control effect being the ONLY way out is the bug, not the fix.

## Requirements

### REQ-1: Root-cause the post-contact freeze (P0)
- Reproduce: enemy collides with the player pawn in normal play (observed with
  the enemy in or entering its aggro/chase behaviour in L1 and L2).
- Identify which state the enemy lands in after contact and why it never
  exits. Suspect surface: the contact/attack path on `AEnemyBase` and its
  interaction with the effect state machine (the same machinery the
  Controlled/effect-expiry flow uses) — but follow the evidence, not this
  guess.

### REQ-2: Guaranteed exit from every enemy state (P0)
- After the fix: a robot that touches the player resumes its normal behaviour
  tree (return to chase, patrol, or its room's idle rules) once whatever
  contact reaction it has (attack, knockback recoil, cooldown) completes.
- Audit the enemy state machine for other states with no unconditional exit;
  fix any found in the same pass or file follow-up issues per find.

### REQ-3: Regression coverage (P1)
- A `KrowdKontrol.PIE` scenario (or Unit test if the state machine is testable
  headless) that drives an enemy into player contact and asserts it is moving/
  acting again within a bounded time WITHOUT any new ability application.

## Out of scope

- Rebalancing contact damage, knockback distances, or effect durations.
- New status-bar visuals (the existing indication is fine).

## Existing surfaces to build on (do not reinvent)

- `AEnemyBase` state machine + effect expiry (Controlled-duration flow,
  issues #224/#225).
- The PIE scenario test group (`KrowdKontrol.PIE`, issues #236/#237) for the
  regression test.
