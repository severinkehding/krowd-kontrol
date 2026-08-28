# Issue #370: Crowd Mastery Skill Tree — Data Model

Adds the data schema for the P-Organ-style skill tree described in
`docs/prd-mastery-skill-tree.md` REQ-1 (node/bubble half only): `EMasteryTreePhase`
(`app/Source/KrowdKontrol/MasteryTreeData.h`), `FMasterySkillBubble` (one skill
bubble — `BubbleId`, `DisplayName`, `PointCost`, a deferred `EffectHookId`), and
`FMasteryTreeNode : public FTableRowBase` (one DataTable row per node — `ParentNodeId`
prerequisite, `Phase`, and its 4 `Bubbles`). Mirrors the existing
`FLevelSequenceRow`/`DT_LevelSequenceTable` "one `.h` file per DataTable family"
convention exactly, including the `NAME_None`-as-"no prerequisite" sentinel idiom.

Also authors a populated placeholder content asset,
`/Game/Data/DT_MasteryTreeTable` (`app/Content/Data/DT_MasteryTreeTable.uasset`): a
root node (`Node_Root`, `ParentNodeId = NAME_None`) and a child node
(`Node_PenZoneMastery`, `ParentNodeId = "Node_Root"`), each with 4 bubbles, using
effect-hook IDs drawn from the PRD's own REQ-3 starter-catalog examples (ability
cooldown reduction, Controlled-duration bonus, energy max increase, pen-zone radius
bonus, movement speed). Authored headlessly via `UnrealEditor-Cmd.exe
-run=pythonscript` (`unreal.DataTableFactory` + `fill_data_table_from_json_string` +
`save_loaded_asset`) — no live MCP connection needed or used.

This is purely additive data: no spend/refund logic, no subsystem, no UI reads this
table yet. That is explicitly out of scope here — a later issue extends
`UCrowdMasteryTotalSubsystem` to consume it, and a separate P1 modifier-catalog issue
resolves `EffectHookId` into real gameplay effects.

## Acceptance criteria

- [x] `FMasterySkillBubble`/`FMasteryTreeNode` C++ structs exist with: bubble ID,
      display name, point cost, parent node reference, phase/tier, effect-hook
      identifier.
- [x] A DataTable instance is authored in `app/Content/Data/DT_MasteryTreeTable.uasset`
      with 2 nodes, 4 bubbles each, and a real parent/child prerequisite link
      (`Node_PenZoneMastery.ParentNodeId == "Node_Root"`).
- [x] No gameplay or UI code reads this data — no `Initialize()`/subsystem/widget
      added or modified in this change.
- [x] Struct layout is documented with field-purpose (why, not what) comments.
- [x] `app-source-tracked/` mirror of both new `.h`/`.cpp` files is present and
      identical to `app/` (verified via `diff`).

## Validation evidence

`harness/run_ue_automation.sh KrowdKontrol.Unit.MasteryTreeData`:

```
UE_BUILD_START KrowdKontrolEditor Win64 Development
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
```

The real `DT_MasteryTreeTable` asset's shape was independently confirmed via a
headless `-run=pythonscript` verification call before the Automation test was even
written: `export_to_json_string()` on the loaded table shows exactly 2 rows
(`Node_Root`, `Node_PenZoneMastery`), 4 bubbles each, non-empty `EffectHookId` per
bubble, and `Node_PenZoneMastery.ParentNodeId == "Node_Root"`.

MISSION.md Hard Invariants reviewed against this diff: no kill-rule, colour-lock,
ability-roster, enemy-roster, engine/dimensionality, networking, or `app`-tracking
invariant is touched — this is new, inert data with no runtime consumer.

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
