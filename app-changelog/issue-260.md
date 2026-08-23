# Issue #260: Ability tray hover tooltips

Adds a compact hover tooltip to each of the 5 ability tray tiles
(`UAbilityCooldownTrayWidget`), sourced entirely from `AbilityData`: ability name, the
canonical key binding, a one-line effect description, duration, range/shape, and the
ability's colour-matched enemy type with its reserved-colour swatch. Standard UMG
hover detection (`UWidget::SetToolTip`) does the triggering — no new input/cursor code
was added. `AbilityData` gained two new per-ability fields (`EffectDescription`,
`KeyBindingLabel`) to carry the two pieces of tooltip content that didn't already exist
anywhere in the codebase; everything else is read from `AbilityData`'s existing
fields. The 5 canonical key bindings (LMB=Stun, RMB=Sleep, Q=Root, E=Snare, MMB=Fear)
follow the operator's 2026-08-23 ruling recorded in `docs/prd-ability-tray-ux.md` REQ-2
— the live 1-5 `DefaultInput.ini` mappings remain functional legacy alternates and are
deliberately never surfaced in UI. No rebinding system was built.

## Acceptance criteria

- [x] Hovering a tray tile shows a compact tooltip with: ability name, canonical key
      binding, one-line effect description, duration, range/shape, and the
      colour-matched (or colour-neutral, for Stun) enemy type with its reserved-colour
      swatch (`UAbilityTooltipWidget`, wired via `IconBorder->SetToolTip()` in
      `AbilityCooldownTrayWidget::BuildWidgetTree()`).
- [x] All tooltip content is sourced from `AbilityData` (including the 2 new fields)
      — no hardcoded per-tile string in `AbilityTooltipWidget.cpp` (the
      `Range`/`TargetType`→text helpers are per-enum, not per-ability).
- [x] Tooltip chrome (background, border, all 6 text rows) uses only
      `HUDChromeColours`; the swatch is the tooltip's sole reserved-colour use —
      confirmed by `KrowdKontrol.Unit.ReservedGameplayColours`'s new section (6).
- [x] `KrowdKontrol.Unit.AbilityTooltipWidget` passes, asserting the tooltip widget
      populates every display field from a given ability's `AbilityData` — at the
      widget/data-binding level, independent of live cursor input.
- [x] The 5 canonical key bindings (LMB=Stun, RMB=Sleep, Q=Root, E=Snare, MMB=Fear)
      match the operator's 2026-08-23 ruling exactly, pinned by dedicated test
      assertions in both `KrowdKontrol.Unit.AbilityTooltipWidget` and
      `KrowdKontrol.Unit.AbilityData`.
- [x] No rebinding system was built; 1-5 legacy bindings remain functional but
      unsurfaced.
- [x] `python harness/ci.py` (full mode) reports `GATE_OK` with no regression in any
      pre-existing `KrowdKontrol.Unit.*` test.
- [x] `app/` and `app-source-tracked/` copies of every changed/new file are
      byte-identical (`diff`, confirmed for all 9 touched files).

## Files changed

- `AbilityData.h` / `.cpp` (UPDATE) — new `EffectDescription`/`KeyBindingLabel` fields,
  populated for all 5 abilities.
- `AbilityTooltipWidget.h` / `.cpp` (CREATE) — new self-built `UUserWidget`, mirrors
  `OnScreenPromptWidget`'s lifecycle pattern.
- `AbilityCooldownTrayWidget.h` / `.cpp` (UPDATE) — constructs one tooltip per slot in
  `BuildWidgetTree()`'s loop and attaches via `SetToolTip()`; added a friend-class
  declaration for the new test's `SlotIconBorders` integration check.
- `Private/Tests/KrowdKontrolAbilityTooltipWidgetTest.cpp` (CREATE) —
  `KrowdKontrol.Unit.AbilityTooltipWidget`.
- `Private/Tests/KrowdKontrolReservedGameplayColoursTest.cpp` (UPDATE) — new audit
  section (6) for the tooltip's chrome colours, with the swatch's deliberate
  reserved-colour collision pinned in (6a).
- `Private/Tests/KrowdKontrolAbilityDataTest.cpp` (UPDATE) — extended each ability's
  block with `KeyBindingLabel`/`EffectDescription` assertions.

## Validation evidence

Full gate (`python harness/ci.py`, mode=full):

```
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=99
APP_STARTED driver=cli
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
HOLDOUT_ABSENT no .factory/holdout/run.py
MUTATIONS_ABSENT no harness/mutations/run.py
GATE_OK mode=full
```

Targeted filter re-runs during implementation additionally confirmed, each
`passed=1 total=1`: `KrowdKontrol.Unit.AbilityTooltipWidget`,
`KrowdKontrol.Unit.ReservedGameplayColours`, `KrowdKontrol.Unit.AbilityData`, and
`KrowdKontrol.Unit.AbilityCooldownTrayWidget` (no regression from the tooltip wiring
added to its `BuildWidgetTree()` loop).

A real hover-triggered visual tooltip display was not exercised end-to-end by
automation — no input-simulation primitive exists in this project's headless
`-nullrhi` Automation run to drive a live mouse hover inside PIE (matches this issue's
own stated test boundary: "can be tested at the widget/data-binding level independent
of live cursor input"). This is a known, pre-documented gap, not unique to this issue.

MISSION.md Hard Invariants reviewed against this diff: Hard Invariant 3 (the 5
reserved gameplay colours) is honored — chrome uses only `HUDChromeColours`, and the
swatch's use of a reserved colour is the tooltip's sole, documented, tested exception.
Hard Invariant 4 (Stun has no countered-enemy colour matchup) is honored — Stun's
tooltip renders "No enemy colour match", tested explicitly. No kill-rule, ability-
roster, enemy-roster, engine/dimensionality, networking, or `app`-tracking invariant is
touched.

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
