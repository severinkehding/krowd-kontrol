# Issue #372: Persist skill-tree spent points and unlocked bubbles in the mastery save file

Extends the Crowd Mastery save-file mechanism (`ULevelClearTimeSaveGame`, shared
slot, already carrying `AccumulatedCrowdMasteryTotal` since #330) to also persist
the skill-tree spend state issue #371 (PR #391) introduced session-only:
`UCrowdMasteryTotalSubsystem::SpentPoints` and `UnlockedBubbleIds`. Before this
issue, a player who spent points and unlocked bubbles then closed the game lost
that progress on relaunch even though their earned total survived.

Two new fields on `ULevelClearTimeSaveGame`, following the exact naming/doc pattern
`AccumulatedCrowdMasteryTotal` already uses:

- `SpentCrowdMasteryPoints` (`int32`, default 0)
- `UnlockedCrowdMasteryBubbleIds` (`TArray<FName>`, default empty — `TArray` not
  `TSet` to match `GetUnlockedBubbles()`'s existing return type; the subsystem
  converts to/from its internal `TSet<FName>` at load/persist time)

`UCrowdMasteryTotalSubsystem::LoadPersistedTotal()`/`PersistAccumulatedTotal()` (both
existing method names kept unchanged — see Architect rationale below) are broadened
to read/write all three fields together in one load-then-mutate-then-save cycle
instead of just `AccumulatedTotal`. `TrySpendOnBubble()` (on success) and
`RefundAllAndClearUnlocks()` — previously in-memory-only mutators — now each call
`PersistAccumulatedTotal()` at the end, same call-placement pattern
`DepositRunMastery()`/`ResetAccumulatedTotal()` already establish.

No new save file, no new save slot, no UI work — same pattern #330 already
established for `AccumulatedCrowdMasteryTotal`, just extended to two more fields.

## Acceptance criteria

- [x] `ULevelClearTimeSaveGame` gains fields for spent points and unlocked bubble
      IDs (`SpentCrowdMasteryPoints`, `UnlockedCrowdMasteryBubbleIds`)
- [x] Save/load paths write/read these new fields alongside
      `AccumulatedCrowdMasteryTotal`, same save slot, no new save file
- [x] A unit test spends points, unlocks bubbles, saves, reloads into a fresh
      subsystem instance, and asserts spent points and unlocked bubbles match
      exactly
- [x] Existing save files without these fields load without error, defaulting to
      0/empty (backward-compatibility test added — a legacy save with only
      `AccumulatedCrowdMasteryTotal` set loads `SpentPoints`==0 and
      `GetUnlockedBubbles()` empty)

## Not building (scope limits, per the issue's own text)

- Modifier-slot persistence (`OwnedModifierIds`, `SlottedModifiersByBubbleId`) —
  issue #376's own separate, still-open scope.
- Any UI — the skill-tree screen (REQ-2) is unimplemented; nothing in this issue is
  visible to a player.
- A new save file or save slot — explicitly ruled out by the issue's own acceptance
  criteria.
- Renaming `LoadPersistedTotal()`/`PersistAccumulatedTotal()` — both names remain
  accurate enough after broadening; a rename would touch every call site in this
  class for a purely cosmetic reason the issue doesn't ask for.
- Changing `ResetAccumulatedTotal()`'s semantics (whether resetting the earned
  total should also clear spend/unlocks) — pre-existing behavior, out of scope for
  a persistence-only issue.

## Concurrent-PR note

`app/`'s copy of `CrowdMasteryTotalSubsystem.h`/`.cpp` also contains a
`GetUnlockedEffectHookIds()` method belonging to still-open PR #396 (issue #375),
sharing the same `app/` symlink. This PR's diff was hand-spliced against a clean
`origin/main` baseline to exclude that unrelated method entirely — confirmed via
`grep -c GetUnlockedEffectHookIds` returning 0 across every file this PR touches.

## Validation evidence

`harness/run_ue_automation.sh KrowdKontrol.Unit.CrowdMasteryTotalSubsystem`:

```
UE_BUILD_START KrowdKontrolEditor Win64 Development
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=2 total=2
UE_AUTOMATION_OK
```

`harness/run_ue_automation.sh KrowdKontrol.Unit.CrowdMasteryTotalSubsystemSpend`:

```
UE_BUILD_START KrowdKontrolEditor Win64 Development
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
```

`harness/run_ue_automation.sh KrowdKontrol.Unit.CrowdMasteryModifierSlot` (regression
check — this test already calls `TrySpendOnBubble`/`RefundAllAndClearUnlocks` and
deletes the save slot before/after, so the new write-through is inert for its
purposes):

```
UE_BUILD_START KrowdKontrolEditor Win64 Development
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
```

All three pass, 0 regressions.

MISSION.md Hard Invariants reviewed against this diff: no kill-rule, colour-lock,
ability-roster, enemy-roster, engine/dimensionality, networking, or `app`-tracking
invariant is touched — this is subsystem persistence logic and unit tests only, no
UI, no gameplay-facing behavior change.

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
