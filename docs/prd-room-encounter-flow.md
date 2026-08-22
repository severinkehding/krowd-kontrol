# PRD: Room Encounter Flow (containment, room-scoped aggro, entry countdown)

**Author**: operator (Severin), from the 2026-08-22 live co-op playtest of
L_Level01. **Feeds**: `dark-factory-prd-to-issues`. Grounded in MISSION.md (`01`
escalating room-by-room crowd management, `05` room/connector level structure)
and direct playtest observations. Builds directly on the merged room-gating
system (issue #218 / PR #229) — this PRD is about what happens *inside and
around* a room, where gating only covered the exit door.

## Problem — playtest findings

1. **Enemies aggro through walls.** Detection is a pure proximity radius
   (`DetectionRangeUnits`, no line-of-sight, no room scoping), so enemies in the
   next room aggro before the player has even reached the door — the operator
   was being hunted through solid walls by rooms they hadn't entered.
2. **Rooms don't physically contain.** The gated door blocks the doorway, but
   the room perimeters aren't sealed — the operator simply walked *around* the
   door, breaking out of the room bounds and into the next room, bypassing the
   entire gating mechanic (and its "clear the room to advance" teaching beat).
3. **No preparation moment.** Even entering legitimately, the room's enemies
   are already converging mid-doorway. The operator's design: entering a fresh
   room should grant a short, visible prep window before the encounter goes
   live.

## Operator design decision (2026-08-22, locked — do not re-litigate at triage)

On first entry to an un-cleared room, a **3-second on-screen countdown** runs;
the room's enemies hold (stay Idle, no detection) until it expires, then the
room activates and its enemies engage. Once per room per run.

## Requirements

### REQ-1: Sealed room perimeters (P0)
- Close the greybox shells so the gated doorway is the only way between rooms:
  no walkable gaps at room corners/edges, and the connector corridors are
  walled so the player cannot leave them sideways.
- The player must never be able to stand in a room the door gating says they
  cannot enter. Manual walk-the-perimeter verification in PIE plus an
  automation check where feasible (e.g., collision-blocking assertions across
  the perimeter, or a `KrowdKontrol.PIE.` scenario sweep once that tier —
  issues #236–#240 — lands).

### REQ-2: Room-scoped aggro (P0) — ✅ implemented, PR #274
- An enemy may only begin detection (Idle → Alert) while the player is inside
  its own room — walls stop being transparent to aggro. Derive "its own room"
  from the merged ownership model (`ARoomActor::OwnedEnemies`, issue #218) and
  the same player-in-room resolution the door gating already uses; do not
  build a perception/line-of-sight system for this.
- Enemies already Alert stay Alert (escalate-only detection is unchanged —
  this scopes *onset*, not de-aggro).
- Automation tests: player outside the room → owned Idle enemy never alerts at
  any distance; player inside → existing proximity rules apply unchanged.

### REQ-3: Room-entry countdown (P0)
- Per the locked decision: first entry into an un-cleared room starts a
  visible 3-second countdown (on-screen, big and unambiguous — the existing
  on-screen prompt widget surface is acceptable placeholder rendering; a
  number counting 3 → 2 → 1 is the bar).
- While counting: the room's owned enemies hold Idle (their detection gate
  from REQ-2 stays closed) and the player can move/prep freely.
- At zero: the room activates — REQ-2's gate opens and the room's enemies run
  detection normally (with the player inside, they will typically alert
  immediately and engage, matching the operator's "enemies will run at you"
  intent).
- Fires once per room per run; re-entering a cleared or already-activated room
  never re-triggers it. Configurable duration (default 3.0s).
- Automation tests: countdown starts on first entry only; enemies hold during
  it; activation on expiry; no re-trigger after clear/re-entry.

## Out of scope
- De-aggro / leashing (escalate-only stays the rule; #218's gating bounds the
  snowball at room granularity).
- Line-of-sight or perception systems (room membership is the scope, not
  raycasts).
- Audio/visual countdown polish beyond a clearly readable number (art pass
  later).
- Wave-spawner interaction changes (spawned enemies already join room
  ownership via `AddOwnedEnemy`; they obey the same room-scoped gate).

## Existing surfaces to build on (do not reinvent)
`ARoomActor` (`OwnedEnemies`, nearest-room ownership, `OnRoomClearedStateChanged`)
and `ADoorConnectorActor`'s player-side resolution (PR #229);
`AEnemyBase::TickCheckDetection` (the Idle→Alert gate to scope) /
`DetectionRangeUnits`; the greybox shells from issue #187 / PR #193 (REQ-1
extends them); the on-screen prompt widget surface (mismatch-nudge lineage) for
the countdown rendering; the `KrowdKontrol.PIE.` tier (issues #236–#240) for
scenario-level verification once available.
