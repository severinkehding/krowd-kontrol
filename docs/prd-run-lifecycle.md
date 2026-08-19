# PRD: Run Lifecycle & Progression Signals

**Author**: operator (Severin), drafted with the interactive session, 2026-08-19.
**Feeds**: `dark-factory-prd-to-issues`. Grounded in MISSION.md P0/P1 scope (`01`, `05`,
`06`, `16`) and the gaps repeatedly hit by closed issues #4, #5, #7, #46, #154's PR
review, and the Drain boss chain — none of which could proceed because the game has no
lifecycle events. This PRD defines them.

## Problem

The codebase has enemies, bosses, banking, levels, HUD, energy damage, and a merged
clear-time tracking/persistence layer (`ULevelClearTimeSubsystem`, PR #154 lineage) —
but **no concept of a level beginning, being cleared, or being failed**. Nothing can
start/stop timers, detect a win, punish a loss, unlock the next level, or complete a
run. Every downstream feature (personal bests, summary screen, New Game+, boss
checkpointing) is blocked on these signals existing.

## Requirements

### REQ-1: Level-begin and level-clear signals (P0)
A world-level system (e.g. a `UWorldSubsystem`) that:
- Fires a **level-begin** event at world begin-play on gameplay maps (keyed by map
  name; prototype maps included is acceptable at first).
- Fires a **level-clear** event when every spawned `AEnemyBase` in the world has
  reached `Banked` (terminal state), given at least one enemy existed. Enemies spawned
  later (wave spawners) extend the requirement — clear only fires when *all* live
  spawned enemies are Banked and no spawner has pending waves
  (`UWaveSpawnerComponent::IsWaveTimerActive()` exposed state).
- Both events are broadcast (dynamic multicast) so any system can subscribe.
- Automation tests drive spawn→bank sequences and assert both events fire exactly once
  and in order.

### REQ-2: Wire clear-time tracking to the signals (P0)
`ULevelClearTimeSubsystem::StartLevelTimer` / `StopLevelTimerAndRecordClear` (already
merged: timer, per-level personal best, USaveGame persistence) must be invoked by
REQ-1's events — level-begin starts the timer for the current map, level-clear stops it
and records. After this lands, a real PIE playthrough of a level must produce a save
file in `Saved/SaveGames/` with the recorded time (the exact evidence PR #154's E2E
found missing).

### REQ-3: Player defeat / level-fail signal (P0)
`UPlayerEnergyComponent` reaching 0 energy currently does nothing. Define and implement
the fail state:
- A **level-failed** event fires when the possessed pawn's energy reaches 0.
- Consistent with MISSION's non-lethal identity, the player is *incapacitated*, not
  killed: input is disabled and the fail event fires; what happens next is REQ-4's
  restart flow. No death animation/ragdoll needed (placeholder-first).
- The clear timer for the level is discarded (a failed run never records a best).

### REQ-4: Level restart flow (P1)
On level-failed: restart the current level (map reload is acceptable placeholder
behavior), returning the player to the level's start with full energy and the level's
enemy population reset. Boss-fight re-entry (closed issue #46's ask): when a level
contains a boss encounter, restarting after a fail returns to the boss encounter, not
all the way to the level's beginning — implementable as a simple checkpoint flag
("boss reached") on the lifecycle subsystem, honored by the restart flow.

### REQ-5: Crowd Mastery tracking (P1)
Closed issue #4's ask, now unblocked by this PRD's signals and PR #154's persistence
pattern: track the largest number of *simultaneously Controlled* enemies during a
level (sampled on each cast application via `OnAbilityCastApplied` and on
Controlled-state expiry), persist the per-level best in the same
`ULevelClearTimeSaveGame` (extend it), and reset per level-begin.

### REQ-6: Post-run summary display wiring (P1)
Closed issue #5's ask: on level-clear, show the already-merged
`UPostRunSummaryWidget` populated with the real clear time (this run), the persisted
personal best, and the run's Crowd Mastery value (REQ-5) — replacing its current
placeholder values. Dismissible; dismissal proceeds to REQ-7 where applicable.

### REQ-7: Run completion signal (P2 — foundation only)
A **run-complete** event that fires on clearing the final level of the run sequence
(for now: a configurable "final map" name; the full 5-level sequence per MISSION's
level-progression decision arrives with Levels 2–5). Closed issue #7 (Overclock
unlock) subscribes to this later; this PRD only requires the event to exist and be
tested, not any consumer.

## Out of scope
- New Game+ / Overclock Mode itself (#7/#9 — P2, consumes REQ-7 later).
- Co-op variants of any signal (P2 tier).
- Level-select / save-slot UI.
- Punishment states (separate PRD).

## Existing surfaces to build on (do not reinvent)
`AEnemyBase` Banked state + `OnEnemyBanked`; `ABossBase::OnBossBanked`;
`UWaveSpawnerComponent`; `ULevelClearTimeSubsystem` + `ULevelClearTimeSaveGame`;
`UPlayerEnergyComponent`; `UPostRunSummaryWidget`; `UAbilityCastComponent::OnAbilityCastApplied`;
`AKrowdKontrolPlayerController`/GameMode (HUD wiring pattern, PR #133).
