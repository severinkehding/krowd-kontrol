# PRD: Level Playability & Presentation (spawn, lighting, geometry, camera, wayfinding)

**Author**: operator (Severin), drafted with the interactive session from a live
playtest, 2026-08-20. **Feeds**: `dark-factory-prd-to-issues`. Grounded in MISSION.md
(`05` level structure, the neon-noir identity) and in direct operator playtest
evidence from today — every problem below was hit in a real PIE session on
`L_Level01`, and every cause below was verified against the actual map contents and
source, not guessed.

## Problem — playtest findings, each with its verified cause

1. **Level 1 is unplayable out of the box.** `L_Level01` (and `L_Level02`) contain
   no player pawn and no `PlayerStart`. The project's design places playable pawns
   in the map (they self-possess via `AutoPossessPlayer`; `AKrowdKontrolGameMode`
   deliberately sets no `DefaultPawnClass` — see its header comment / issue #132).
   Result: PIE falls back to the engine's invisible free-fly DefaultPawn at the
   world origin — movement keys "work", nothing else does. Verified live: enemies
   logged `FindPlayerEnergyComponent ... found no APawn with a
   UPlayerEnergyComponent`, and casting only produced log traffic after the
   operator's session manually placed a `FlatCamera3DPrototypePawn` (not saved —
   the shipped map is still broken).
2. **The level is pitch dark.** `L_Level01`/`L_Level02` contain zero scene
   lighting — no DirectionalLight, no SkyLight, no SkyAtmosphere, no fog, no
   PostProcessVolume (verified against the map packages). The only light sources
   are per-actor accent point lights (enemy glow/trim, beacon, attack-tell). The
   prototype map `L_FlatCamera3DPrototype` has a full lighting stack; the real
   levels got none of it.
3. **There is no world to see.** `L_Level01` contains no static meshes at all — no
   floor, no walls (verified). `ARoomActor` is topology-only by design (target-zone
   markers, no geometry — its header says so). The player floats over void.
4. **Everything feels far away / hard to read.** Rooms sit ~30m apart (x = 0, 3000,
   6000) connected only by logical `DoorConnectorActor`s, and the camera is a fixed
   `SpringArm` at `TargetArmLength = 800`, pitch −80°, no tuning knobs exposed.
5. **No sense of where to go.** Target zones are placeholder markers with small
   beacon lights; nothing guides the player across 30m of darkness toward them.

## Requirements

### REQ-1: Every gameplay level contains a playable spawn (P0)
- Place a `FlatCamera3DPrototypePawn` (the current playable pawn) plus a
  `PlayerStart` at the intended level entrance in `L_Level01` and `L_Level02`.
- Add an automation test that loads each gameplay map (`L_Level01`, `L_Level02`,
  and future `L_Level*`) and fails if the world contains no pawn with a
  `UPlayerEnergyComponent` — so a level can never ship spawn-broken again. (The
  test-side map-load pattern already exists in the level tests.)

### REQ-2: Baseline lighting rig for gameplay levels (P0)
- Every gameplay map gets a lighting baseline the player can actually see by:
  directional light + sky light (+ optional subtle fog), tuned dim-but-readable to
  keep the neon-noir identity — accent lights should *pop against* the baseline,
  not be the only illumination.
- Build it as one reusable piece (a spawnable "level lighting rig" actor or a
  documented template block placed per map) so Levels 3–5 inherit it trivially.
- Must not recolour or wash out the reserved gameplay colours (MISSION Hard
  Invariant on `ReservedGameplayColours` — lighting stays neutral/cool, gameplay
  colour semantics stay readable).

### REQ-3: Greybox floor and room shells (P0)
- The player must never see void: give each room a visible floor and simple wall
  shells, and give the connector paths between rooms a walkable, visible strip.
- Greybox/placeholder-first is explicitly fine (flat meshes + simple materials);
  no Blender asset work required. Either extend `ARoomActor` to spawn its own
  floor/shell geometry (preferred — every current and future room gets it for
  free) or place static meshes per map; the agent picks, but states the choice in
  the PR.

### REQ-4: Camera framing pass (P1)
- Expose the top-down camera's framing (`TargetArmLength`, boom pitch, FOV) as
  `EditAnywhere`/config-tunable properties on `AFlatCamera3DPrototypePawn` instead
  of constructor constants.
- Retune the defaults for readability at gameplay scale (closer than today's
  800cm/−80°; exact values are the implementer's judgment, stated in the PR).
- Automation test: defaults land within the newly documented ranges and the
  properties actually drive the spring arm.

### REQ-5: Compress and simplify Level 1 (P1)
- Level 1 is the teaching level: shrink the room spacing so the next room is
  visible from the previous one (no 30m dark treks), and thin the encounter to a
  gentle ramp (first room: 1–2 enemies near spawn; later rooms grow). Keep the
  existing room/target-zone/door topology pattern — move and retune, don't
  rebuild.
- Level 2 keeps its current scope but inherits REQ-1/REQ-2/REQ-3.

### REQ-6: Wayfinding to the next objective (P1)
- Make "where do I go" answerable at a glance: raise target-zone beacon
  visibility (taller light column, brighter placeholder marker — placeholder-first
  is fine) and mark door connectors visibly in-world.
- The HUD side already has a hook (`RefreshTargetZoneBeacons()` on
  `AKrowdKontrolPlayerController`, PR #133) — an on-screen or in-world direction
  cue may consume it, but in-world visibility is the P1 bar; HUD arrows are
  optional polish.

## Out of scope
- Final art, Blender-authored meshes, materials beyond greybox (asset pipeline is
  its own track).
- Niagara/VFX polish, post-process grading beyond basic readability.
- Levels 3–5 themselves (MISSION's 5-level decision) — they just inherit the rig
  and patterns from REQ-2/REQ-3.
- Minimap or full navigation UI.

## Existing surfaces to build on (do not reinvent)
`ARoomActor` / `DoorConnectorActor` / `APlaceholderTargetZoneActor` (which already
owns a `BeaconLightComponent`); `AFlatCamera3DPrototypePawn` (`CameraBoom` spring
arm); `L_FlatCamera3DPrototype`'s lighting stack as the reference for REQ-2;
`AKrowdKontrolPlayerController::RefreshTargetZoneBeacons()`; the level automation
tests' map-load pattern for REQ-1's guard test.
