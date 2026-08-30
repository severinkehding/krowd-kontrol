# Issue #380: Full-Respec Integration for Mastery RESET/CONFIRM

## Summary

Wires `UCrowdMasteryTotalSubsystem::RefundAllAndClearUnlocks()` (issue #371, already
shipped) into `UMainMenuWidget`'s existing RESET → CONFIRM flow (issue #329), so
confirming a reset does a full respec: refund every spent point and clear every
unlocked bubble, in that pinned order, before the existing `ResetAccumulatedTotal()`
call zeroes the earned total (unchanged semantics for the total itself).
`HandleMasteryResetConfirmClicked()` now records the call order into a new
`LastMasteryRespecCallOrder` test-observability seam (mirrors
`LastSelectedLevelMapName`'s precedent), and — if the MASTERY tree screen is
currently open — calls `UMasteryScreenWidget::RefreshAfterRespec()` so its points
display updates immediately, without requiring BACK + re-open.

Modifier-slot clearing (the other half of `docs/prd-mastery-skill-tree.md` REQ-5) is
out of scope — issue #376 (modifier catalog / 2-slot data model) is **still OPEN, not
merged, as of 2026-08-31** (independently verifiable: `gh issue view 376 --json
state` → `"state":"OPEN"`), so there is no slot state to clear yet; the issue's own
Notes section explicitly anticipates and directs this P0-subset scoping.

## This is a re-implementation of a rejected PR

PR #399 already built this correct logic once, but was rejected in validation
because its `app-source-tracked/` mirror step also swept in issue #374's entire,
unrelated, still-in-progress tree-render implementation (`PopulateTreeContent()`,
`TreeCanvas`, `RefreshBubbleStates()`, `HandleBubbleClicked()`,
`MasterySkillBubbleWidget`) into the same PR, undisclosed. This is the third time
this exact failure mode has hit this shared-`app/`-symlink setup (PR #345, #387,
#399) — see `.factory/decisions.md` D-009 and `CLAUDE.md`'s Environment section for
why `app/` is a single shared workspace across all concurrent factory worktrees.

This PR was written specifically to avoid that recurrence: issue #374's worktree
(`worktrees/archon/task-fix-issue-374`) was confirmed to have live, uncommitted
changes to `MasteryScreenWidget.cpp`/`.h` (plus new, untracked
`MasterySkillBubbleWidget.*` files) at implementation time, and this PR's diff was
constructed to exclude every line of that content.

## `app/` vs. `app-source-tracked/` divergence (read this before reviewing)

Unlike this repo's usual convention (see issue #373's changelog: "verified
byte-identical"), **`app/`'s and `app-source-tracked/`'s copies of
`MasteryScreenWidget.h`/`.cpp` are intentionally NOT byte-identical** for the
duration issue #374 is in flight:

- **`app/Source/KrowdKontrol/MasteryScreenWidget.h`/`.cpp`** (the real, live,
  Editor-built copy on the shared Windows host) already contains #374's
  in-progress tree-render content (`PopulateTreeContent()`, `TreeCanvas`,
  `BubbleWidgets`, `RefreshBubbleStates()`, `HandleBubbleClicked()`) plus a
  `RefreshAfterRespec()` function left over from PR #399's superseded attempt at
  this same issue that already calls `RefreshBubbleStates()` in addition to
  `RefreshPointsDisplayText()`. **This PR does not edit these two files in `app/`
  at all** — they already compile and work as-is, and editing them risks
  clobbering #374's live, unpushed work.
- **`app-source-tracked/Source/KrowdKontrol/MasteryScreenWidget.h`/`.cpp`** (this
  PR's own tracked contribution) is spliced — not copied — onto the clean,
  pre-#374 baseline already on `main`: exactly one new `RefreshAfterRespec()`
  method (calling only `RefreshPointsDisplayText()`, since bubble-visual content
  doesn't exist at this issue's own scope) and one new friend-class line
  (`friend class FKrowdKontrolMainMenuMasteryResetTest;`).

Both versions are correct for their own context: `app/`'s live superset
implementation runs the actual game and harness tests today (it's a harmless
extra behavior since #374's tree genuinely exists in the live workspace), and
`app-source-tracked/`'s minimal version is what this PR discloses and owns.
`MainMenuWidget.h`/`.cpp` and the rewritten test file have zero overlap with #374's
work and are copied wholesale, byte-identical between `app/` and
`app-source-tracked/` as usual.

## Files changed

| File | Action | What it contains |
|------|--------|-------------------|
| `Source/KrowdKontrol/MainMenuWidget.h` | UPDATE | New `LastMasteryRespecCallOrder` test-observability seam (ordered call log, mirrors `LastSelectedLevelMapName`) |
| `Source/KrowdKontrol/MainMenuWidget.cpp` | UPDATE | `HandleMasteryResetConfirmClicked()` now calls `RefundAllAndClearUnlocks()` before `ResetAccumulatedTotal()`, records call order, and refreshes an open tree screen via `RefreshAfterRespec()` |
| `Source/KrowdKontrol/MasteryScreenWidget.h` / `.cpp` (`app-source-tracked/` only, spliced not mirrored) | UPDATE | Adds `RefreshAfterRespec()` (calls `RefreshPointsDisplayText()`) + one friend-class line |
| `Source/KrowdKontrol/Private/Tests/KrowdKontrolMainMenuMasteryResetTest.cpp` | UPDATE | Rewrites blocks (e2)/(e3): full respec with real spend/unlock + live tree-screen refresh, and no-open-screen guard — both `#374`-independent (no `MasterySkillBubbleWidget`/`PopulateTreeContent` references) |
| `docs/prd-mastery-skill-tree.md` | UPDATE | REQ-5 annotated "⚠️ partially implemented, issue #380", mirroring REQ-1's style |
| `app-changelog/issue-380.md` | CREATE | This file |

No `.Build.cs` change needed — no new dependencies.

## Acceptance criteria

- [x] `HandleMasteryResetConfirmClicked()` calls `RefundAllAndClearUnlocks()` then
      `ResetAccumulatedTotal()`, in that order, pinned by `LastMasteryRespecCallOrder`
      — proven by `KrowdKontrolMainMenuMasteryResetTest.cpp` block (e2)
- [x] After CONFIRM: `GetSpentPoints() == 0`, `GetUnlockedBubbles().Num() == 0`,
      `GetAccumulatedTotal() == 0` (unchanged #329 semantics) — block (e2)
- [x] Main-menu mastery display refreshes immediately (existing behavior, unaffected)
      — block (e), unmodified
- [x] An already-open MASTERY tree screen's points display refreshes immediately, no
      manual navigation — block (e2)
- [x] No crash when CONFIRM fires with the tree screen never opened — block (e3)
- [x] PR diff does not include any of issue #374's
      `PopulateTreeContent`/`TreeCanvas`/`RefreshBubbleStates`/`HandleBubbleClicked`/
      `MasterySkillBubbleWidget` content — verified by diffing every changed hunk
      against `origin/main` and #374's worktree state before committing
- [x] PR body (this changelog) explicitly discloses the `app/` vs.
      `app-source-tracked` divergence for `MasteryScreenWidget.h`/`.cpp` and why
- [x] `docs/prd-mastery-skill-tree.md` REQ-5 annotated as partially implemented, P0
      subset done, P1 (modifier slots) deferred to #376
- [x] Level 2 and Level 3 validation commands pass

## Scope limits (not built here)

- **Modifier-slot clearing** (PRD REQ-5's other half) — #376 (modifier catalog /
  2-slot data model) is still OPEN, not merged, as of 2026-08-31 (verify: `gh issue
  view 376 --json state`); there is no slot state to clear yet. The issue's own
  Notes section directs this exact P0-subset scoping.
- **Tree/bubble visual refresh on respec** — depends on #374's tree-render content,
  which is not part of this codebase's committed (`app-source-tracked/`) baseline
  yet. `app/`'s live `RefreshAfterRespec()` already does this as a bonus once #374
  lands and both pieces coexist in the live workspace, but it is not this PR's
  claim or test responsibility.
- **Any edit to `MasteryScreenWidget.h`/`.cpp` or `MasterySkillBubbleWidget.*` in
  `app/`** — owned by #374's in-progress work; not touched here.
- **Persistence of spent points / unlocks across launches** — REQ-1 already scoped
  this as session-only; unaffected by this change.

## Post-review fixes

Automated review of this PR (code-review, comment-quality, test-coverage agents)
independently traced the same CRITICAL bug: `UMasteryScreenWidget::RefreshPointsDisplayText()`
still computed `UnspentPoints` as raw `GetAccumulatedTotal()`, never subtracting
`GetSpentPoints()` — a formula left over from issue #373, before #371 shipped spend
tracking. This PR's own block (e2) sanity assertion (`"UNSPENT POINTS: 4"` after a
deposit of 5 and a spend of 1) was the first test anywhere to combine a real spend
with this display, and it asserted the *intended* value, not the one the unfixed
formula would produce.

Fixed in `app-source-tracked/`:
- `MasteryScreenWidget.cpp`'s `RefreshPointsDisplayText()` now computes
  `GetAccumulatedTotal() - GetSpentPoints()` in both the cached and freshly-resolved
  branches — matching the "available balance" formula `TrySpendOnBubble()` already
  documents (`CrowdMasteryTotalSubsystem.h`).
- Updated the two stale "there is no spend/refund tracking yet" comments
  (`MasteryScreenWidget.h`'s class doc, `MasteryScreenWidget.cpp`'s function comment)
  that dated from before #371 shipped.
- Reframed `MainMenuWidget.cpp`'s refund-before-reset ordering comment from an
  active-hazard claim to a defensive/forward-looking one — both callees are plain
  synchronous, disjoint-field mutations, so nothing today can actually read a
  mid-respec state.
- Corrected `MainMenuWidget.cpp`'s `if (MasteryScreenWidgetInstance)` refresh-guard
  comment from "if currently open" to "if it has been opened at least once this
  session" (the pointer is never nulled by the BACK handler, only
  `RemoveFromParent()`ed).
- Added a one-line comment on `LastMasteryRespecCallOrder.Reset()` clarifying it
  runs before subsystem resolution so a failed resolve leaves the array empty.
- Added two direct unit-test blocks ((f2)/(f3)) to `KrowdKontrolMasteryScreenWidgetTest.cpp`
  covering `RefreshAfterRespec()` in isolation (unbuilt-widget degrade-gracefully,
  and a built widget re-reading a real subsystem's balance) — previously only
  exercised indirectly through `KrowdKontrolMainMenuMasteryResetTest.cpp`.

Note: `app/`'s live copy of `MasteryScreenWidget.cpp`/`.h` already carries an
equivalent fix (attributed to #374, which independently corrected the same formula
while building its superset tree-render work) — `app/` was left untouched for these
files per the divergence policy above; only the `app-source-tracked/` splice and
`MainMenuWidget.cpp`/the widget's own test file (byte-identical between `app/` and
`app-source-tracked/`) were updated and mirrored to `app/`.

## Validation evidence

`python harness/ci.py --quick` → `GATE_OK mode=quick` (`UNIT_PASSED tests=136`,
`PIE_PASSED tests=8`) — re-run after the post-review fixes above, against the live
Editor build (not self-reported): all existing tests plus the new (f2)/(f3)
assertions pass with no regression vs. the 136/8 baseline PR #399 already
established.

Hard invariants (`MISSION.md`'s 8): reviewed. Invariant #8 (Unreal project not
git-tracked) satisfied by construction — `app/` stays the gitignored symlink; only
`MainMenuWidget.h`/`.cpp`, the test file, `MasteryScreenWidget.h`/`.cpp`'s spliced
addition, this changelog, and the PRD doc are tracked, per D-009.

## Manual PIE sign-off still required

Same standing limitation as every prior main-menu PR (#324, #329, #346, #373): no
ability-cast/click input primitive reaches real PIE input in this environment
(`holdout_no_ability_cast_input_primitive`). Automation tests call
`HandleMasteryResetClicked()`/`HandleMasteryResetConfirmClicked()` directly rather
than through a real Slate click. Manual PIE click-through is recommended before
merge, same as those PRs.

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` splice are the tracked-repo record of that
change, per D-009. Not a substitute for reading `app-source-tracked/` directly, and
note the deliberate `MasteryScreenWidget.h`/`.cpp` divergence documented above.
