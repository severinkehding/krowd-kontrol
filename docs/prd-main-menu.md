# PRD: Main Menu — Start and Play Without an Agent in the Loop

**Author**: operator (Severin), 2026-08-26. Every playtest so far has required
Claude to launch the editor, load a map, and start PIE by hand.
**Feeds**: `dark-factory-prd-to-issues`. Pairs with
`docs/prd-post-run-progression.md` (the clear screen routes back here) and
`docs/prd-crowd-mastery-persistence.md` (the menu is where mastery lives).

## Problem

The game has no front end. There is no map that greets a player; starting a
session means opening the editor on a gameplay map directly. The operator
cannot simply "start the game and play through it" — a human (or agent) must
assemble every session. That blocks unattended playtesting, makes packaged
builds pointless, and gates everything a front end would host (level select,
mastery display, settings).

## Operator design decision (2026-08-26, locked — do not re-litigate at triage)

A proper main menu the project can grow into — not a throwaway. It is the
game's entry point: launch game → main menu → pick a level → play → clear
screen → back to menu or next level. The operator should never need an agent
to start a playtest again.

## Requirements

### REQ-1: Front-end map as the game's entry point (P0)
- A dedicated menu map (e.g. `/Game/Maps/L_MainMenu`) set as the project's
  game-default map, so launching the game (PIE from that map, `-game`, or a
  packaged build) lands in the menu — no editor choreography.
- Editor workflow unaffected: opening a gameplay map directly and hitting PIE
  still works for development.

### REQ-2: Level select (P0) — ✅ implemented, issue #325
- The menu lists the shipped levels (L1–L4 today) and starts the chosen one.
- Driven by the same level-sequence data as
  `docs/prd-post-run-progression.md` REQ-3 — one authority for "what levels
  exist and in what order," consumed by both the menu and the next-level
  button. Implemented as `ULevelSequenceSubsystem::GetShippedLevelMapNames()`,
  extending the authority issue #321 established rather than duplicating it.
- No lock/unlock gating yet: all shipped levels selectable (Alpha stance —
  gating is a future PRD once there's a reason for it).
- Known gap: the real `LevelSequenceTable` content `DataTable` asset doesn't
  exist yet (same gap #321 flagged), so the real in-game menu shows zero
  level buttons until that content-authoring follow-up lands. Automation
  tests inject their own in-code table to cover the logic in the meantime.

### REQ-3: Menu chrome and navigation (P0)
- Title, level select, a Quit button (quits cleanly in packaged/-game; exits
  PIE in editor). Mastery display slots in via its own PRD — leave an anchored
  region for it.
- Mouse-first (the in-game cursor work, issue #262, already gives us cursor
  UX); keyboard navigation is P2.
- HUD chrome rules apply (Hard Invariant 3 — neutral chrome, no reserved
  gameplay colours for decoration).

### REQ-4: Clear screen returns here (P1) — ✅ implemented, issue #326
- Once this map exists, `docs/prd-post-run-progression.md`'s final-level
  routing and any "back to menu" affordance target this menu.

## Out of scope

- Settings/options screens, save slots, difficulty selection.
- Skill tree UI (see mastery PRD — display only there, no spend).
- Packaging/distribution work itself (the menu must merely not block it).

## Existing surfaces to build on (do not reinvent)

- UMG widget lineage (`UPostRunSummaryWidget`/`UBriefingCardWidget` build
  their trees in C++ — same pattern).
- In-game cursor + click handling (issue #262).
- Level-sequence data (see progression PRD's note — verify shape at triage).
