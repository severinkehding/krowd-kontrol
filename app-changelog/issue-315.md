# Issue #315: Enemy-Type-to-Chain-Colour Authority

Adds `AbilityData::GetChainColourForEnemyType(EEnemyType)`
(`app/Source/KrowdKontrol/AbilityData.h`/`.cpp`), REQ-1 of
`docs/prd-colour-coded-herding.md`: a single lookup that derives each of the 4 locked
enemy types' "chain colour" from the existing `FAbilityData::CounteredEnemyType`/
`Colour` fields (PR #303's colour-match data), rather than inventing a second
hardcoded colour table. The implementation iterates `AbilityData::GetAll()` and
returns the `Colour` of the one non-colour-neutral ability whose `CounteredEnemyType`
matches the input, explicitly skipping Stun (`bIsColourNeutral`) so its meaningless
default `CounteredEnemyType` (`RU_NNR`) can never shadow Snare's real `RU_NNR`
matchup. This is a backend data-authority change with no player-visible output by
itself — REQ-2 (enemy tint), REQ-3 (pen/pole tint), and REQ-4 (HUD agreement) are
separate, later issues that will call this function once it exists.

## Acceptance criteria

- [x] `AbilityData::GetChainColourForEnemyType(EEnemyType)` exists, is
      `KROWDKONTROL_API`, and resolves all 4 `EEnemyType` values to one of the 5
      `ReservedGameplayColours` entries
- [x] Each resolved colour exactly matches its enemy type's PR #303 colour-match
      target (RU-NNR↔Snare/Purple, TR-UPR↔Root/Teal, B0-0MR↔Fear/Orange,
      SN-1PR↔Sleep/Blue)
- [x] White is never returned for any of the 4 enemy types
- [x] No duplicate colour table exists anywhere — the implementation derives from
      `AbilityData::GetAll()`, not a second hardcoded switch/map
- [x] `KrowdKontrol.Unit.ChainColour` exists, compiles, and passes, covering all 4
      bullets above
- [x] `app/` and `app-source-tracked/` copies of all changed/new files are identical
      (`diff` clean)
- [x] `python harness/ci.py` (full) reports `GATE_OK`, unit test count incremented by
      1 versus baseline
- [x] No regression in `KrowdKontrol.Unit.AbilityColourMatch` or
      `KrowdKontrol.Unit.ReservedGameplayColours` (neither file is modified by this
      change; re-run individually to confirm no incidental breakage)

## Validation evidence

Full gate (`python harness/ci.py`, mode=full):

```
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=120
PIE_PASSED tests=5
APP_STARTED driver=cli
UE_BUILD_START KrowdKontrolEditor Win64 Development
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
HOLDOUT_ABSENT no .factory/holdout/run.py
MUTATIONS_ABSENT no harness/mutations/run.py
GATE_OK mode=full
```

`UE_AUTOMATION_RESULT passed=1 total=1` is `KrowdKontrol.Unit.ChainColour`, the new
test added by this change. No regression in the pre-existing
`KrowdKontrol.Unit.AbilityColourMatch` or `KrowdKontrol.Unit.ReservedGameplayColours`
tests (neither is modified by this change; both remain part of the 120 passing unit
tests counted above).

MISSION.md Hard Invariants reviewed against this diff: this change touches the
colour-lock invariant (Hard Invariant 3) and the enemy-roster invariant (Hard
Invariant 5) only by *reading* them via `ReservedGameplayColours`/`EnemyType.h` — it
never redefines the 5 RGB values or the 4-enemy roster, and the ability-roster
invariant (Hard Invariant 4, Stun's colour-neutral status) is explicitly preserved by
the `bIsColourNeutral` guard.

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
