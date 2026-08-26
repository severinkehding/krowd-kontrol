# PRD: Post-Run Progression — Next Level / Rerun from the Clear Screen

**Author**: operator (Severin), from the 2026-08-26 solo playtests (cleared L1
and L2 end-to-end).
**Feeds**: `dark-factory-prd-to-issues`. Extends `docs/prd-run-lifecycle.md`
(whose REQ-6 post-run summary display and REQ-7 run-completion signal are the
foundation this builds on).

## Problem

Clearing a level shows the post-run summary (clear time, personal best, Crowd
Mastery — the wired-up PR #302 screen), and then… nothing. There is no way to
continue: no next level, no rerun, no exit. The run dead-ends on the summary
and the operator has to leave PIE / ask an agent to set up the next level.
Additionally the summary's layout is wrong: the information renders in the
top-left corner instead of centred.

## Operator design decision (2026-08-26, locked — do not re-litigate at triage)

The clear screen becomes the run's junction point, with exactly this layout
contract:

- The summary information block sits **centred on screen** (not top-left).
- Below the information block: **[RERUN LEVEL]** and **[NEXT LEVEL]** buttons.
- The information is **above the buttons and never overlaps them** at the
  supported resolutions (1280x720 minimum through 3840x2160 — same envelope
  the HUD widgets already test against).

## Requirements

### REQ-1: Centre the post-run summary (P0)
- `UPostRunSummaryWidget`'s content block anchors centred (both axes), with
  the existing chrome rules (Hard Invariant 3 neutrals) unchanged.
- Resolution-safety test coverage in the same style as the quest tracker's
  envelope assertions (issue #310's test additions are the pattern to copy).

### REQ-2: Rerun button (P0) — ✅ implemented, issue #320
- Reloads the current level fresh (the defeat-restart flow, issue #223's
  PIE-prefix-safe map reload, is the machinery to reuse — clear-screen rerun
  and defeat-restart should share one code path).
- Keyboard/mouse both work; the in-game cursor (issue #262) is already
  visible on this screen.

### REQ-3: Next-level button (P0) — ✅ implemented, issue #321 / PR #335
(mechanism only — `LevelSequenceTable` has no populated Content DataTable asset yet,
so every real level currently shows the "FINISH RUN (More Levels Coming)" placeholder;
follow-up issue recommended for real table content)
- Advances to the next level in the shipped sequence (L_Level01 → L_Level02 →
  L_Level03 today; the sequence definition must be data, not hardcoded ifs,
  so L4/L5 slot in when they land).
- On the final shipped level, the button reads differently (e.g. "FINISH RUN")
  and routes to the main menu once `docs/prd-main-menu.md` lands; until then
  it may rerun the final level with a "more levels coming" label — pick the
  cheaper, honest placeholder.

### REQ-4: Button/summary layout integrity test (P1)
- Automation coverage that the info block and the two buttons never overlap
  and stay on-screen at min/max target resolutions.

## Out of scope

- Level select UI (that's `docs/prd-main-menu.md`).
- Any change to what stats the summary shows.
- Post-run rewards/unlock logic beyond what already exists.

## Existing surfaces to build on (do not reinvent)

- `UPostRunSummaryWidget` (+ its wiring test), `ULevelClearTimeSubsystem`,
  Crowd Mastery display (PR #302).
- Defeat-restart map reload (issue #223), run-lifecycle signals
  (`ULevelLifecycleSubsystem`).
- Level-sequence data groundwork (`LevelSequenceData.h` /
  `LevelSequenceSubsystem`) — verify current state at triage before assuming
  its shape; extend rather than duplicate if it fits REQ-3's data-driven rule.
