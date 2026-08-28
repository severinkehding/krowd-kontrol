# PRD: Levels 4 and 5 — Concluding the Demo

**Author**: operator (Severin), 2026-08-28.
**Feeds**: `dark-factory-prd-to-issues`. Completes the five-level Alpha arc
MISSION.md's difficulty ramp requires; Levels 1–3 shipped (issues #42/#43/#45
lineage), 4 and 5 are the missing tail. The operator wants the demo
concludable end-to-end from the main menu.

## Problem

The demo dead-ends after Level 3. The level-sequence data, next-level button,
menu level-select, and final-level→menu routing all shipped and are data-driven
— they are waiting on exactly two more level assets. Ability unlocks are also
mapped to levels 4 (Fear) and 5 (Snare) in `UAbilityUnlockLevelSubsystem`, so
those two abilities currently never unlock in a real run.

## Operator design decision (2026-08-28, locked — do not re-litigate at triage)

Two hand-authored levels in the established greybox pattern, continuing the
ramp: strictly more rooms and enemies than Level 3, using the full enemy
roster, concluding with Level 5 as the demo's finale. On Level 5's clear, the
existing final-level routing returns to the main menu (already shipped —
`docs/prd-post-run-progression.md` REQ-3, issue #326).

## Requirements

### REQ-1: Level 4 (P0)
- Hand-authored `/Game/Maps/L_Level04` on the ARoomActor/ADoorConnectorActor
  foundation, following the shipped authoring pattern (headless pythonscript
  commandlet, same as #42/#43/#45).
- Strictly exceeds Level 3's room and enemy counts; all four enemy types
  present; type-keyed zones per room per the shipped banking rules.
- Registered in `DT_LevelSequenceTable` after Level 3 (menu + next-level
  button pick it up with zero code changes — that is the point of the data).
- Fear unlocks here per the existing `UAbilityUnlockLevelSubsystem` mapping;
  the briefing/unlock prompt flow must announce it (see the open briefing
  regression bug — coordinate, don't duplicate).
- Structure test in the KrowdKontrolLevel0NTest lineage (room/zone/density
  assertions via `LevelStructureTestUtils`).

### REQ-2: Level 5 (P0)
- Same pattern; strictly exceeds Level 4; Snare unlocks here. Registered
  after Level 4 in the table.
- As the demo finale, its clear screen exercises the shipped FINISH-RUN →
  main-menu routing (no new code expected — verify, don't rebuild).
- Structure test as above.

### REQ-3: Ramp sanity pass (P1)
- One documented tuning pass over the five-level sequence (enemy counts,
  room counts, unlock pacing) recorded in the changelog so the operator can
  playtest the ramp against stated intent.

## Out of scope

- The final boss (Drain) — issue #54 remains parked on its own operator
  design decision (Hard Invariant 2 exception); these levels must not
  pre-empt it.
- New mechanics, new enemy types, narrative content, real art.
- Onboarding encounters for the newly unlocked abilities (issue #31's scope —
  it becomes reopenable once these levels exist; note that at triage).

## Existing surfaces to build on (do not reinvent)

- Level-authoring commandlet pattern + `LevelStructureTestUtils` (issues
  #42/#43/#45, memory: headless pythonscript authors content assets).
- `DT_LevelSequenceTable` (`/Game/Data/`), `ULevelSequenceSubsystem`,
  menu level-select (#325), final-level routing (#326).
- `UAbilityUnlockLevelSubsystem`'s existing 4→Fear / 5→Snare mapping.
