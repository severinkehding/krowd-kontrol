# PRD: Ability Targeting Shapes & Effect Semantics

**Author**: operator (Severin), 2026-08-22 — decisions made live against the OG
Game Design Document (May 2023 GDD ability table) during the co-op playtest
review. **Feeds**: `dark-factory-prd-to-issues`. **Depends on** the "Cursor &
Aiming Foundation" PRD's issues (in-game cursor, robot-faces-cursor, shared
indicator system) — decompose with those as named prerequisites, not
duplicates.

## Problem

All five abilities today are the same move: auto-hit the nearest hot enemy,
apply an identical immobile "Controlled" state. The OG GDD specifies five
distinct aimed shapes with distinct effect behaviours; the operator has now
locked the concrete geometry. Multi-enemy rooms currently offer no targeting
decision at all.

## Operator design decisions (2026-08-22, locked — do not re-litigate)

Robot size = the pawn's placeholder body diameter; "4× robot size" is the
locked radius language below. Every ability uses the shared press/hold
indicator semantics from the Cursor & Aiming PRD.

| Ability | Shape (locked) | Effect semantics (OG GDD) |
|---|---|---|
| **Fear** | AoE circle centred **on the robot**, radius **4× robot size** — affects every enemy inside | Feared enemies **flee away from the robot** for the duration |
| **Snare** | **Cone, 75°**, in front of the robot (facing = cursor), medium range | Slow: 50% base, 75% on colour match (see #65) — enemies stay active |
| **Root** | **Line shot** from the robot toward the cursor, rather long range — first enemy (or all along the line; implementer states choice) | Immobilized but **can still use its attacks** |
| **Stun** | **Thrown "bomb"**: AoE circle radius **4× robot size**, lands **at the cursor position** (short throw range, clamped) | Full immobilize, baseline duration |
| **Sleep** | **Thrown "bomb"**: same 4×-radius circle at cursor, **long** throw range | Full immobilize; **breaks early if the target is hit by any other ability** |

Future, explicitly out of scope now (record in the Root issue's notes): an
upgraded Root fires additional ±45° side lines — the upgrade system does not
exist yet.

## Requirements

### REQ-1: The five shapes (P0)
Implement each row above through the shared indicator/targeting foundation:
AoE and cone affect **all** valid enemies in the shape (finally multi-target),
line and bombs per their geometry. Ranges follow the OG tiers already encoded
in `AbilityData` (`EAbilityRange` Short/Medium/Long) — reconcile the stored
`EAbilityTargetType` values with the locked table (it becomes: Fear
self-circle, Snare cone, Root line, Stun/Sleep thrown-circle).
Automation tests per shape: in-shape enemies affected, out-of-shape not,
facing/cursor-direction respected, throw-range clamping.

### REQ-2: Distinct effect behaviours without breaking the loop (P0)
- Fear-flee, Snare-slow, Root-can-still-attack, Sleep-break-on-hit implemented
  as flavours **of the existing Controlled state** — `IsControlled()`,
  banking eligibility, Crowd Mastery sampling, and the herd/bank chain must
  keep working unchanged for every flavour (a snared-slow or fear-fleeing
  enemy that crosses its pen still banks).
- Herding interaction note for the implementer: once #214 (follow-the-player)
  exists, define per-flavour precedence (e.g. Fear-flee overrides follow for
  its duration) — state the choice in the PR.
- Automation tests: each flavour's behaviour + each flavour banks at its
  type-keyed zone.

### REQ-3: Rebind pass (P1)
Adopt the OG bindings alongside the current ones (both work): LMB=Stun,
RMB=Sleep, Q=Root, E=Snare, MMB=Fear, keeping 1–5 as alternates. Update
DefaultInput.ini and the pawn's bindings; the tray UX PRD displays whichever
binding set is canonical.

## Out of scope
- Ability upgrades/powerups (the Root side-lines note above).
- Enemy-side attack pattern changes.
- Jump/dash.
- New abilities.

## Existing surfaces to build on (do not reinvent)
The Cursor & Aiming PRD's issues (cursor world position, facing, indicator
system); `UAbilityCastComponent` (cast entry point, cooldown gating, unlock
gating, `OnAbilityCastApplied` — extend, don't fork); `AbilityData`
(durations, ranges, colours, target types); `AEnemyBase`
`ReceiveControl`/`ControllingAbility`/`TickControlledDuration` +
`GetControlledDurationOverrideSeconds` (flavour hooks belong beside these);
the type-keyed `ATargetZone` acceptance; `UCrowdMasterySubsystem` sampling.
