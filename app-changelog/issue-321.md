# Issue #321: Data-driven NEXT LEVEL button on the post-run summary screen

Adds a `[NEXT LEVEL]` button to `UPostRunSummaryWidget` that advances the run through
the shipped level sequence by resolving the next map from `ULevelSequenceSubsystem`'s
`ComputeNextLevelMapName()` (issue #216 mechanism) — not a hardcoded if-chain. On the
sequence's final level (`ComputeNextLevelMapName() == NAME_None`) the button relabels
to `"FINISH RUN (More Levels Coming)"` and reruns the current level via the existing
defeat-restart reload path, `AKrowdKontrolPlayerController::RequestLevelRestart()`
(issue #223), instead of routing to a main menu (deferred to `docs/prd-main-menu.md`).

## Critical finding fixed as part of this issue

`ULevelSequenceSubsystem::HandleLevelClear()` previously called
`UGameplayStatics::OpenLevel()` itself, automatically, the instant `OnLevelClear`
fired. This was inert only because `LevelSequenceTable` is unset in production today
(`ComputeNextLevelMapName()` always returns `NAME_None`). The instant a real table gets
populated, that auto-advance would race the post-run summary screen — tearing down the
world before the player could read it or press anything, making the new button
decorative or racy.

**Fix**: extracted the `OpenLevel()` call out of `HandleLevelClear()` into a new public
`ULevelSequenceSubsystem::AdvanceToNextLevel()`. `HandleLevelClear()` now only resolves
`FinalMapName` bookkeeping (so `ULevelLifecycleSubsystem::RefreshLevelClearState()`'s
`OnRunComplete` check still fires correctly) and warns once on a missing row — it no
longer travels the map itself. `AdvanceToNextLevel()` is now only ever called from
`UPostRunSummaryWidget::HandleNextLevelClicked()`, i.e. only on a real player click.

Verified safe against the existing `KrowdKontrolLevelSequenceSubsystemTest.cpp` suite:
every existing case only asserted `ComputeNextLevelMapName()`/`FinalMapName`, never the
real `OpenLevel()` call, since `CreateNewMap()` test Worlds are never game worlds
(`IsGameWorld()` is false) — the removed branch was already dead code from those
tests' perspective. No existing assertion changed.

## Also changed

- `AKrowdKontrolPlayerController::RequestLevelRestart()` moved from `private` to
  `public` so `UPostRunSummaryWidget::HandleNextLevelClicked()` can call it directly —
  the one shared reload path both this button's final-level case and the existing
  defeat-restart flow (issue #223) now use, and issue #320's rerun button can reuse
  once it lands.
- `AEnemyBase.h` gained a `friend class FKrowdKontrolPostRunSummaryNextLevelButtonTest;`
  declaration (not called out in the original plan) — needed so the new test's
  Idle→Alert→Attack→Controlled→Banked sequence can drive a real `OnLevelClear`, the
  same private-`TickCheckDetection()` access grant every sibling test in this family
  already has.

## Post-review fixes

Applied after the review pass on this PR (all still `GATE_OK`):

- **Concurrent-task leakage removed from the tracked mirror.** The reviewed diff of
  `app-source-tracked/Source/KrowdKontrol/EnemyBase.h` carried a second, unrelated
  `friend class FKrowdKontrolTeachingPromptComponentTest;` grant for issue #219 (a
  different, in-flight task) — leaked in through the shared `app/` symlink, exactly
  the failure mode `[[feedback_check_diff_for_concurrent_task_leakage]]` describes.
  Stripped from `app-source-tracked/EnemyBase.h` (this PR's tracked deliverable). Left
  in place in the live `app/EnemyBase.h`, because issue #219's own in-progress work
  in that shared, untracked project already has a `KrowdKontrolTeachingPromptComponentTest.cpp`
  depending on it — confirmed by a real `C2248` compile failure when it was removed
  from `app/` too. Issue #219's own PR will add this grant to the tracked mirror when
  it lands.
- `UPostRunSummaryWidget::HandleNextLevelClicked()`'s two silent-failure branches (bad
  owning-player cast on the final-level path; missing `ULevelSequenceSubsystem` on the
  advance path) and `HandleLevelClear()`'s missing-subsystem fallback now log a
  one-shot `UE_LOG(LogTemp, Warning, ...)`, matching this file's own established
  `bHasWarnedMissing...` idiom (`ResolveLevelClearTimeSubsystem()`) instead of failing
  silently on a player-triggered action.
- `ULevelSequenceSubsystem` gained a public `LastAdvanceAttemptedMapName` seam, set
  unconditionally at the top of `AdvanceToNextLevel()` before its `IsGameWorld()`
  guard. Closes two coverage gaps flagged by review: (1)
  `KrowdKontrolPostRunSummaryNextLevelButtonTest.cpp` case (a) now asserts the click
  handler actually routed to `AdvanceToNextLevel()` with the right map, not just "didn't
  crash"; (2) `KrowdKontrolLevelSequenceSubsystemTest.cpp` case (a) now asserts
  `HandleLevelClear()` itself never sets this seam — regression protection for this
  issue's own "critical finding" (auto-advance no longer runs from `OnLevelClear`),
  which previously had none.
- `docs/prd-post-run-progression.md` REQ-3 and `docs/prd-teaching-arc.md`'s
  `ULevelSequenceSubsystem` description updated to match the shipped behavior (see
  review's docs-impact findings).

## Scope boundaries — explicitly not done here

- **No real `LevelSequenceTable` Content DataTable asset.** Issue #216 already scoped
  this out as a follow-up, and this codebase has no precedent yet for wiring a real
  Content DataTable as an `EditDefaultsOnly` `UWorldSubsystem` default (same
  unresolved gap `ULevelBriefingSubsystem::LevelBriefingTable` has). Net effect: a
  real PIE playthrough of `L_Level01`/`L_Level02`/`L_Level03` today still shows
  `"FINISH RUN (More Levels Coming)"` on every level, since the table stays unset.
  The button mechanism itself is real and fully tested against a synthetic in-test
  table (`BuildSequenceTable()`, mirroring `KrowdKontrolLevelSequenceSubsystemTest.cpp`'s
  own helper). Recommend a follow-up issue for real table content + a
  subsystem-default-wiring mechanism (affects `LevelSequenceTable` and
  `LevelBriefingTable` identically).
- **No rerun button** (`[RERUN LEVEL]`, issue #320) and **no centred info-block layout**
  (issue #319) — both still open, unmerged at the time of this PR. `NextLevelButton` is
  appended as the next child of the existing root `UVerticalBox`, directly below the
  info block, consistent with the AC's fallback positioning requirement. Whichever of
  #319/#320 lands next reconciles final joint layout.
- **No bespoke keyboard-handling code.** `UButton::OnClicked` fires identically for
  mouse click and native Slate keyboard activation (Enter/Space while focused) — no new
  input code invented, mirroring `UMainMenuWidget::QuitButton`'s precedent and issue
  #324's explicit decision for the same button shape. Real click-through/keyboard-focus
  verification in a live PIE session is flagged for manual sign-off (this environment
  cannot drive real Slate input).
- **No main-menu routing on the final level** — still explicitly deferred to
  `docs/prd-main-menu.md`'s routing work, per this issue's own AC.

## Acceptance criteria

- [x] `[NEXT LEVEL]` button added to `UPostRunSummaryWidget`, below the info block
- [x] Label/behavior resolved from `ULevelSequenceSubsystem::ComputeNextLevelMapName()`,
      not hardcoded conditionals
- [x] Non-final level: clicking resolves and (in a real game world) loads the correct
      next map via `AdvanceToNextLevel()` — `KrowdKontrol.Unit.PostRunSummaryNextLevelButton`
      case (a)
- [x] Final shipped level: button reads `"FINISH RUN (More Levels Coming)"` and reruns
      the current level via `RequestLevelRestart()` — case (b)
- [x] Button responds to mouse click and native Slate keyboard activation via
      `UButton::OnClicked` (no bespoke input code) — real PIE click-through/keyboard
      verification flagged for manual sign-off (this environment cannot drive real
      Slate input)
- [x] Automation tests confirm both (a) and (b)
- [x] `ULevelSequenceSubsystem`'s auto-advance-on-clear hazard fixed (moved to
      caller-triggered `AdvanceToNextLevel()`) — see "Critical finding" above
- [x] Scope boundaries stated explicitly above, not silently skipped
- [x] `python harness/ci.py --mode=full` reports `GATE_OK`
- [x] `app-source-tracked/` mirror + this changelog written

## Validation evidence

Full gate (`python harness/ci.py --mode=full`), re-run after the post-review fixes above:

```
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=119
PIE_PASSED tests=5
APP_STARTED driver=cli
UE_BUILD_START KrowdKontrolEditor Win64 Development
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
HOLDOUT_ABSENT no .factory/holdout/run.py
MUTATIONS_ABSENT no harness/mutations/run.py
GATE_OK mode=full
```

New/extended coverage: `KrowdKontrol.Unit.PostRunSummaryNextLevelButton` (new, 2
cases), `KrowdKontrol.Unit.PostRunSummaryWidget` (extended: button exists, `OnClicked`
bound, default "NEXT LEVEL" label), `KrowdKontrol.Unit.ReservedGameplayColours`
(extended: `NextLevelButtonLabel` colour audit against MISSION.md Hard Invariant 3),
`KrowdKontrol.Unit.LevelSequenceSubsystem` (comment-only update describing the split,
no assertion changes — verified safe above).

MISSION.md Hard Invariants reviewed against this diff: no kill-rule, colour-lock
(`NextLevelButtonLabel` audited above, uses the existing shared `HUDChromeColours`
text colour, not a new one), ability-roster, enemy-roster, engine/dimensionality,
networking, or `app`-tracking invariant is touched.

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
