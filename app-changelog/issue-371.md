# Issue #371: Crowd Mastery Skill Tree — Spend/Refund/Prerequisite API

Extends `UCrowdMasteryTotalSubsystem` (issue #327/#330's sole Crowd Mastery total
authority) with the ability to actually spend earned points against the skill-tree
data model issue #370 added (`MasteryTreeData.h` / `DT_MasteryTreeTable`).

Adds a spent-points counter (`SpentPoints`, kept strictly separate from the
untouched earned `AccumulatedTotal`), a set of unlocked bubble IDs
(`UnlockedBubbleIds`), and a public, test-injectable
`TObjectPtr<UDataTable> MasteryTreeTable` reference loaded lazily in `Initialize()` —
same pattern `ULevelSequenceSubsystem::LevelSequenceTable` already establishes. Four
new entry points:

- `TrySpendOnBubble(BubbleId)` — spends the bubble's `PointCost` against the
  available balance (`AccumulatedTotal - SpentPoints`); fails closed (no state
  mutation) if the bubble is unknown, already unlocked, its prerequisite isn't met,
  or the balance is insufficient.
- `IsPrerequisiteMet(BubbleId)` — true for root nodes (`ParentNodeId == NAME_None`)
  or once the owning node's parent has at least one unlocked bubble, per
  `MasteryTreeData.h`'s own documented rule.
- `GetUnlockedBubbles()` — every currently-unlocked bubble ID.
- `RefundAllAndClearUnlocks()` — full respec: zeroes `SpentPoints`, clears
  `UnlockedBubbleIds`, never touches `AccumulatedTotal`.

Plus `GetSpentPoints()`, a `BlueprintPure` getter mirroring
`GetAccumulatedTotal()`'s exact shape — not explicitly named in the acceptance
criteria, but necessary for the counter to be readable by tests or a future caller.

No UI or save-file work is in this issue — `MasteryScreenWidget`'s "UNSPENT POINTS"
placeholder still reads `GetAccumulatedTotal()` unchanged, and `SpentPoints`/
`UnlockedBubbleIds` are session-only (no persistence). Both are explicitly deferred
to later follow-up issues per #371's own Notes section.

## Acceptance criteria

- [x] `UCrowdMasteryTotalSubsystem` gains a spent-points counter separate from the
      earned total (`SpentPoints`, distinct from `AccumulatedTotal`)
- [x] `UCrowdMasteryTotalSubsystem` gains a set of unlocked bubble IDs
      (`UnlockedBubbleIds`, exposed read-only via `GetUnlockedBubbles()`)
- [x] `TrySpendOnBubble(BubbleId)` returns success/failure and mutates state only on
      success
- [x] `IsPrerequisiteMet(BubbleId)` correctly reflects "parent node reached" (root
      nodes trivially met; child nodes met once the parent has at least one
      unlocked bubble)
- [x] `GetUnlockedBubbles()` returns exactly the set of currently-unlocked IDs
- [x] `RefundAllAndClearUnlocks()` performs a full respec: zeroes `SpentPoints`,
      clears `UnlockedBubbleIds`, leaves `AccumulatedTotal` untouched, no partial
      refunds
- [x] `TrySpendOnBubble` rejects and does not mutate state when available points are
      insufficient
- [x] `TrySpendOnBubble` rejects and does not mutate state when the prerequisite is
      not met
- [x] Unit tests cover: successful spend, insufficient-points rejection,
      prerequisite rejection, full respec refund restoring the exact spent-point
      count with all unlocks cleared (plus two defensive cases beyond the literal
      criteria: already-unlocked rejection, unknown-`BubbleId` rejection)
- [x] No UI or save-file changes included
- [x] `run_ue_automation.sh` passes for both the existing and new test targets with
      0 regressions

## Not building (scope limits, per the issue's own text)

- Persistence of `SpentPoints`/`UnlockedBubbleIds` to `ULevelClearTimeSaveGame` —
  explicitly deferred to a separate follow-up issue.
- Wiring into `MasteryScreenWidget` or the RESET → CONFIRM menu flow — deferred to
  a separate "Full-respec integration" issue.
- Resolving `EffectHookId` into real gameplay effects — deferred to the separate P1
  modifier-catalog issue.
- A "total changed" broadcast delegate — no precedent for one on this subsystem;
  not introduced speculatively.

## Validation evidence

`harness/run_ue_automation.sh KrowdKontrol.Unit.CrowdMasteryTotalSubsystem`:

```
UE_BUILD_START KrowdKontrolEditor Win64 Development
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=2 total=2
UE_AUTOMATION_OK
```

(Matches both `KrowdKontrol.Unit.CrowdMasteryTotalSubsystem` and the new
`KrowdKontrol.Unit.CrowdMasteryTotalSubsystemSpend` by substring — both pass.)

`harness/run_ue_automation.sh KrowdKontrol.Unit.CrowdMasteryTotalSubsystemSpend`:

```
UE_BUILD_START KrowdKontrolEditor Win64 Development
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
```

Full suite, `harness/run_ue_automation.sh KrowdKontrol.Unit`:

```
UE_BUILD_START KrowdKontrolEditor Win64 Development
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=130 total=130
UE_AUTOMATION_OK
```

All `KrowdKontrol.Unit.*` tests pass, 0 regressions.

`python harness/ci.py --quick`:

```
HARNESS_START mode=quick driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=130
PIE_PASSED tests=8
GATE_OK mode=quick
```

MISSION.md Hard Invariants reviewed against this diff: no kill-rule, colour-lock,
ability-roster, enemy-roster, engine/dimensionality, networking, or `app`-tracking
invariant is touched — this is subsystem logic and unit tests only, no UI, no
gameplay-facing behavior change (nothing calls the new API yet).

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
