# PRD: Penning Rules — Any Ability Pens, and You Can See Where

**Author**: operator (Severin), from the 2026-08-28 solo playtest.
**Feeds**: `dark-factory-prd-to-issues`. Grounded in MISSION.md's core loop
(control → herd → pen) and the shipped banking chain (`ATargetZone`,
`docs/prd-herd-mechanic.md`).

## Problem

Two connected playtest findings at the pylons:

1. **Only Stun-controlled robots appear to pen.** Root-controlled units in
   particular did not bank at the pylon; the operator could only reliably pen
   after a Stun. If it is Controlled and inside the pen zone, it must bank —
   regardless of which ability produced the Controlled state.
2. **The pen zone is invisible.** Nothing shows where "close enough to the
   pylon" begins, so delivering a robot is guesswork.

## Operator design decision (2026-08-28, locked — do not re-litigate at triage)

- **Any control ability pens.** A robot in the Controlled state (whatever
  ability caused it — Stun, Sleep, Root, Snare, Fear) that enters its
  matching pylon's banking zone banks. No per-ability penning privileges.
  (Type-keying stays: the zone must accept the robot's type per the shipped
  rule.)
- **The banking radius gets a visual indicator on the ground around the
  pylon** — a circle, sized comparably to the Stun ability's AoE circle (the
  operator's stated size reference), using the pole's chain colour for
  type-keyed zones per the shipped colour-chain rules and neutral treatment
  for any-type zones. Placeholder-quality decal/ring is fine; legibility at
  gameplay distance is the bar.

## Requirements

### REQ-1: Root-cause and fix per-ability penning (P0)
- Reproduce: Root a robot, walk it into its pylon's zone, observe no bank.
  Identify why (suspects, follow the evidence: Root's
  `bAllowsAttackWhileControlled`/movement flavour changing overlap behaviour;
  banking overlap requiring a component state only Stun's flavour sets; Fear
  moving the enemy away from the zone by design — distinguish "can't pen"
  from "won't walk in").
- Fix so Controlled + inside-zone + type-accepted ⇒ banks, for all five
  abilities. Regression coverage per ability (unit; PIE scenario where the
  harness allows — note the holdout's known no-ability-cast limitation, so
  drive states directly in tests).

### REQ-2: Pen-radius ground indicator (P0)
- A visible ring/disc on the floor around each pylon showing the banking
  zone's actual extent (derived from the zone's real collision bounds — never
  a hand-tuned duplicate constant that can drift).
- Chain colour for type-keyed zones, neutral chrome for any-type; obeys Hard
  Invariant 3.
- Visible in normal play (not a debug view); does not occlude the pole or
  robots.

### REQ-3: Indicator honesty test (P1)
- Automation coverage asserting the rendered indicator's radius matches the
  zone's banking-overlap extent within tolerance, so visuals can't lie after
  a later zone-size change.

## Out of scope

- Changing zone type-keying rules or banking scoring.
- New pylon art beyond the ground ring.
- Ranged-unit control-application issues (`docs/prd-ranged-engagement.md`
  REQ-4 owns that half).

## Existing surfaces to build on (do not reinvent)

- `ATargetZone` banking overlap + `OnActorBanked` chain;
  `APlaceholderTargetZoneActor` (pole, beacon, chain-colour material from
  issue #317).
- The ability targeting-indicator shape rendering (#264) for the ground-ring
  drawing technique, and the Stun AoE radius constant as the size reference.
