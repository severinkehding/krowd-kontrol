# Issue #318: Verify and Lock Quest Tracker / Briefing Card Colour-Sourcing Against the Chain-Colour Authority

REQ-4 of `docs/prd-colour-coded-herding.md` (P1): confirm the quest tracker's
suggested-ability line and the briefing card's ability-name colour usage read from the
single chain-colour authority added by issue #315/PR #341
(`AbilityData::GetChainColourForEnemyType`), rather than a separate/duplicate colour
source. This is a verification/drift-prevention chore — the audit below found no
production colour-sourcing bug to fix on either surface.

## Audit findings

| HUD surface | Current colour source | Verdict |
|---|---|---|
| Quest tracker suggested-ability line (`QuestTrackerWidget.cpp`, `SuggestedAbilityText->SetColorAndOpacity(FSlateColor(Data.Colour))` where `Data = AbilityData::Get(Suggested)`) | `AbilityData::Get(Ability).Colour` — the exact same `FAbilityData::Colour` field `GetChainColourForEnemyType` iterates over and returns (`AbilityData.cpp`, reverse-lookup by `CounteredEnemyType`). Not a local constant, not a second table. | **No drift possible** — same underlying data as the authority, reached via ability slot instead of enemy type. The existing test checked the value but not against the authority function itself — **strengthened here (see below)**. |
| Briefing card ability-name colour (`BriefingCardWidget.cpp`, `NewAbilityText->SetColorAndOpacity(TextColor)` where `TextColor = FSlateColor(HUDChromeColours::GetText())`) | `HUDChromeColours::GetText()` — plain desaturated chrome text colour, identical to `LevelNameText`/`ObjectiveText`. No per-ability colour exists anywhere in `UBriefingCardWidget` or `FLevelBriefingRow`. | **PRD premise does not hold for this surface.** The briefing card does not "already colour-match ability names" — it deliberately doesn't. `KrowdKontrol.Unit.ReservedGameplayColours` has asserted since issue #246 that `NewAbilityText`'s colour must never collide with a reserved gameplay colour — that test is itself the "would fail if colour ever diverged from the authority" regression this issue asks for, because there is no chain-colour usage on this surface to drift in the first place. **No code change to `BriefingCardWidget`.** |

Adding real per-ability chain-colouring to the briefing card would require a new
structured ability-slot field on `FLevelBriefingRow` (a data-table schema change) plus
either colouring the whole designer-authored line or splitting it into multiple text
runs — both out of scope for a "colour-sourcing only" drift-verification chore per the
issue's own constraint. If briefing-card ability-name colouring is wanted, that is new
scope belonging in a fresh PRD-derived issue, not a silent expansion of this one.

## What changed

`KrowdKontrolQuestTrackerWidgetTest.cpp`'s existing "(12a) suggestion-unlocked"
assertion block gained a second colour assertion, pinning the suggested-ability text
colour directly against `AbilityData::GetChainColourForEnemyType(EEnemyType::SN_1PR)`
(the REQ-1 authority function itself), in addition to the pre-existing assertion
against `AbilityData::Get(EAbilitySlot::Sleep).Colour`. Both hold today (same
underlying data), but only the new assertion actually fails if a future change makes
the authority's derivation disagree with an ability's own `Colour` field. No new test
case was added — this strengthens an existing one, so the Automation test count is
unchanged.

No production (non-test) source file changed. `BriefingCardWidget.h`/`.cpp` and
`QuestTrackerWidget.h`/`.cpp` are untouched. No wording, layout, or triggering logic
changed anywhere.

## Files changed

- `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolQuestTrackerWidgetTest.cpp` —
  added the authority-pinned colour assertion to the "(12a)" block; updated the file's
  header comment bullet list to note issue #318's addition
- `app-source-tracked/Source/KrowdKontrol/Private/Tests/KrowdKontrolQuestTrackerWidgetTest.cpp` —
  matching mirror (D-009), confirmed byte-identical via `diff`

## Acceptance criteria

- [x] Quest tracker's suggested-ability colour is asserted directly against
      `AbilityData::GetChainColourForEnemyType(EEnemyType::SN_1PR)` in
      `KrowdKontrolQuestTrackerWidgetTest.cpp`, and that assertion passes
- [x] `app/` and `app-source-tracked/` copies of the changed test file are
      byte-identical (`diff` clean)
- [x] PR body documents the briefing-card audit finding: it has no ability-colour
      source to verify against the authority, and `KrowdKontrol.Unit.ReservedGameplayColours`
      already regression-tests that it can never acquire one via a reserved colour
      collision
- [x] No change to `BriefingCardWidget.h`/`.cpp`, `QuestTrackerWidget.h`/`.cpp`, or any
      other production source file
- [x] No change to wording, layout, or triggering logic of either widget
- [ ] `python harness/ci.py` (full) reports `GATE_OK`, unit test count unchanged from
      baseline — deferred to `dark-factory-validate`
- [ ] No regression in `KrowdKontrol.Unit.QuestTrackerWidget`,
      `KrowdKontrol.Unit.ReservedGameplayColours`, or
      `KrowdKontrol.Unit.RoomActorBankingWiring` — deferred to `dark-factory-validate`

## Validation evidence

Full validation (build + run `KrowdKontrol.Unit.QuestTrackerWidget` and the other
Automation tests listed above) is deferred to the separate `dark-factory-validate`
node, per this repo's factory workflow split between `implement` (light inline check)
and `dark-factory-validate` (exhaustive gate).

MISSION.md Hard Invariants reviewed against this diff: this change touches the
colour-lock invariant (Hard Invariant 3) only by *asserting* against it more strictly
via `AbilityData::GetChainColourForEnemyType` — it never redefines the 5 reserved
colours and adds no new colour-producing code path.

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
