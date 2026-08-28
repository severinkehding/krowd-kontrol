# Issue #326: Route final-level post-run summary to the main menu

Replaces `UPostRunSummaryWidget::HandleNextLevelClicked()`'s final-level branch
(`ResolvedNextLevelMapName == NAME_None`) — previously a deliberate, documented
placeholder (issue #321/#342, `docs/prd-post-run-progression.md` REQ-3) that reran the
current level via `AKrowdKontrolPlayerController::RequestLevelRestart()` because the
main menu map didn't exist yet. It now does (issue #323/#324, merged PR #334):
`/Game/Maps/L_MainMenu`, already wired as the project's `GameDefaultMap`. The
final-level branch now resolves `UGameMapsSettings::GetGameDefaultMap()` (the same
authority `KrowdKontrolMainMenuMapTest.cpp` verifies equals `/Game/Maps/L_MainMenu`,
not a re-hardcoded literal) and, guarded by `World->IsGameWorld()` (the same guard
`AdvanceToNextLevel()`/`HandleLevelSelected()`/`RequestLevelRestart()` all already use),
calls `UGameplayStatics::OpenLevel()` to it.

**Fix**: `HandleNextLevelClicked()`'s final-level branch no longer casts
`GetOwningPlayer()` to `AKrowdKontrolPlayerController` or calls
`RequestLevelRestart()`. It records the resolved map into a new
`LastMainMenuLoadAttemptedMapName` member (test-observability seam, mirroring
`ULevelSequenceSubsystem::LastAdvanceAttemptedMapName`'s identical "real travel is
unreachable in Automation Editor Worlds" precedent) before the `IsGameWorld()` guard,
then calls `UGameplayStatics::OpenLevel(this, MainMenuMapName)` inside it. The
now-dead `bHasWarnedMissingOwningController` warn-once guard (only used by the removed
cast) is deleted; `bHasWarnedMissingOwningControllerOnRerun`
(`HandleRerunClicked()`) and `bHasWarnedMissingOwningControllerForFocus`
(`HandleLevelClear()`) are untouched.

Also updates the final-level button label from `"FINISH RUN (More Levels Coming)"` to
`"FINISH RUN"` (matches `docs/prd-post-run-progression.md` REQ-3's own suggested
label, now that the button actually goes somewhere) — the `"FinishRun"` NSLOCTEXT key
is unchanged, only the displayed string.

The non-final-level branch (`AdvanceToNextLevel()`) and the `RERUN LEVEL` button
(`HandleRerunClicked()`) are untouched, per the issue's explicit AC.

## Scope boundaries — explicitly not done here

- **No change to `ULevelSequenceSubsystem` or `LevelSequenceData.h`.** The "is this
  the final level" resolution (`ComputeNextLevelMapName() == NAME_None`) was already
  correct; this issue only changes what happens *after* that resolution.
- **No lock/unlock gating, no confirmation dialog before loading the main menu** — out
  of scope per `docs/prd-main-menu.md`'s own "Out of scope" section and this issue's
  AC, matching `HandleLevelSelected()`'s existing "no confirmation step" precedent.
- **No populated `DT_LevelSequenceTable` content asset authoring** — a separately
  tracked known gap (`docs/prd-post-run-progression.md` REQ-3's own note), unaffected
  by this change; the Automation test continues to inject its own synthetic table.

## Acceptance criteria

- [x] After clearing the final shipped level, the post-run summary's final-level
      control loads `/Game/Maps/L_MainMenu` (via `UGameplayStatics::OpenLevel`,
      `IsGameWorld()`-guarded) instead of rerunning the level
- [x] `RERUN LEVEL` button and the `NEXT LEVEL` button's non-final-level behavior
      (`AdvanceToNextLevel()`) are unchanged
- [x] `KrowdKontrol.Unit.PostRunSummaryNextLevelButton` automation test covers and
      passes both the non-final-level and the new final-level-routes-to-main-menu
      cases
- [x] Code mirrors existing patterns exactly (`IsGameWorld()` guard,
      `LastXAttemptedMapName` test-observability seam, `UGameMapsSettings` resolution)
- [x] No regressions in existing tests
- [x] `app-source-tracked/` mirror and this changelog both written
- [x] `python harness/ci.py --quick` reports `GATE_OK`

## Validation evidence

`python harness/ci.py --quick`:

```
HARNESS_START mode=quick driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=124
PIE_PASSED tests=5
GATE_OK mode=quick
```

Full validation (build + `KrowdKontrol.Unit.PostRunSummaryNextLevelButton` compile/run,
E2E, holdout/mutations) deferred to the `dark-factory-validate` node per this repo's
workflow split.

MISSION.md Hard Invariants reviewed against this diff: no kill-rule, colour-lock,
ability-roster, enemy-roster, engine/dimensionality, networking, or `app`-tracking
invariant is touched.

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
