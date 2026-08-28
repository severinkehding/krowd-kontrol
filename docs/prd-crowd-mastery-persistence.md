# PRD: Crowd Mastery Persistence — Points That Add Up, Visible in the Menu

**Author**: operator (Severin), 2026-08-26.
**Feeds**: `dark-factory-prd-to-issues`. Depends on `docs/prd-main-menu.md`
(display surface) and builds on the existing Crowd Mastery tracking
(`docs/prd-run-lifecycle.md` REQ-5, wired to the post-run summary in PR #302).

## Problem

Crowd Mastery is earned per run and shown on the clear screen, then vanishes.
Nothing accumulates, so the meta-reward loop MISSION.md points at has no
substance: there is nowhere to see your total and no sense of progression
across runs.

## Operator design decision (2026-08-26, locked — do not re-litigate at triage)

- Mastery points **accumulate across runs and persist for the session** (a
  session = one game/editor launch). Full cross-launch save-file persistence
  is fine to include if the existing save machinery makes it cheap, but
  session persistence is the P0 bar.
- The **main menu shows the running total**, and offers a **reset** control
  that zeroes it (with a confirm — it's destructive).
- **No skill tree yet, no spending.** Watching the number add up is the
  entire scope. (Spending/skill tree is a deliberate future PRD.)
- **Future, explicitly out of scope now**: syncing this state to Steam once
  the platform hookup exists. Design the storage so a future sync layer wraps
  it rather than rewrites it (one authority owning the total, everything else
  reading through it).

## Requirements

### REQ-1: Session-persistent mastery total (P0)
- A GameInstance-scoped authority (subsystem) owns the accumulated total;
  per-run mastery (existing tracking) deposits into it on level clear.
- Survives level transitions, reruns, and returns to menu within one launch.
- Unit coverage: deposit-on-clear, accumulation across two simulated runs,
  and reset.

### REQ-2: Menu display (P0) — ✅ implemented, issue #328
- The main menu's reserved region (main-menu PRD REQ-3) shows
  "CROWD MASTERY: <total>", updating on return from a run.
- Chrome rules: neutral chrome; if the existing HUD has an established
  mastery colour/style on the clear screen, reuse it — do not invent a new
  one.

### REQ-3: Reset control (P1)
- A menu control that zeroes the accumulated total after an explicit confirm
  step. No partial resets.

### REQ-4: Save-file persistence (P2 — only if cheap)
- If the existing save machinery (the lifecycle/PIE tests reference a save
  file) already fits, persist the total across launches behind the same
  authority. If it needs new machinery, defer — session persistence is the
  bar, and Steam sync will revisit storage anyway.

## Out of scope

- Skill tree, spending, unlock gating of any kind.
- Steam/platform integration (future PRD once hooked up).
- Rebalancing how much mastery a run awards.

## Existing surfaces to build on (do not reinvent)

- Crowd Mastery run tracking + summary display (run-lifecycle REQ-5, PR #302).
- `ULevelLifecycleSubsystem::OnLevelClear` as the deposit trigger.
- The save-file usage referenced by the lifecycle PIE tests (issue #238) —
  check its shape before deciding REQ-4's cost.
