# PRD: Cursor & Aiming Foundation

**Author**: operator (Severin), 2026-08-22, from the OG Game Design Document
(May 2023, "Krowd Kontrol" GDD — control scheme: "Mouse Pointer = Change player
direction") and the live co-op playtests. **Feeds**: `dark-factory-prd-to-issues`.
This PRD is the foundation layer; the per-ability targeting shapes are a
separate PRD ("Ability Targeting Shapes & Effect Semantics") that depends on
the issues from this one.

## Problem

The game has no aiming. There is no in-game mouse cursor, the robot has no
facing control, and every cast auto-targets the nearest hot enemy — which the
operator's playtests experienced as "stun is applied globally." The OG GDD is
a twin-stick design (genre: top-down shooter; WASD moves, mouse points), and
every ability is specified as an aimed, bullet-style cast. None of that layer
exists.

## Operator design decisions (2026-08-22, locked — do not re-litigate)

1. **A visible in-game mouse cursor exists during play** — the aiming
   reference for every ability.
2. **The robot faces the cursor** (movement stays WASD-relative; facing is
   cosmetic-plus-aim, per the OG "change player direction").
3. **Shared ability-indicator interaction pattern**, identical for every
   ability:
   - **Press** the ability's key: its range/shape indicator flashes visibly
     and the ability activates immediately.
   - **Hold** the key (including press-and-hold while the ability is on
     cooldown): show the range/shape indicator only — a preview, no cast.

## Requirements

### REQ-1: In-game cursor (P0)
- A visible cursor during PIE/gameplay (software crosshair or shown hardware
  cursor — implementer's judgment; must survive the existing
  capture-permanently viewport mouse settings in DefaultInput.ini).
- A clean, single API for "cursor world position on the gameplay plane"
  (deproject to the floor plane) that the facing system, the shapes PRD, and
  future systems consume — one implementation, no per-ability re-derivation.

### REQ-2: Robot faces the cursor (P0)
- The player pawn's yaw tracks the cursor's world position each frame.
  Movement input remains world-relative WASD (unchanged bindings).
- This defines "in front of the robot" for the cone/line abilities in the
  shapes PRD.
- Automation tests: facing derives from a given cursor world position;
  movement unaffected by facing.

### REQ-3: Shared targeting-indicator system (P0) — ✅ rendering primitive implemented, issue #264 (press/hold input wiring still open)
- A reusable indicator component/system that any ability can drive with a
  shape spec (circle-at-actor, circle-at-cursor, cone, line — shapes
  parameterized; the concrete per-ability values live in the shapes PRD):
  renders as a ground decal/primitive in **the ability's reserved colour**
  (legitimate information-colour use under Hard Invariant 3).
- Implements the locked press/hold semantics from decision 3 for all five
  ability keys, including the on-cooldown preview.
- Placeholder-quality rendering is fine (translucent shapes); readability at
  the current camera framing is the bar.
- Automation tests: press shows-and-casts; hold previews without casting;
  hold-during-cooldown previews; indicator colour matches AbilityData.

## Out of scope
- The per-ability shapes, ranges, and effect behaviours (the shapes PRD).
- Controller right-stick aiming (OG maps it; later).
- Jump/dash movement verbs (OG Space/F; separate future scope).
- Rebinding UI.

## Existing surfaces to build on (do not reinvent)
`AFlatCamera3DPrototypePawn` (input component, camera boom — deprojection uses
its camera); `AbilityData` (colours per ability); `ReservedGameplayColours`;
DefaultInput.ini's existing action/axis mappings and mouse-capture settings;
the ability keys' existing `BindAction` sites (press/hold needs
IE_Pressed/IE_Released pairs).
