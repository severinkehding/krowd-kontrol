# Issue #376: Modifier Catalog + 2-Slot-Per-Skill Data Model

Adds the data-side half of `docs/prd-mastery-skill-tree.md` REQ-4 on top of the
skill-tree data model (issue #370) and spend/refund/prerequisite API (issue #371):
a new `FMasteryModifierRow` DataTable catalog and a 2-slot-per-unlocked-bubble
modifier system on `UCrowdMasteryTotalSubsystem`.

## What shipped

New file `ModifierData.h`: `EModifierCategory` (AttackType/SurvivalType/
AbilityType/ItemType), `EModifierTier` (TierI/TierII/TierIII), and
`FMasteryModifierRow : public FTableRowBase` (Category/Tier/DisplayName/
EffectHookId) — mirrors `MasteryTreeData.h`'s established "one DataTable row per
content item" shape exactly. `FMasterySkillBubble` gains a
`TArray<EModifierCategory> SlotAcceptedCategories` field (index 0 = slot 0's
accepted category, index 1 = slot 1's).

`UCrowdMasteryTotalSubsystem` gains a `ModifierCatalogTable` content-asset
reference (same lazy-load-in-`Initialize()` pattern as `MasteryTreeTable`), an
`OwnedModifierIds` inventory, a `SlottedModifiersByBubbleId` map, and five new
entry points:

- `GrantModifier(ModifierId)` — adds to the owned inventory; fails closed if
  unknown in the catalog or already owned. Not called from anywhere yet — the
  real level-clear earn trigger is a separate, later issue.
- `GetOwnedModifiers()` — every currently-owned modifier ID.
- `TrySlotModifier(BubbleId, ModifierId)` — fails closed (no state mutation)
  if `BubbleId` is not unlocked, `ModifierId` is unknown/not owned, or no open
  slot's pre-assigned accepted category matches the modifier's category. Same
  "no mutation until every guard passes" idiom as `TrySpendOnBubble`.
- `UnslotModifier(BubbleId, ModifierId)` — removes an entry if present.
- `GetSlottedModifiers(BubbleId)` — a bubble's 0-2 currently-slotted IDs.

`RefundAllAndClearUnlocks()` (REQ-5's respec) now also clears
`SlottedModifiersByBubbleId`, leaving `OwnedModifierIds` untouched — earned
modifiers survive a respec, matching how `AccumulatedTotal` already survives it.

`DT_ModifierCatalogTable.uasset` authored headlessly: 5 rows across all 4
categories, both Tier I and Tier II represented.

## Design decision: fixed-per-slot category, not anti-duplication

The issue's acceptance criteria text — *"the modifier's category doesn't match
what the slot accepts"* — describes a fixed, pre-assigned category per slot
position, not an anti-duplication rule across a bubble's already-slotted
modifiers. Both readings were evaluated on this issue. The fixed-per-slot
reading (`FMasterySkillBubble::SlotAcceptedCategories`, indexed by slot
position; a candidate must match some *open* slot's assigned category) is what
this issue's own acceptance-criteria wording decisively describes, and it's
what the unit tests (`KrowdKontrolCrowdMasteryModifierSlotTest.cpp`, scenario
4b) assert: a category mismatch is rejected even when an open slot exists,
proving it's not "first open slot wins" / anti-duplication. An anti-duplication
reading was considered and rejected as not matching the issue's literal text.

## Note on this PR's shape: mirror-isolation, not new implementation

This is a re-run of issue #376. A prior attempt (PR #401) implemented this exact
feature, was tested (138 unit + 8 PIE tests passing), and passed pass-1 review —
then was rejected at pass-2 because its `app-source-tracked/` mirror
accidentally included `GetUnlockedEffectHookIds()`, a method belonging to a
different, still-open, unmerged PR (#396 / issue #375) touching the same shared
`CrowdMasteryTotalSubsystem.{h,cpp}` files in the live `app/` tree. The feature
logic itself was never in question. This PR rebuilds the `app-source-tracked/`
mirror hunk-by-hunk against the current `main` baseline, explicitly excluding
every hunk attributable to #375's `GetUnlockedEffectHookIds()` (declaration,
implementation, and doc comment) — confirmed via
`grep -c GetUnlockedEffectHookIds` returning 0 on both tracked files.
`app/Source/KrowdKontrol/KrowdKontrolPlayerController.*` (issue #375/PR #396's
live, unmerged work) is not touched, read for patterns, or referenced by this
PR at all.

## Acceptance criteria

- [x] `FMasteryModifierRow`/`EModifierCategory`/`EModifierTier` exist in
      `ModifierData.h`, following `MasteryTreeData.h`'s established pattern
- [x] `/Game/Data/DT_ModifierCatalogTable.uasset` is authored with 5 rows across
      4 categories and both Tier I and Tier II represented
- [x] `UCrowdMasteryTotalSubsystem` tracks, per unlocked bubble, up to 2 slotted
      modifier references, plus a separate owned-modifiers inventory
- [x] `TrySlotModifier`/`UnslotModifier` API exists, rejects on not-unlocked,
      both-slots-full, and category-mismatch (fixed-per-slot-category via
      `SlotAcceptedCategories`)
- [x] `Tier` value is stored and exposed but not gate-enforced (separate,
      later issue's job)
- [x] Unit tests cover: successful slot, slot-when-not-unlocked rejection,
      slot-when-full rejection, category-mismatch rejection
- [x] `RefundAllAndClearUnlocks()` clears slotted modifiers, leaves owned
      inventory untouched
- [x] `app-source-tracked/` mirror contains only this issue's own hunks —
      `GetUnlockedEffectHookIds` does not appear anywhere in the diff
- [x] `KrowdKontrolPlayerController.*` are not touched by this PR's diff at all

## Not building (scope limits, per the issue and PRD)

- Tier-gating enforcement — `Tier` is stored and exposed but never
  gate-enforced. Separate, later tier-gating issue.
- Modifier acquisition wiring — `GrantModifier` is not called from anywhere in
  this change. The real level-clear earn trigger is a separate, later issue.
- Slotting UI — no `MasteryScreenWidget` changes. Separate UI issue, matching
  how #370/#371 stayed data/API-only.
- `GameplayTags`-based category — a plain `UENUM` matches the established
  `MasteryTreeData.h` convention; no new plugin dependency.
- Persistence — `OwnedModifierIds`/`SlottedModifiersByBubbleId` are
  session-only, same as `SpentPoints`/`UnlockedBubbleIds` (issue #371 already
  deferred that persistence round-trip).
- Anything in `KrowdKontrolPlayerController.*` — that's issue #375/PR #396's
  live, unmerged, concurrent work.
- Cross-bubble modifier-reuse policy — `TrySlotModifier` currently allows the
  same owned modifier into multiple bubbles at once (not consumed on slot).
  Documented as current behavior (see the slot test's scenario 3 comment), not
  silently fixed here — a real design decision for a follow-up issue.

## Validation evidence

`grep -c GetUnlockedEffectHookIds` on both tracked subsystem files: `0` for
both — confirms the mirror-isolation defect that sank PR #401 does not recur.

`python harness/ci.py --quick` (see Implementation Report for this run's
output).

---

The real Unreal project stays under `app/` (gitignored, D-003) — this
changelog and its matching `app-source-tracked/` copy are the tracked-repo
record of that change, per D-009. Not a substitute for reading
`app-source-tracked/` directly.
