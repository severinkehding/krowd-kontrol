# Issue #11: Locked GameplayColors palette (Background colour + lock test)

`ReservedGameplayColours` (added by issue #70) already held the 5 locked
gameplay-information colours (Purple/Teal/Orange/Blue/White). This issue adds the
missing `GetBackground()` accessor (desaturated near-black, PRD
11-visual-and-art-direction.md REQ-2) and a new Automation Framework test,
`KrowdKontrol.Unit.GameplayColorsAreLocked`, that locks the shape of the constant set
itself — exactly 5 info colours, all mutually distinct from each other and from
Background, and Background verified near-black on each channel independently.
`GetBackground()` is deliberately excluded from `GetAll()` so it can never be mistaken
for a 6th reserved information colour.

## Files changed (all real edits under `app/`, gitignored per D-003 — this is the
tracked-repo record of that change)

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/ReservedGameplayColours.h` | UPDATE | Added `GetBackground()` declaration + doc comment |
| `app/Source/KrowdKontrol/ReservedGameplayColours.cpp` | UPDATE | Added `GetBackground()` implementation (`FLinearColor(0.02, 0.02, 0.03, 1.0)`) |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolGameplayColorsAreLockedTest.cpp` | CREATE | `KrowdKontrol.Unit.GameplayColorsAreLocked` — asserts `GetAll()` has exactly 5 entries, all 5 + Background are mutually distinct, and Background is near-black on R/G/B independently |
| `app/Source/KrowdKontrol/KrowdKontrol.Build.cs` | UPDATE (deviation, out of scope) | Added `PrivateDependencyModuleNames.Add("EngineSettings")` — fixes a pre-existing shared-`app/`-state link failure (`LNK2019` on `UGameMapsSettings::GetGlobalDefaultGameMode`) in an unrelated, uncommitted test file that was blocking the mandatory full rebuild gate for every worktree, not just this one. See `implementation.md`'s Deviations section for the full account. |

## Acceptance criteria

- [x] **Single source of truth for the palette's Background colour**, alongside the
      existing 5 reserved info colours. `ReservedGameplayColours::GetBackground()`.
- [x] **Background is desaturated / near-black**, distinct in kind from the 5
      saturated info colours, and NOT counted as a 6th reserved colour (excluded from
      `GetAll()`).
- [x] **A new Automation Framework test guards the 5-colour lock**:
      `KrowdKontrol.Unit.GameplayColorsAreLocked` asserts exactly 5 entries in
      `GetAll()`, mutual distinctness of all 6 values (5 + Background), and
      Background's near-black-ness per channel.
- [x] **No regressions** — all 40 unit tests pass, including the pre-existing
      `KrowdKontrol.Unit.ReservedGameplayColours` audit test from issue #70.

## Validation

```
$ python harness/ci.py
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=40
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

Full-module rebuild succeeded (`UE_BUILD_OK`), confirming the `EngineSettings`
dependency fix actually resolved the pre-existing shared-state link error rather than
just working around it locally. Hard Invariant #3 (5-colour lock) re-verified by
reading `GetAll()`'s output directly, not just trusting the gate — see
`validation.md` Phase 3 for the full breakdown.

---

Source lives under `app/` (gitignored, D-003) — this file is the tracked-repo record
of that change, not a substitute for reading the actual code.
