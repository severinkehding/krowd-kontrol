# PRD: Colour-Coded Herding — One Solid Colour per Enemy ↔ Ability ↔ Pen Chain

**Author**: operator (Severin), from the 2026-08-26 solo playtests (L1 and L2).
**Feeds**: `dark-factory-prd-to-issues`. Grounded in MISSION.md Hard
Invariant 3 (the five reserved gameplay-information colours) — this PRD is
that invariant finally doing its job end-to-end in the world, not just in the
HUD.

## Problem

The pen poles/markers are visible in the level, but standing in a room the
operator **could not tell which robot belongs to which pole**. The
type-keyed banking rule already exists in code (a zone can reject a
wrong-type robot, issue #242), but nothing in the world communicates the
mapping — you find out by trial. The matchup knowledge (which ability counters
which enemy) is likewise only surfaced through HUD text (quest tracker
suggested-ability line, briefing card), never on the world objects themselves.

## Operator design decision (2026-08-26, locked — do not re-litigate at triage)

**Solid colour identity across the whole chain.** Each enemy type gets ONE of
the five reserved gameplay colours, applied SOLIDLY (not a small marker — the
dominant readable colour of the thing) to all three members of its chain:

1. the enemy itself (body/tint),
2. the ability that counters it (already colour-keyed via the colour-match
   system — keep those assignments as the source of truth),
3. the pen/pole that robot must be delivered to.

"All blue for blue things, including the pole they need to be brought to."
A player should be able to look at a blue robot and know: blue ability, blue
pole, done.

## Requirements

### REQ-1: Single source of truth for the chain colour (P0) — ✅ implemented, issue #315
- One authority (extend `ReservedGameplayColours` / the existing
  enemy-type→ability matchup data) maps enemy type → chain colour. Every
  consumer below reads it; no local colour constants.
- The assignment must agree with the existing ability colour-match data — if
  Root's colour-match target is TR-UPR, then Trooper's chain colour is Root's
  colour, and so on for all matchups.

### REQ-2: Enemies wear their chain colour solidly (P0) — ✅ implemented, issue #316
- The enemy's body reads as its chain colour at gameplay camera distance —
  material tint on the placeholder mesh is fine (placeholder-first), the
  existing small `EnemyTypeIndicatorComponent` marker is NOT sufficient alone
  but stays as reinforcement.
- Status/effect indication (Controlled bar etc.) must remain readable on top
  of the new tint.

### REQ-3: Pens/poles wear the chain colour of the type they accept (P0)
- Each type-keyed `ATargetZone` pole/marker renders solidly in its accepted
  type's chain colour (emissive or lit so it reads across the room).
- A zone that accepts any type (if any such zone remains) uses the existing
  neutral chrome treatment, never one of the five reserved colours.

### REQ-4: HUD agreement (P1)
- Quest tracker's suggested-ability line and the briefing card use the same
  chain colours when naming enemies/abilities (they already colour-match
  ability names — verify they can't drift from REQ-1's authority).

## Out of scope

- New meshes/art. This is colour on existing placeholders.
- Changing which ability counters which enemy, or banking rules themselves.
- Colour-blind accessibility modes (worth a future PRD; the five-colour
  reservation was chosen with contrast in mind — don't undo that here).

## Existing surfaces to build on (do not reinvent)

- `ReservedGameplayColours` (the five reserved colours + lookup).
- Colour-match bonus data (PR #303) as the enemy↔ability matchup authority.
- `EnemyTypeIndicatorComponent` (issue #242), `ATargetZone` type-keying.
