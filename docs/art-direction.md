# Art Direction: Low-Poly Mars Colony (operator session, 2026-08-30)

Reference doc, not a PRD — records the art direction the operator locked in and how
the levels are built, so factory work respects and extends it instead of fighting it.

## The direction

The game is set in a **low-poly Mars colony**, styled on ITHappy's *Mars Colony*
and *Mars Spaceport* packs (purchased 2026-08-29, imported under
`/Game/Space_Station_2/` and `/Game/Space_Station_1/` respectively). The play
strip — the row of gameplay rooms — sits on a flat tech-tile apron
(`SM_tile_platform_*`) carved out of the Martian terrain, with the colony
(hangars, domes, spaceport, terrain) transplanted from the packs' own
demonstration maps into districts surrounding the strip on all sides.

Per-level variation (all five levels rebuilt this way):

| Level | Flavour | Sources |
|-------|---------|---------|
| L1 | Brown-Mars colony + spaceport mix | hand-picked districts |
| L2 | Spaceport-heavy (hangars, rail, vehicles) | spaceport + brown |
| L3 | Dense brown city | brown + one spaceport slot |
| L4 | Polar/snow colony (cool fog tint) | snow + spaceport |
| L5 | Grand finale mix | brown + spaceport + snow rotating |

## How it's built (and rebuilt)

Everything is scripted and idempotent — headless
`UnrealEditor-Cmd.exe -run=pythonscript` scripts in `app/Saved/Scripts/`:

- `dump_demo_maps.py` / `dump_layouts.py` — harvest the packs' demo maps into
  placement recipes (`app/Saved/EnvKit/demo_{brown,snow,spaceport}.json`) and the
  levels' gameplay layouts (`level_layouts.json`).
- `build_l1_world.py` / `build_worlds_2to5.py` — delete previous transplant
  actors (label prefixes `MarsT_`, `RoomDress_`, `EnvCageAuto_`), transplant
  district clusters around each level's play strip, lay the tile apron, cycle
  room wall materials, place cage pens at target zones, dress room interiors
  with pack props, and apply the demo lighting recipe (sun intensity π, pitch
  −31.34°/yaw 36.41°, skylight 1.0, dusty height fog + clamped auto-exposure).
- `dress_rooms.py` — L1's interior dressing (generalised version lives inside
  `build_worlds_2to5.py`).

Key rules baked into the scripts:

- **Play-strip protection**: a transplant is skipped when its conservative world
  AABB overlaps the strip (rooms ± 1700) AND its top exceeds z = −25. Center-only
  checks are NOT enough — pack terrain meshes are huge and poke through floors.
- **Commandlet spawning**: `spawn_actor_from_object` returns None in commandlet
  mode; use `spawn_actor_from_class(StaticMeshActor)` + set `static_mesh`.
- **Ground**: `SM_tile_platform_001/002/003/006` at z = −14 (top just under the
  room floors); never `SM_land_*` — those have baked-in dune relief.
- **Spaceport districts** sit further out (±8400 vs ±7200) — their hangar meshes
  are colossal and otherwise loom over the strip.

## Palette

Room surfaces are C++ properties on `ARoomActor` (`FloorMaterial`/`WallMaterial`,
see RoomActor.h rationale) pointing at `/Game/EnvKit/Materials/MIC_Kit*`:
floor slate `(0.28,0.30,0.34)`; walls steel-blue `(0.42,0.50,0.60)`, warm sand
`(0.62,0.52,0.38)`, rust terracotta `(0.55,0.34,0.26)` — cycled per room, all-steel
on the polar L4. Hard Invariant 3 still applies: the five reserved gameplay colours
stay reserved; environment colours are desaturated.

## Character art (same session)

- Player pawn: Fab "Cute Robot" skeletal mesh (`/Game/CuteRobot/`) with
  `UCosmeticLocomotionComponent` procedural bob (rig ships without clips;
  retarget parked as issue #395).
- First L1 enemy: Paragon Wukong at 1.3× with Idle/Jog clips driven by the same
  component's speed-hysteresis mode.
