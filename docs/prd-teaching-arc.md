# PRD: Level Progression & Teaching Arc

**Author**: operator (Severin), drafted with the interactive session from the
2026-08-22 live playtest. **Feeds**: `dark-factory-prd-to-issues`. Grounded in
MISSION.md (`01` core loop, `02` one-ability-unlock-per-level, `09` onboarding
folded into real levels — no separate tutorial level), the 5-level operator
decision (issue #69), and direct operator playtest findings. Related but separate:
PR #212 carries the operator's type-keyed zone ruling (zones accept by enemy type;
colour stays a bonus) — that ruling is #212's fix scope, not this PRD's.

## Problem — playtest findings

1. **Nothing advances the game.** Clearing a level fires `OnLevelClear` (merged),
   shows the summary (#175, in progress), and… stops. No system loads the next
   level. (Unlock-call half fixed by #217: `NotifyLevelReached` now fires via
   `UAbilityUnlockLevelSubsystem` on every level start, using an interim
   map-name-derived index. Level-advance mechanism now implemented, PR #298/#216
   — real `LevelSequenceTable` DataTable content and `L_Level03/04/05.umap` are
   the remaining gap.)
2. **Rooms don't gate.** The operator walked into room 2 and 3 of L_Level01
   without touching room 1; with escalate-only detection and no de-aggro, this
   snowballed every enemy in the level into one mob converging on spawn
   (verified live: Bombers from x≈4800 ended up behind the player at x≈−366).
3. **Nothing teaches.** A new player gets no instruction at all. MISSION forbids
   a separate tutorial level — teaching must live inside Level 1 itself.
4. **Cause→effect feedback lags.** The operator felt a large delay between a
   Bomber reaching them and the HUD energy dropping. The energy wiring itself is
   event-driven (same-frame broadcast), so the gap is the Bomber's attack
   wind-up/fuse with an under-communicated telegraph, plus possibly widget
   presentation. For a teaching game, damage cause→effect must read instantly.

## Requirements

### REQ-1: Level advance + ability unlock on clear (P0) — ✅ mechanism implemented, PR #298 (issue #216); real LevelSequenceTable content + L_Level03/04/05 still pending
- After level-clear (and summary dismissal once #175 lands), load the next level
  in the run sequence (L_Level01 → L_Level02 → … per MISSION's 5-level
  decision; final level instead fires the existing run-complete path).
- On arriving in level N, call `NotifyLevelReached(N)` — the merged unlock
  mapping (2→Sleep, 3→Root, 4→Fear, 5→Snare) does the rest; level 1 is its
  documented no-op. The sequence lives in config, not hardcoded per-map C++.
- Automation tests: clear→advance loads the configured next map; unlock signal
  fires with the arrived level's index; final level routes to run-complete and
  does not advance.

### REQ-2: Room gating (P0) — ✅ implemented, PR #229
- A room's exit door(s) stay closed/impassable until every enemy belonging to
  that room is Banked; then they open (visibly — the existing door markers are
  the surface). Entry doors never re-close behind the player.
- "Belonging to that room" derives from `ARoomActor::OwnedEnemies`, auto-discovered
  in `BeginPlay()` via nearest-room-by-distance over real `AEnemyBase` actors (issue
  #218) — not global enemy counts, so wave-spawned enemies (via `AddOwnedEnemy()`)
  gate their own room.
- This also bounds the no-de-aggro snowball: enemies can still converge within a
  room, but a fresh room's population cannot join until the player opens it.
- Automation tests: door blocked while any owned enemy is un-Banked; opens on
  last bank; wave-spawned additions re-gate until banked.

**Implementation note**: this does *not* use `RoomEnemyBudgetController`
(issue #82) — that component is a separate, placeholder-actor spawn/budget
mechanism with its own `FOnRoomCleared` delegate, unwired to any real banking
trigger. Room-cleared signal for REQ-3's prompt should bind to
`ARoomActor::OnRoomClearedStateChanged`, not `RoomEnemyBudgetController::OnRoomCleared`.

### REQ-3: No-waffle instruction prompts (P1) — ability-unlock half ✅ implemented, issue #220
- Short, imperative, contextual on-screen instructions in Level 1, one at a
  time, each dismissed by the player doing the thing:
  "STUN IT — PRESS 1" (first hot enemy) → "IT FOLLOWS YOU — WALK" (first
  control, once herding #214 exists) → "DROP IT ON THE GLOWING PEN" (first
  controlled enemy near a zone) → "ROOM CLEAR — DOOR OPEN" (first gate opens).
- Build on the existing on-screen prompt widget (the mismatch-nudge's surface)
  and the existing first-Stun hooks (Gizmo bark, beacon flash) — no new UI
  framework. Later levels get exactly one prompt per newly unlocked ability
  ("SLEEP — PRESS 2 — STRONG VS SNIPERS"), which is where colour-matching gets
  taught, as a bonus, per the operator's teaching-order decision.
- Prompts fire once per save/run, never re-nag.

### REQ-4: Instant, legible damage feedback (P1)
- The Bomber's attack telegraph must clearly read as "about to explode" for its
  entire fuse (the tell light exists; make its escalation unmistakable —
  placeholder-quality flash/scale ramp is fine), so the delay the operator felt
  becomes a readable wind-up instead of a mystery gap.
- On the frame energy actually drops: an immediate HUD reaction on the energy
  meter (flash/pulse — placeholder-quality) so cause→effect is simultaneous to
  the player. The underlying event wiring is already same-frame; this is
  presentation.
- Automation tests where feasible (telegraph state flags, meter reaction hook
  fires on `OnEnergyChanged`); visual quality is playtest-verified.

## Out of scope
- The type-keyed zone acceptance ruling and zone spawn-position fix (PR #212's
  fix pass, already ruled and documented there).
- Herding movement itself (#214, queued) and per-follower separation (#215).
- Level-select UI, save-slot UI, difficulty settings.
- Narrative content beyond the existing Gizmo bark hooks.

## Existing surfaces to build on (do not reinvent)
`ULevelLifecycleSubsystem` (`OnLevelClear`, final-map/run-complete config);
`ULevelSequenceSubsystem` (`LevelSequenceTable` DataTable config,
`ComputeNextLevelMapName()`, issue #216 — resolves the next map name on
`OnLevelClear`, or sets `FinalMapName` at the sequence's end; the actual
`OpenLevel()` travel moved to a separate caller-triggered `AdvanceToNextLevel()`,
issue #321, to avoid racing the post-run summary screen);
`UAbilityUnlockComponent::NotifyLevelReached` (mapping merged; now called via
`UAbilityUnlockLevelSubsystem`, #217 — level index is still map-name-derived,
pending #217's reconciliation with `ULevelSequenceSubsystem`, issue #217);
`ARoomActor` (`OwnedEnemies`/`OnRoomClearedStateChanged`, issue #218) /
`ADoorConnectorActor` (+ door markers, `GatingRoom`/`RefreshGateState`);
the on-screen prompt widget + `AbilityMatchupNudgeComponent` pattern;
`AbilityUnlockPromptComponent` (issue #220, the ability-unlock half of REQ-3);
`GizmoFirstContactComponent` / `FirstStunBeaconComponent` (first-cast hooks);
`ABomberEnemy`'s `AttackTellLightComponent` + explosion path;
`UEnergyMeterWidget` (`OnEnergyChanged` binding); `UPostRunSummaryWidget` (#175).
