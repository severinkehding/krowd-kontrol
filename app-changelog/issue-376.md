# Issue #376: Crowd Mastery Skill Tree — Modifier Catalog + 2-Slot-Per-Skill Data Model

Adds the data-side half of `docs/prd-mastery-skill-tree.md` REQ-4: a new
`FMasteryModifierRow` DataTable family (`ModifierData.h` — `EModifierCategory`,
`EModifierTier`, `FMasteryModifierRow : public FTableRowBase`) mirroring the shipped
`MasteryTreeData.h` pattern exactly, plus an extension of
`UCrowdMasteryTotalSubsystem` (issue #371's spend/unlock authority) to track, per
unlocked skill bubble, up to 2 slotted modifier references
(`SlottedModifiersByBubbleId`) and a separate owned-modifiers inventory
(`OwnedModifierIds`).

Five new public entry points, following `TrySpendOnBubble`'s established
`bool TryX(...)` fail-closed shape:

- `GrantModifier(ModifierId)` — adds to the owned inventory; fails if unknown in
  `ModifierCatalogTable` or already owned.
- `GetOwnedModifiers()` — every currently-owned modifier ID.
- `TrySlotModifier(BubbleId, ModifierId)` — fails closed (no mutation) if the
  bubble isn't unlocked, the modifier is unknown or not owned, both slots are
  full, or the bubble already has a slotted modifier of the same `Category`.
- `UnslotModifier(BubbleId, ModifierId)` — removes a present entry; no-ops on an
  absent one.
- `GetSlottedModifiers(BubbleId)` — the bubble's currently slotted modifier IDs.

`RefundAllAndClearUnlocks()` is extended to also clear `SlottedModifiersByBubbleId`
(not `OwnedModifierIds`), per PRD REQ-5's "clear all unlocks and slotted
modifiers" — earned modifiers persist through respec, same as the earned mastery
total.

A populated placeholder content asset, `/Game/Data/DT_ModifierCatalogTable`
(`app/Content/Data/DT_ModifierCatalogTable.uasset`), was authored headlessly via
`UnrealEditor-Cmd.exe -run=pythonscript` (`unreal.DataTableFactory` +
`fill_data_table_from_json_string` + `save_loaded_asset`), same mechanism issue
#370 used for `DT_MasteryTreeTable`. 5 rows spanning all 4 categories
(`AttackType`/`SurvivalType`/`AbilityType`/`ItemType`) and both Tier I and Tier II,
independently verified via a second headless `export_to_json_string()` call before
the Automation test asserting against the real asset was even written.

## Design decision: category rule is anti-duplication, not fixed-per-slot (superseded)

The issue's literal acceptance-criteria wording ("the modifier's category doesn't
match what the slot accepts") reads as if each of a bubble's 2 slots has its own
pre-assigned accepted category. That reading was rejected: the shipped data model
has no per-slot-category field anywhere to check against, and the PRD's own cited
naming-shape reference documents the real rule as "a node can't have upgrades of
the same type" — anti-duplication across a bubble's own slots. `TrySlotModifier`
implements the anti-duplication reading and documents this decision directly in its
doc comment. This is the one genuine design judgment call in this change; flagged
here for reviewer visibility per this workflow's own prior stalled-run guidance.

**Superseded by pass-1 validation feedback (PR #401).** The behavioral validator
held the issue's literal wording as decisive: a slot has a pre-assigned accepted
category, not an anti-duplication rule across already-slotted modifiers.
`FMasterySkillBubble` now carries `SlotAcceptedCategories` (indexed by slot
position), and `TrySlotModifier` matches a candidate modifier's `Category` against
an open slot's accepted category instead of scanning already-slotted modifiers for
a `Category` collision. `KrowdKontrolCrowdMasteryModifierSlotTest.cpp` scenario 4
was rewritten accordingly (two same-category slots both filling proves this is no
longer an anti-duplication rule; a mismatched-category rejection with an open slot
still available proves it's a fixed per-slot check, not "first empty slot wins").

`EModifierCategory` includes a fourth category, `ItemType`, beyond the PRD's own
three named examples (Survival/Attack/Ability) — the PRD's own cited reference
source documents four, and including it now avoids a breaking enum change later.

## Not building (scope limits, per the issue's own acceptance criteria)

- **Tier-gating enforcement** — `Tier` is stored and exposed but never
  gate-enforced; deferred to a separate, later tier-gating issue.
- **Modifier acquisition wiring** — `GrantModifier` is not called from anywhere in
  this change; the real level-clear earn trigger is a separate, later issue.
- **Slotting UI** — no `MasteryScreenWidget`/`MasterySkillBubbleWidget` changes;
  deferred to a separate UI issue, matching how #370/#371 stayed data/API-only.
- **`GameplayTags`-based category** — rejected in favor of a plain `UENUM`, matching
  the established `MasteryTreeData.h` convention; no new plugin dependency.
- **Persistence** — `OwnedModifierIds`/`SlottedModifiersByBubbleId` are
  session-only, same as `SpentPoints`/`UnlockedBubbleIds` still are (issue #371
  explicitly deferred that persistence round-trip).

## Acceptance criteria

- [x] `FMasteryModifierRow`/`EModifierCategory`/`EModifierTier` exist in
      `ModifierData.h`, following `MasteryTreeData.h`'s established pattern
- [x] `/Game/Data/DT_ModifierCatalogTable.uasset` is authored with 5 rows across 4
      categories and both Tier I and Tier II represented
- [x] `UCrowdMasteryTotalSubsystem` tracks, per unlocked bubble, up to 2 slotted
      modifier references (`SlottedModifiersByBubbleId`) and a separate owned
      inventory (`OwnedModifierIds`)
- [x] `TrySlotModifier`/`UnslotModifier` API exists, rejects on not-unlocked,
      both-slots-full, and category-mismatch against a slot's pre-assigned accepted
      category (see "superseded" note above)
- [x] `Tier` value is stored and exposed but not gate-enforced
- [x] Unit tests cover all 4 literally-named acceptance-criteria cases plus the
      respec interaction, unslot, and defensive unknown/not-owned rejections
- [x] `RefundAllAndClearUnlocks()` clears slotted modifiers, leaves owned inventory
      untouched
- [x] `app-source-tracked/` mirror contains only this issue's own hunks — spliced
      against `origin/main`, explicitly excluding PR #396's already-live
      `GetUnlockedEffectHookIds()` addition to the same file (confirmed present in
      the shared `app/` tree, unrelated to this issue, unmerged as of this change)
- [x] Level 2-3 validation commands pass with exit 0, 0 regressions

## Validation evidence

`harness/run_ue_automation.sh KrowdKontrol.Unit.ModifierData`:

```
UE_BUILD_START KrowdKontrolEditor Win64 Development
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
```

`harness/run_ue_automation.sh KrowdKontrol.Unit.CrowdMasteryModifierSlot`:

```
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
```

Full suite, `harness/run_ue_automation.sh KrowdKontrol.Unit.`:

```
UE_AUTOMATION_RESULT passed=138 total=138
UE_AUTOMATION_OK
```

All `KrowdKontrol.Unit.*` tests pass, 0 regressions (138 = prior baseline plus this
issue's 2 new tests, plus whatever else has landed on `main` concurrently).

`python harness/ci.py --quick`:

```
HARNESS_START mode=quick driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=138
PIE_PASSED tests=8
GATE_OK mode=quick
```

The real `DT_ModifierCatalogTable` asset's shape was independently confirmed via a
headless `-run=pythonscript` verification call before the Automation test was even
written: `export_to_json_string()` on the loaded table shows exactly 5 rows
(`Mod_SurvivalAbilityTierI`, `Mod_AttackAbilityTierI`, `Mod_AbilityTierI`,
`Mod_ItemTierI`, `Mod_SurvivalAbilityTierII`), correct `Category`/`Tier` per row,
and non-empty `DisplayName`/`EffectHookId`.

MISSION.md Hard Invariants reviewed against this diff: no kill-rule, colour-lock,
ability-roster, enemy-roster, engine/dimensionality, networking, or `app`-tracking
invariant is touched — this is new subsystem state, a new DataTable-row family, and
unit tests only; no UI, no gameplay-facing behavior change (nothing calls the new
API yet except the tests themselves).

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
