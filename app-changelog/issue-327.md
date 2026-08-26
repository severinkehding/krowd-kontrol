# Issue #327: Crowd Mastery total does not persist across level clears

Adds `UCrowdMasteryTotalSubsystem` (`app/Source/KrowdKontrol/CrowdMasteryTotalSubsystem.h/.cpp`),
a new `UGameInstanceSubsystem` that owns a single accumulated Crowd Mastery total,
initialized to 0 at GameInstance startup. The existing world-scoped
`UCrowdMasterySubsystem` (which tracks a per-run peak, `RunningMaxControlledCount`) now
additionally subscribes to `ULevelLifecycleSubsystem::OnLevelClear` and deposits that
run's value into the new total via `DepositRunMastery(int32)`.

This is the foundation issue (REQ-1) for `docs/prd-crowd-mastery-persistence.md`. It is
intentionally data-layer only - no UI is added; the menu-display (REQ-2) and
reset-control (REQ-3) issues are separate follow-ups that build on this subsystem's
`GetAccumulatedTotal()` / `ResetAccumulatedTotal()` API.

## Acceptance criteria

- [x] `UCrowdMasteryTotalSubsystem` (a `UGameInstanceSubsystem`) owns a single
      accumulated Crowd Mastery total, initialized to 0 at GameInstance startup
- [x] On `ULevelLifecycleSubsystem::OnLevelClear`, the run's Crowd Mastery value
      (`UCrowdMasterySubsystem::GetRunningMaxControlledCount()`) is added to the
      subsystem's total
- [x] The total survives level transitions, level reruns, and returns to the main menu
      within a single session (guaranteed by construction - `UGameInstanceSubsystem`
      field lives for the GameInstance's full lifetime)
- [x] Exposes `GetAccumulatedTotal()` (read) and `ResetAccumulatedTotal()` (zero) - no
      UI added by this issue
- [x] `UCrowdMasteryTotalSubsystem` is the sole authority for the accumulated total
- [x] Unit test coverage: deposit-on-clear, accumulation across two runs, reset-to-zero
- [x] `app/` and `app-source-tracked/` copies byte-identical for every touched file
- [x] `app-changelog/issue-327.md` written (this file)

## Validation evidence

`python harness/ci.py --mode full`: `GATE_OK` -
`UNIT_PASSED tests=112`, `PIE_PASSED tests=5`, `UE_AUTOMATION_OK passed=1 total=1`,
`E2E_PASSED steps=1`.

Hard invariants (MISSION.md's 8): reviewed, none implicated by a scoring/mastery-total
subsystem - no regression found. `app-source-tracked/` mirror contains only the changed
`.h`/`.cpp`/test files (no `.uasset`/`.umap`/`Content/`/`Binaries/`/`Intermediate/`),
satisfying invariant #8's carve-out.

## Scope limits (not built here)

- No UI (menu display is REQ-2, separate follow-up)
- No reset control/confirm UI (REQ-3, separate follow-up)
- No save-file/cross-launch persistence (REQ-4, explicitly deferred)
- No end-to-end Automation test of the real `GetGameInstance()`-based successful-deposit
  path - matches this codebase's established precedent
  (`KrowdKontrolLevelClearTimeWiringTest.cpp`: `CreateNewMap()` worlds have a null
  `GetGameInstance()`, untestable "by design"). A manual PIE check before merge is
  recommended, same as issue #170's precedent.

## Review follow-up (2026-08-26)

Automated review (PR #331) surfaced one HIGH, three MEDIUM (one standalone, two bundled
into the same fix), and two LOW findings. All fixed in a follow-up commit:

- `HandleLevelClear()`'s null-`GameInstance` branch now returns silently instead of
  logging a warning, matching the precedent its own doc comment already claimed to
  follow (`ULevelLifecycleSubsystem::EnsureLevelClearTimeSubscription()`, which stays
  silent for exactly this case since it is the normal state for this project's
  `CreateNewMap()`-based Automation test worlds). This also stops the warning from
  firing during several pre-existing, unrelated tests
  (`KrowdKontrolLevelLifecycleSubsystemTest.cpp`, `KrowdKontrolLevelSequenceSubsystemTest.cpp`,
  `KrowdKontrolPostRunSummaryWidgetWiringTest.cpp`) that broadcast `OnLevelClear` in
  `CreateNewMap()` worlds. Only one failure condition (missing
  `UCrowdMasteryTotalSubsystem`) now uses `bHasWarnedMissingCrowdMasteryTotalSubsystem`,
  so its doc comment was trimmed to match, and the field's citation to
  `ULevelLifecycleSubsystem`'s precedent is now accurate (this one fix resolved three
  separate review findings that shared the same root cause).
- Added a wiring test (case (g) in `KrowdKontrolCrowdMasterySubsystemTest.cpp`) that
  asserts `Initialize()` really binds `HandleLevelClear` to
  `ULevelLifecycleSubsystem::OnLevelClear` via a real `AddDynamic` binding - mirroring
  this same file's existing case (e) for the sibling `OnLevelBegin` binding. This was
  the only automation coverage gap in the PR: the actual "deposit-on-clear" wiring had
  zero test coverage before this fix.
- Corrected `CrowdMasteryTotalSubsystem.h`'s citation of `ULevelClearTimeSubsystem`'s
  `GetWorld()`/`GetGameInstance()`-free rationale from "top-of-file rationale" (that
  rationale is not actually at the top of that file) to "`SubscribeToLevelLifecycle()`'s
  doc comment," where it actually lives.
- Added this changelog file, which the PR description claimed existed but had not
  actually been written.

`python harness/ci.py --mode full`: `GATE_OK` -
`UNIT_PASSED tests=112`, `PIE_PASSED tests=5`, `UE_AUTOMATION_OK passed=1 total=1`,
`E2E_PASSED steps=1`. `harness/run_ue_automation.sh KrowdKontrol.Unit.CrowdMasterySubsystem`
independently rebuilt and passed after these changes.

---

The real Unreal project stays under `app/` (gitignored, D-003) - this changelog and its
matching `app-source-tracked/` copy are the tracked-repo record of that change, per
D-009. Not a substitute for reading `app-source-tracked/` directly.
