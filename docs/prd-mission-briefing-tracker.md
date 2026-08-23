# PRD: Mission Briefing & Live Quest Tracker

**Author**: operator (Severin), from the 2026-08-22 live co-op playtests.
**Feeds**: `dark-factory-prd-to-issues`. Grounded in MISSION.md (`09` onboarding
inside real levels, `13` HUD rules incl. the five-colour reservation, `07`
Gizmo-bark narrative frame) and the operator's direct experience: both
playtests required a human outside the game to explain what the level wanted —
the game itself never says.

## Problem

Nothing in-game tells the player the objective. A fresh player has no idea the
goal is "control enemies and deliver them to pens," where the pens are, which
enemies remain, or which ability suits what. The operator has been receiving
mission briefings over chat; the game should deliver them itself.

## Operator design decision (2026-08-22, locked — do not re-litigate at triage)

Two connected pieces:
1. **A pre-level mission briefing** shown when a level starts — what this level
   asks, in a few no-waffle lines — dismissed into…
2. **A compact, persistent quest tracker** during play, comparable to a WoW
   quest tracker: a small corner panel (never dominating the screen) that
   live-tracks objectives and progress.

## Requirements

### REQ-1: Pre-level briefing card (P0) — ✅ implemented, issue #246 / PR #272
- On level-begin (the merged `ULevelLifecycleSubsystem::OnLevelBegin`), show a
  briefing overlay: level name, the objective in imperative one-liners (e.g.
  "PACIFY ALL 8 ROBOTS — STUN THEM, HERD THEM TO THEIR PENS"), and any newly
  unlocked ability for this level ("NEW: SLEEP — RMB — STRONG VS SNIPERS").
- Dismissed by any input or a short auto-timeout; play is paused or safe while
  it shows (coordinate with the room-encounter countdown PRD's prep flow —
  briefing first, countdown on room entry).
- Briefing content is data-driven per level (config/data asset), not
  hardcoded C++ strings per map.

### REQ-2: Persistent compact quest tracker (P0) — ✅ implemented
(banked-count line: issue #247/PR #271; suggested-ability line: issue #249/PR #287;
current-room-state line: issue #248/PR #289; directional cue: issue #250/PR #301)
- A small anchored panel (a corner; think WoW quest tracker scale — must not
  take meaningful screen space away from play) listing the level's live
  objectives with progress, updating in real time off existing events:
  - "Robots penned: 3/8" (from `ATargetZone::OnActorBanked` / room-cleared
    state — the merged banking chain).
  - Current room state: "Room 2 — 1 robot left" / "DOOR OPEN" (from
    `ARoomActor::OnRoomClearedStateChanged` + door gating, PR #229), with a
    trailing directional-cue glyph toward the objective (REQ-3).
  - Which ability to use: the tracker names the suggested ability per remaining
    enemy type — colour-matched suggestion when that ability is unlocked
    ("SNIPERS → SLEEP (RMB)"), otherwise the universal fallback
    ("ANY ROBOT → STUN (LMB)").
  - **Ratified (operator, 2026-08-23)**: key-binding display always uses the
    canonical OG-GDD bindings (LMB=Stun, RMB=Sleep, Q=Root, E=Snare, MMB=Fear)
    via `AbilityData::KeyBindingLabel`, never the legacy 1-5 numbers — same
    ruling recorded in `docs/prd-ability-tray-ux.md` REQ-2.
- Event-driven updates only (no per-frame polling) — matches the HUD's
  existing event-binding convention (`UEnergyMeterWidget` lineage).
- HUD chrome obeys Hard Invariant 3: panel chrome uses the existing neutral
  `HUDChromeColours`; the five reserved colours appear only as genuine
  information (e.g. the suggested-ability swatch).

### REQ-3: Direction hint, minimal form (P1) — ✅ implemented, issue #250 / PR #301
- "Where" at the cheapest useful level: the tracker's active line carries a
  simple directional cue toward the current objective (e.g. an arrow glyph
  toward the next un-cleared room's door or the pen area) — not a minimap, not
  a nav system. Reuse the beacon/world-marker knowledge that already exists
  (target-zone actors, door markers) to resolve the direction.

### REQ-4: Camera default zoom-out retune (P0, small)
Operator decision from the same playtests: the shipped framing (arm length
clamped 300–600) is far too close for crowd management; the live sessions ran
at ~1600 (≈2.7×) which was slightly much. **New default: arm length 1500
(≈2.5× the old max), FOV 90**, with the `EditAnywhere` clamp ranges on
`AFlatCamera3DPrototypePawn` widened to make the new default legal (e.g.
600–2000). Update `KrowdKontrol.Unit.FlatCamera3DPipelineCameraFraming`'s
documented bounds to match. Pure retune of issue #188's surfaces — no new
mechanics.

## Out of scope
- Minimap or full navigation UI (explicitly rejected scale).
- Voice/narrative delivery of briefings (Gizmo barks stay their own track; a
  briefing card may *quote* bark text later).
- Quest content beyond the core loop (no side objectives yet).
- Pause-menu/objectives-screen duplication.

## Existing surfaces to build on (do not reinvent)
`ULevelLifecycleSubsystem::OnLevelBegin`; `ATargetZone::OnActorBanked` +
`ARoomActor` cleared/ownership state (PR #229) and door markers;
`UAbilityUnlockComponent`/`UAbilityUnlockLevelSubsystem` (#217) for the
new-ability line; `AbilityData` (names, keys, colours, countered types) for
suggestions; the HUD widget creation/wiring pattern in
`AKrowdKontrolPlayerController` (issue #132) and `HUDChromeColours`; the
on-screen prompt widget as the interim rendering vocabulary.
