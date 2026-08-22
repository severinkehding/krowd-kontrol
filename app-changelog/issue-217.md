# Issue #217: Call NotifyLevelReached on level start so ability unlocks fire in real play

## Summary

`UAbilityUnlockComponent::NotifyLevelReached()` already held the correct level→ability
mapping (2→Sleep, 3→Root, 4→Fear, 5→Snare, from issue #69) and was fully covered by
unit tests, but nothing in production code ever called it — only test files did. As a
result, Sleep/Root/Fear/Snare stayed permanently locked in real play (only Stun, which
unlocks at construction, ever worked), confirmed by the issue author across two operator
playtests. This fix adds `UAbilityUnlockLevelSubsystem`, a `UWorldSubsystem` that
subscribes to the already-existing `ULevelLifecycleSubsystem::OnLevelBegin` signal in
`Initialize()` (mirroring the already-merged `UCrowdMasterySubsystem` precedent exactly)
and forwards the level index — parsed from the map's `L_LevelNN` name, with an interim
default of level 1 for non-matching/prototype maps — to the possessed pawn's
`UAbilityUnlockComponent`.

**Post-review fix pass**: code review flagged that `HandleLevelBegin`'s pawn lookup had
no fallback if `OnLevelBegin` (which fires exactly once per world) beat
`AutoPossessPlayer`'s pawn possession — an unrecoverable, silent miss reproducing the
original bug in a narrower window. Fixed by adding
`UAbilityUnlockLevelSubsystem::RetryPendingUnlockForPawn()`, called from
`AKrowdKontrolPlayerController::BeginPlay()`/`OnPossess()` (mirroring that class's
existing `WireWidgetsToPawn`/`ApplyBossCheckpointIfRequested` dual-call-site idiom for
the same timing hazard) — this does modify `KrowdKontrolPlayerController.h/.cpp` and
`AbilityUnlockComponent.h`'s stale doc comment, unlike the original scope-discipline
claim above. Also removed a dead `friend class` declaration (granted no access beyond
already-`public` members) and added test coverage for the missing-pawn/-component
branch and the new retry path.

## Acceptance Criteria

| Criterion | Status |
|---|---|
| Loading level 2 unlocks Sleep in real play | Done — `HandleLevelBegin` forwards `NotifyLevelReached(2)`; covered by `KrowdKontrol.Unit.AbilityUnlockLevelSubsystem` case (b). |
| Loading level 3 unlocks Root in real play | Done — same wiring, level index 3; covered by case (b). |
| Loading level 4 unlocks Fear in real play | Done — same wiring, level index 4; covered by case (b). |
| Loading level 5 unlocks Snare in real play | Done — same wiring, level index 5; covered by case (b). |
| Fires from the real `OnLevelBegin` broadcast, not just a direct function call | Done — case (c) drives `ULevelLifecycleSubsystem::OnWorldBeginPlay()` and asserts the subscription set up in `Initialize()` reaches `UAbilityUnlockComponent`. |
| No unintended early unlocks (level 1 / prototype maps) | Done — `ParseLevelIndexFromMapName` defaults to level 1 for `L_Level01` and unrecognised map names (`L_FlatCamera3DPrototype`, `L_Paper2DPrototype`), which `NotifyLevelReached` already documents as a safe no-op; asserted in cases (a)–(c). |
| PIE map-name mangling handled | Done — `UWorld::RemovePIEPrefix()` strips the `UEDPIE_0_` prefix before parsing, matching the existing `AKrowdKontrolPlayerController::StripPIEPrefixFromMapName()` idiom; asserted in case (a). |

## Validation Evidence

- `python harness/ci.py` (full mode) → `GATE_OK`: `UNIT_PASSED tests=82` (includes
  `KrowdKontrol.Unit.AbilityUnlockLevelSubsystem`), `UE_BUILD_OK`,
  `UE_AUTOMATION_RESULT passed=1 total=1`, `E2E_PASSED steps=1`. Passed clean on first
  run, no fixes required.
- Scope check: diff touches the three original new files plus, after the review fix
  pass, `KrowdKontrolPlayerController.h/.cpp` (retry wiring) and
  `AbilityUnlockComponent.h` (one comment line) under
  `app-source-tracked/Source/KrowdKontrol/...` — no governance files, no `.github/`, no
  deploy config.
- `app/` vs `app-source-tracked/` parity: confirmed byte-identical via `diff` for all
  six touched files (no concurrent-task leakage from the shared `app/` symlink).
- Hard invariants (MISSION.md): reviewed by inspection — the change only wires an
  existing ability-unlock mapping into real play; introduces no new ability, colour, or
  enemy type, and doesn't touch kill/banking rules, engine choice, or networking scope.
  No regression against invariants #1-8.

See `implementation.md` and `validation.md` in the workflow run artifacts for the full
record.
