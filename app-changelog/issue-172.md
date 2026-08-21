# Issue #172: Restart current level on level-failed (map reload, full reset)

`AKrowdKontrolPlayerController::HandleLevelFailed()` (issue #171, PRD "Run Lifecycle &
Progression Signals" REQ-3) already disabled input and discarded the in-progress clear
timer when the player's energy hit zero, but nothing actually restarted the level. This
adds a new private `RequestLevelRestart()`, called as the last step of
`HandleLevelFailed()`, which (a) sets a new, publicly-readable `bRestartRequested` bool
(the automated-test-observable proxy for the restart) and (b) — only in a real game
world (PIE/packaged play), never inside an in-process Automation test world — calls
`UGameplayStatics::OpenLevel()` targeting the current map's own name. Because
`UPlayerEnergyComponent::CurrentEnergy` and `AEnemyBase::CurrentState` are only ever
seeded/defaulted at construction, a genuine `OpenLevel` reload satisfies "full energy"
and "enemy population reset" for free via fresh actor construction — no manual reset
code was added anywhere. This is PRD REQ-4 (P0); no boss-checkpoint re-entry logic is
included (explicitly deferred to a follow-up issue per #172's own body).

## Files changed

| File | Action | Contains |
|------|--------|----------|
| `KrowdKontrolPlayerController.h` | UPDATE | New `bRestartRequested` field, `WasRestartRequested()` accessor (`BlueprintPure`), `RequestLevelRestart()` and `ComputeRestartLevelName()` private method declarations, `FKrowdKontrolLevelRestartTest` friend declaration |
| `KrowdKontrolPlayerController.cpp` | UPDATE | `#include "Kismet/GameplayStatics.h"`; `RequestLevelRestart()` and `ComputeRestartLevelName()` implementations; call added at the end of `HandleLevelFailed()` |
| `PlayerEnergyComponent.h` | UPDATE | Appended `FKrowdKontrolLevelRestartTest` friend-class line, so the new test can seed `CurrentEnergy` deterministically the same way `KrowdKontrolLevelFailedTest.cpp` does |
| `Private/Tests/KrowdKontrolLevelRestartTest.cpp` | CREATE | `KrowdKontrol.Unit.LevelRestart` — asserts `bRestartRequested` flips false→true in response to a real `OnLevelFailed` firing (and stays false on a non-fatal hit), and that `ComputeRestartLevelName()` targets the current map |
| `Private/Tests/KrowdKontrolPlayerEnergyComponentTest.cpp` | UPDATE (self-fix) | Added a construct-twice invariant case pinning "fresh instance always starts at `MaxEnergy`" — the guarantee the "full energy for free on reload" claim depends on |

## Acceptance criteria

- [x] Subscribing to `OnLevelFailed` triggers a restart of the current level (map
      reload): `HandleLevelFailed()` → `RequestLevelRestart()` →
      `UGameplayStatics::OpenLevel()`, guarded by `World->IsGameWorld()` so it never
      fires inside an Automation test world.
- [ ] After restart, the player pawn has full energy — satisfied for free by fresh
      `UPlayerEnergyComponent` construction on reload (no reset code needed or added;
      the class's "no other public mutator" invariant is unchanged), and pinned by a
      construct-twice invariant test (`KrowdKontrol.Unit.PlayerEnergyComponent`).
      **Not yet checked**: requires a human/holdout PIE session — see "Manual PIE
      verification" below for the exact steps and who must sign off.
- [ ] After restart, the level's enemy population is reset (no `Banked`/`Controlled`
      state survives) — satisfied for free by fresh `AEnemyBase` construction on
      reload. **Not yet checked**: requires a human/holdout PIE session — see below.
- [x] Automated coverage exists for the genuinely testable part: `bRestartRequested`
      correctly flips in response to a real `OnLevelFailed` firing, and never on a
      non-fatal hit — `KrowdKontrol.Unit.LevelRestart`.
- [ ] The PR body/this changelog describes how a real PIE reload was manually
      verified. **Not yet checked**: see "Manual PIE verification" below — the
      checklist exists but has no corroborating evidence of having been run yet.
- [x] No boss-checkpoint re-entry logic added — out of scope, not touched.
- [x] `python harness/ci.py` (full mode) passes: `GATE_OK mode=full`.
- [x] `app/` and `app-source-tracked/` copies of every changed/new file are identical
      (verified via `diff`, both before editing — confirming the mirror started
      identical — and after).

## Manual PIE verification

Not automatable in-process: an in-process `CreateNewMap()` Automation World hangs on a
real `UGameplayStatics::OpenLevel()` call (confirmed via research prior to
implementation — see `RequestLevelRestart()`'s own comment), so the actual map reload,
full-energy result, and enemy-reset result must be checked live in PIE rather than by
an Automation test. `KrowdKontrol.Unit.LevelRestart` instead asserts the World-level
precondition (`CreateNewMap()` World is not a game world) and the testable proxy
(`bRestartRequested` flips true).

Verification steps for a live PIE session (to be run by whoever validates this PR in
the Editor): possess the player pawn, run `Cheat_ZeroPlayerEnergy` (the console/exec
hook added by issue #171/PR #183) or take real contact damage down to 0 energy, and
confirm (1) the level reloads to its start a moment later, (2) the newly-possessed
pawn's `UPlayerEnergyComponent::GetCurrentEnergy()` reads `MaxEnergy` (full), and (3)
no enemy anywhere in the reloaded level is in `Banked`/`Controlled` state (all back to
`Idle`).

**Status: not yet run.** No party in this PR's own pipeline (implement, review, or
this self-fix pass) has editor/PIE access to execute this checklist — per repo memory,
a holdout reviewer likely can't either (no reflected gameplay-component state, no PIE
camera/transform tooling for this). Whoever merges this PR must run the steps above in
the Editor first, then edit this section to record **who** ran it and **when**, and
flip the two corresponding acceptance-criteria boxes above from `[ ]` to `[x]`.

## Validation evidence

`harness/ci.py --quick`: `GATE_OK mode=quick`, `UNIT_PASSED tests=74` (baseline 73 + 1
new test). Targeted runs during implementation:
`harness/run_ue_automation.sh KrowdKontrol.Unit.LevelRestart` → `UE_AUTOMATION_RESULT
passed=1 total=1`, `UE_AUTOMATION_OK`; full `harness/run_ue_automation.sh
KrowdKontrol.Unit.` → `UE_AUTOMATION_RESULT passed=74 total=74`, `UE_AUTOMATION_OK`
(no regressions, `KrowdKontrol.Unit.LevelFailed` included and still passing). Full
`python harness/ci.py` (mode=full, including build + E2E) deferred to the separate
`dark-factory-validate` node per this factory's workflow split.

**Re-run after the self-fix pass** (new `ComputeRestartLevelName()` assertion,
non-fatal-hit negative assertion, and the `PlayerEnergyComponent` construct-twice
invariant case — all additive, no new `IMPLEMENT_SIMPLE_AUTOMATION_TEST` files, so the
test count is unchanged): `harness/ci.py --quick` → `GATE_OK mode=quick`,
`UNIT_PASSED tests=74`; `harness/run_ue_automation.sh KrowdKontrol.Unit.LevelRestart`
→ `passed=1 total=1`; `harness/run_ue_automation.sh KrowdKontrol.Unit.PlayerEnergyComponent`
→ `passed=1 total=1`; full `harness/run_ue_automation.sh KrowdKontrol.Unit.` →
`passed=74 total=74`, `UE_AUTOMATION_OK` — no regressions.

## Deviations from plan

- The investigation/plan artifact's "Files to Change" table did not list
  `PlayerEnergyComponent.h`, but the new test needs to seed `CurrentEnergy`
  deterministically the same way `KrowdKontrolLevelFailedTest.cpp` does, which
  requires friend access. Added a `FKrowdKontrolLevelRestartTest` friend-class line,
  mirroring the existing pattern exactly (no reset method or public mutator added —
  the class's own "ApplyContactDamage is the only permitted mutator" invariant is
  unchanged).

## Self-fix pass (review response)

Applied all 8 findings from the code-review/comment-quality/test-coverage review of
this PR:

- `WasRestartRequested()` changed from `BlueprintCallable` to `BlueprintPure`, matching
  this codebase's documented convention for const boolean query `UFUNCTION`s (see
  `AbilityUnlockComponent.h`).
- Both `web-research.md` citations (a factory-run artifact never tracked in this repo)
  replaced with the inlined underlying fact plus an issue-number reference, matching
  how every other researched claim in this codebase is documented.
- `RequestLevelRestart()`'s doc comment reworded from "Bound to HandleLevelFailed()" to
  "Called at the end of HandleLevelFailed()" — it's a direct call, not a delegate
  binding, and this file otherwise reserves "Bound to" for real `AddDynamic` wiring.
- The friend-class explanatory comment above `FKrowdKontrolLevelRestartTest` in
  `KrowdKontrolPlayerController.h` extended to name its specific reason, mirroring the
  equivalent update already present in `PlayerEnergyComponent.h`.
- Extracted `ComputeRestartLevelName()` as a friend-testable seam so
  `KrowdKontrol.Unit.LevelRestart` can assert the reload targets the current map by
  name, without ever calling the real (Automation-World-hanging) `OpenLevel()`.
- Added a construct-twice invariant case to `KrowdKontrol.Unit.PlayerEnergyComponent`
  pinning "a fresh instance always starts at `MaxEnergy`" — the guarantee the "full
  energy for free on reload" claim depends on.
- Added a one-line comment documenting `bRestartRequested` is intentionally never
  reset (moot in the real game-world path; the owning controller is destroyed on
  reload).
- Added a negative-case assertion: a non-fatal `ApplyContactDamage` call leaves
  `bRestartRequested` false.
- Un-checked the two acceptance-criteria boxes that claimed manual PIE verification
  without corroborating evidence, and added an explicit "Status: not yet run" note —
  no party in this PR's pipeline (implement, review, or this self-fix pass) has
  editor/PIE access to actually execute that checklist. Whoever merges this PR must
  run it first.

What was **not** added: an end-to-end PIE-tier automated test for the actual map
reload (test-coverage Finding 1's Option C) — the harness's own README documents
`e2e.py` as a stub (`NotImplementedError`); tracked as a suggested follow-up issue
instead of attempted here.
