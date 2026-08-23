# Issue #216: Advance to next level on level-clear per a configured level sequence

`ULevelLifecycleSubsystem::OnLevelClear` already broadcast correctly (merged, issue
#169), but nothing consumed it to advance the run — the only existing
level-transition paths were level-restart-on-failure
(`AKrowdKontrolPlayerController::RequestLevelRestart`) and a single hardcoded
`FinalMapName` comparison, explicitly documented as a "placeholder single-map
mechanism, not the full run sequence." Clearing any level simply stalled the run.

This adds a new `UWorldSubsystem`, `ULevelSequenceSubsystem`, that self-subscribes to
`ULevelLifecycleSubsystem::OnLevelClear` from its own `Initialize()` — mirroring
`ULevelBriefingSubsystem`'s established self-subscribe shape, just for `OnLevelClear`
instead of `OnLevelBegin`. Its designer-facing config is a new `UDataTable` of
`FLevelSequenceRow` (`LevelSequenceData.h`), keyed by bare current-level map name,
each row holding the next level's map name — the same "one `UDataTable` row per level
map name" convention `LevelBriefingData.h` already established, rather than
introducing this project's first `UDataAsset`.

On `OnLevelClear`:
- If the current map's row has a real `NextLevelMapName`, `ULevelSequenceSubsystem`
  calls `UGameplayStatics::OpenLevel`, guarded by `World->IsGameWorld()` (mirroring
  `AKrowdKontrolPlayerController::RequestLevelRestart()`'s identical guard — real map
  travel only makes sense in an actual game world, never in the
  `CreateNewMap()`-based Editor worlds `KrowdKontrol.Unit.*` tests run in, where a
  real `OpenLevel()` call hangs the in-process Automation run, issue #172).
- If the row's `NextLevelMapName` is `NAME_None` (an explicit end-of-sequence
  marker), it sets `ULevelLifecycleSubsystem::FinalMapName` to this world's own map
  name instead. Because this handler runs synchronously inside the same
  `OnLevelClear.Broadcast()` call `RefreshLevelClearState()` is still executing,
  by the time that function reaches its own `FinalMapName` comparison — the very next
  lines after `Broadcast()` returns — `FinalMapName` is already set, and the
  **existing** `OnRunComplete` path fires on its own. `ULevelSequenceSubsystem` never
  touches `OnRunComplete` directly.
- If the current map has no row at all (not part of the configured sequence, e.g. a
  prototype map), this is a safe, one-shot-warned no-op — `FinalMapName` is left
  untouched, so an unconfigured map is never mistaken for the sequence's end.

## Trigger choice: `OnLevelClear` directly, not a post-run-summary-widget dismissal event

Issue #216's own acceptance criteria are conditional: the advance should trigger off
a post-run-summary-widget dismissal event only if issue #175
(`UPostRunSummaryWidget` wiring) has landed and exposes that event by the time this
is built. Checked at implementation time (2026-08-24): issue #175 is still open
(reopened 2026-08-22, no PR yet), so per the issue's own explicit fallback, this PR
triggers the advance directly off `ULevelLifecycleSubsystem::OnLevelClear`. **This
should be revisited once #175's dismissal event exists**, per the issue's own note.

## Not built in this change

- The real `LevelSequenceTable` DataTable asset content (the actual
  L_Level01→02→03→04→05 rows) — `L_Level03/04/05.umap` don't exist on disk yet
  either. This mirrors the exact precedent already set by
  `ULevelLifecycleSubsystem::FinalMapName` and
  `ULevelBriefingSubsystem::LevelBriefingTable`, both currently unset,
  designer-authored-later config. Mechanism now, content later.
- Issue #234 (`OnLevelClear`'s real-live-PIE-fire gap) — separate, already addressed
  elsewhere; this PR's automation coverage uses the same direct-call convention every
  other `KrowdKontrol.Unit.*` lifecycle test already uses and is unaffected by #234
  either way.
- A `KrowdKontrol.PIE.*` scenario test for a real map-to-map load — the issue's own
  acceptance criteria only ask for `KrowdKontrol.Unit.*`-style coverage, and this
  codebase's precedent (`KrowdKontrolLevelRestartTest.cpp`, issue #172) also stops at
  asserting the computed seam rather than a real load; a real-map-load PIE test is
  recommended as a follow-up once `L_Level03/04/05.umap` and the real
  `LevelSequenceTable` asset exist. Note also: the `KrowdKontrol.PIE.*` gate rung is
  currently expected to be red regardless, pending issue #292 (unrelated, already
  tracked).
- Reconciling `UAbilityUnlockLevelSubsystem`'s interim `ParseLevelIndexFromMapName()`
  digit-parsing with this sequence config — that subsystem's own header already flags
  this as issue #217's independently-buildable follow-up.

## Files changed

- `Source/KrowdKontrol/LevelSequenceData.h` (new) — `FLevelSequenceRow` DataTable row
  struct.
- `Source/KrowdKontrol/LevelSequenceSubsystem.h`/`.cpp` (new) — `ULevelSequenceSubsystem`,
  the `OnLevelClear` consumer described above.
- `Source/KrowdKontrol/EnemyBase.h` — added
  `friend class FKrowdKontrolLevelSequenceSubsystemTest;` alongside the existing
  `FKrowdKontrolLevelLifecycleSubsystemTest` friend grant, so the new test can drive
  `AEnemyBaseTestActor` through `TickCheckDetection()` the same deterministic way
  every other lifecycle test in this module already does. Not called out in the
  original plan's "Files to Change" table — a real gap found while verifying the
  plan's cited test pattern actually compiles (`TickCheckDetection` is private,
  friend-gated per test class).
- `Source/KrowdKontrol/Private/Tests/KrowdKontrolLevelSequenceSubsystemTest.cpp`
  (new) — three cases: (a) non-final clear resolves the next map,
  `FinalMapName` untouched; (b) final clear (`NextLevelMapName == NAME_None`) sets
  `FinalMapName` and the existing `OnRunComplete` fires; (c) unconfigured map is a
  safe warn-once no-op.

## Validation

`harness/run_ue_automation.sh "KrowdKontrol.Unit.LevelSequenceSubsystem"` — expected
`passed=3 total=3`. Full `python harness/ci.py --quick` also run; see
`implementation.md`.

## Notes

One deviation from the plan: `EnemyBase.h` needed a new friend-class grant for the
new test (see "Files changed" above) — the plan's "Files to Change" table omitted it,
though its own "Mandatory Reading" pointed at the exact `TickCheckDetection` call
sequence that requires it. Everything else matches the plan exactly.

`app/` itself (the gitignored symlink to the real Unreal project, CLAUDE.md's
Environment section / `.factory/decisions.md` D-003) is unchanged in kind by this
tracked copy — the files above under `app-source-tracked/` are a plain-text mirror
made at PR-creation time (D-009) so GitHub has a non-empty diff to open a PR against
and reviewers have real source to check, not a description of it.
