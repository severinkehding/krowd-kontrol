# Issue #380: Full-Respec Integration for RESET/CONFIRM

Wires the already-shipped `UCrowdMasteryTotalSubsystem::RefundAllAndClearUnlocks()`
(issue #371) into `UMainMenuWidget`'s existing RESET → CONFIRM RESET flow (issue
#329), so confirming a reset is now an honest full respec — refund every spent
point and clear every bubble unlock — instead of only zeroing the earned total.

`UMainMenuWidget::HandleMasteryResetConfirmClicked()` now calls
`RefundAllAndClearUnlocks()` before `ResetAccumulatedTotal()` (pinned order, so an
`AccumulatedTotal - SpentPoints` read mid-respec can never transiently go
negative), records the literal call order in a new test-observability seam
(`LastMasteryRespecCallOrder`, mirroring the existing `LastSelectedLevelMapName`
precedent), and — if a `MasteryScreenWidget` is already open —
refreshes it immediately via a new public `UMasteryScreenWidget::RefreshAfterRespec()`
method (the same `RefreshBubbleStates()` + `RefreshPointsDisplayText()` pair
`HandleBubbleClicked()` already runs after a successful spend), so the player never
sees a stale unlocked bubble or a stale points count without backing out and
re-entering the tree screen.

## Scope decision: modifier-slot clearing deferred

Full PRD scope (`docs/prd-mastery-skill-tree.md` REQ-5) also requires clearing
slotted modifiers. That requires the modifier catalog / 2-slot-per-skill data model
from issue **#376**, which is still open and unimplemented as of this PR (confirmed
by `grep -rln "Modifier" app/Source/KrowdKontrol/*.h app/Source/KrowdKontrol/*.cpp`
— only the unrelated `CameraModifier_OvercrowdDistortion`/
`OvercrowdVisualEffectSubsystem` camera-effects system exists — and by
`gh issue view 376`, reopened with no PR landed). Issue #380's own Notes section
anticipates exactly this and directs scoping to the P0 subset (refund points +
clear unlocks) in that case. **This PR implements the P0 subset only** — there is no
modifier-slot state yet to clear, so nothing is silently skipped.

## Acceptance criteria

- [x] `HandleMasteryResetConfirmClicked()` calls `RefundAllAndClearUnlocks()` before
      `ResetAccumulatedTotal()` — order pinned by `LastMasteryRespecCallOrder`
- [x] After CONFIRM: `SpentPoints == 0`, `UnlockedBubbles.Num() == 0`, and
      `AccumulatedTotal` behavior is unchanged from #329 (unconditional zero)
- [x] `MasteryScreenWidgetInstance` (if it exists) refreshes both its points display
      and bubble visual states immediately on CONFIRM, with no manual
      navigation/reload
- [x] Main-menu `MasteryDisplayText` refreshes immediately on CONFIRM (already true
      pre-#380 — verified still true, no regression)
- [x] Modifier-slot clearing explicitly deferred, citing #376 as not yet landed
- [x] `app/` and `app-source-tracked/` copies of all five changed files are
      byte-identical
- [x] `python harness/ci.py` reports `GATE_OK` with no regressions vs. baseline
- [x] `app-changelog/issue-380.md` created following the established format

## Not building (scope limits)

- **Modifier-slot clearing** — #376 (modifier catalog) has not landed. Deferred to
  land alongside #376/#377.
- **Changing `ResetAccumulatedTotal()`'s own zero-vs-not-zero semantics** — issue
  #329's unconditional-zero behavior is unchanged; neither `ResetAccumulatedTotal()`
  nor `RefundAllAndClearUnlocks()` is modified by this PR, both already
  independently unit-tested (#329, #371).
- **Persistence of the respec across app restarts** — `RefundAllAndClearUnlocks()`
  is documented session-only; persistence round-trip is a separate follow-up issue
  per `app-changelog/issue-371.md`.
- **A confirm-order-enforcing virtual/mock subsystem hierarchy** — order is pinned
  via the plain test-observability `TArray<FString>` seam
  (`LastMasteryRespecCallOrder`), matching this codebase's established
  `LastSelectedLevelMapName`/`bMasteryResetConfirmPending` convention rather than
  introducing a new subsystem-subclass test-double pattern.

## Validation evidence

`harness/run_ue_automation.sh KrowdKontrol.Unit.MainMenuMasteryReset`:

```
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
```

`harness/run_ue_automation.sh KrowdKontrol.Unit.` (full unit suite, 0 regressions):

```
UE_AUTOMATION_RESULT passed=136 total=136
UE_AUTOMATION_OK
```

`python harness/ci.py` (full gate):

```
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=136
PIE_PASSED tests=8
APP_STARTED driver=cli
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
GATE_OK mode=full
```

MISSION.md Hard Invariants reviewed against this diff: no kill-rule, colour-lock,
ability-roster, enemy-roster, engine/dimensionality, networking, or `app`-tracking
invariant is touched — this is a widget call-site integration plus a UI-refresh
seam, no new subsystem behavior.

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
