# PRD: Ranged-Unit Engagement — Targeting You Should See, Damage You Can Outrun

**Author**: operator (Severin), from the 2026-08-28 solo playtest (first session
through the main menu, L1–L3).
**Feeds**: `dark-factory-prd-to-issues`. Grounded in MISSION.md's core loop and
the shipped SN-1PR sniper (issues #253–#257 ability system, #336 attack-window
work).

## Problem

Ranged units (SN-1PR) barely participate in the game right now, on both sides
of the exchange:

1. **They deal no damage.** Their shot fires (delegate + tell) but nothing
   reaches the player's energy — there is no threat to respect.
2. **The player can't read them.** Nothing shows WHO the sniper is targeting
   or WHEN the shot is coming, so there is no counterplay — and no system to
   balance until seeing/escaping exists.
3. **Control-activation feels wrong** (operator observation): a ranged unit
   seems to need a Stun first before other control effects "take" and it can
   be dragged to the pylon. Whether that is a real state-machine gate or a
   misreading of effect application, the intended rule below must hold and be
   verified.

## Operator design decision (2026-08-28, locked — do not re-litigate at triage)

A full engagement loop, in this shape:

- The sniper **acquires the player as a target when in its attack range**,
  and this is **visible to the player** (a clear targeting telegraph the
  player can see at gameplay camera distance — beam/line/marker; placeholder
  art fine, legibility mandatory, Hard Invariant 3 colour rules apply).
- The telegraph runs for a readable wind-up, then the shot fires and **applies
  real damage** through the existing contact-damage authority (see #22's
  triage note: `ApplyContactDamage` is the only permitted energy mutator —
  route the shot through a per-hit application of it, never a new drain).
- **The player can break the engagement by leaving range**: moving out of
  attack range mid-telegraph cancels the shot and drops the target; the
  sniper then **chases until back in range** and re-acquires. Outrunning must
  be genuinely possible at current move speeds — that is the balance lever.
- **Any control ability affects a ranged unit directly** — no Stun-first
  activation gate. If the current behaviour requires Stun before other
  effects apply, that is a bug against this rule (see also
  `docs/prd-penning-rules.md` for the banking half of the same complaint).

## Requirements

### REQ-1: Visible targeting telegraph (P0)
- On target acquisition: a player-visible telegraph tied to the sniper and/or
  the player (readable at gameplay distance, distinct from the existing
  attack-tell audio ping, no reserved-colour violations).
- Telegraph ends when: shot fires, target breaks range, or the sniper is
  Controlled.

### REQ-2: Shot damage through the contact-damage authority (P0)
- The fired shot applies a tunable per-hit energy cost via the existing
  `ApplyContactDamage` seam. Named constants, no magic numbers; disclosed in
  the changelog for balance review.

### REQ-3: Range-based break + chase (P0)
- Leaving attack range mid-telegraph cancels the shot (no damage) and the
  sniper re-enters chase; re-acquisition restarts the full telegraph (no
  stored progress).
- Unit coverage for acquire → break → chase → re-acquire, and PIE scenario
  coverage for the full loop where feasible.

### REQ-4: No Stun-first activation gate (P0)
- Verify and enforce: every control ability applies its effect to a ranged
  unit in any pre-Controlled state (Idle/Alert/Attack), same as melee types.
  If a gate exists, remove it; add a regression test asserting each ability
  lands on an Attack-state sniper.

### REQ-5: Balance framework, not final balance (P1)
- First-pass tuning documented (telegraph seconds, damage per hit, ranges,
  chase speed vs player speed) with the explicit expectation of operator
  playtest iteration. Numbers in one tunable place.

## Out of scope

- New art/meshes/animation. Placeholder telegraph visuals are fine.
- Reworking melee enemy behaviour (covered by #313/#336's shipped model).
- Difficulty scaling per level (belongs to the levels PRD).

## Existing surfaces to build on (do not reinvent)

- `ASniperEnemy`'s shot delegate/tell + attack window (#336's derived
  duration), `AEnemyBase` state machine.
- `UPlayerEnergyComponent::ApplyContactDamage` (the locked, only energy
  mutator) and the energy HUD reaction (#222).
- The ability-targeting indicator component (#264/#265) as prior art for
  drawing legible world-space telegraphs.
