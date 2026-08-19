# Issue #171: Fire level-failed signal and incapacitate player at zero energy

Adds a new `ULevelFailComponent` that listens to `UPlayerEnergyComponent::OnEnergyChanged`
and broadcasts a new no-payload `OnLevelFailed` delegate exactly once when energy reaches
0. `AKrowdKontrolPlayerController` subscribes to it (extending its existing
`WireWidgetsToPawn` pattern from PR #133) and reacts by disabling the possessed pawn's
input (`APawn::DisableInput`) and discarding — never recording — the level's in-progress
clear timer via a new `ULevelClearTimeSubsystem::DiscardLevelTimer` sibling method. This
is PRD "Run Lifecycle & Progression Signals" REQ-3 (P0).

## Files changed

| File | Action | Contains |
|------|--------|----------|
| `LevelFailComponent.h`/`.cpp` | CREATE | New `FOnLevelFailed` delegate + `ULevelFailComponent` |
| `LevelClearTimeSubsystem.h`/`.cpp` | UPDATE | New `DiscardLevelTimer(FName LevelID)` sibling method |
| `KrowdKontrolPlayerController.h`/`.cpp` | UPDATE | New `HandleLevelFailed()`/`ResolveLevelClearTimeSubsystem()`, cache field, subscribes in `WireWidgetsToPawn` |
| `PlayerEnergyComponent.h` | UPDATE | Appended 5th friend-class line (`FKrowdKontrolLevelFailedTest`) — issue #177's 4th line untouched |
| `FlatCamera3DPrototypePawn.h`/`.cpp` | UPDATE | Construct + bind `LevelFailComponent` |
| `Paper2DPrototypePawn.h`/`.cpp` | UPDATE | Construct + bind `LevelFailComponent` |
| `Private/Tests/LevelFailedTestListener.h`/`.cpp` | CREATE | Test-only dynamic-delegate listener |
| `Private/Tests/KrowdKontrolLevelFailedTest.cpp` | CREATE | `KrowdKontrol.Unit.LevelFailed` — the full acceptance-criteria integration test |
| `Private/Tests/KrowdKontrolLevelClearTimeSubsystemTest.cpp` | UPDATE | New `DiscardLevelTimer` no-op-on-missing-timer + discard-clears-state cases |

## Acceptance criteria

- [x] `OnLevelFailed` fires when the possessed pawn's `UPlayerEnergyComponent` reaches 0
      energy: `LevelFailComponent.cpp`, `HandleEnergyChanged`.
- [x] On zero energy: pawn input disabled via `APawn::DisableInput` and `OnLevelFailed`
      broadcasts: `KrowdKontrolPlayerController.cpp`, `HandleLevelFailed`.
- [x] No death animation/ragdoll added (placeholder-first, per issue) — not implemented.
- [x] In-progress clear timer discarded via new `ULevelClearTimeSubsystem::DiscardLevelTimer`,
      never recorded as a best: `LevelClearTimeSubsystem.cpp`, `HandleLevelFailed`'s call site.
- [x] `StopLevelTimerAndRecordClear` not repurposed or modified — `DiscardLevelTimer` is a
      separate sibling method.
- [x] New automation test drives energy to 0 and asserts `OnLevelFailed` fires exactly once,
      input is disabled, and the discard path (not the record path) is invoked:
      `KrowdKontrolLevelFailedTest.cpp`.
- [x] `python harness/ci.py` (full mode) passes: `GATE_OK mode=full`, `UNIT_PASSED tests=66`.
- [x] No hard invariant violated — pure signal/input-state plumbing, no death/kill, no new
      gameplay colour, no new ability/enemy type, no networking.
- [x] Issue #177's pre-existing, unrelated concurrent work in `app/`
      (`PunishmentManagerComponent` and friends) left untouched in `app/` itself, and
      excluded from this issue's own mirrored diff — see "Deviations from plan" below;
      an earlier pass of this PR had it leak into `app-source-tracked/`, caught and
      reverted during review.
- [x] `OnLevelFailed`'s exactly-once guarantee holds locally, not just via an upstream
      implementation detail: `ULevelFailComponent` now carries its own `bHasFired` guard
      (matching `UFirstStunBeaconComponent::bHasTriggeredBeacon`'s established pattern),
      rather than relying solely on `PlayerEnergyComponent::ApplyContactDamage`'s
      change-guard.
- [x] `AKrowdKontrolPlayerController::WireWidgetsToPawn` unbinds the previously-wired
      pawn's `LevelFailComponent` before rewiring a new one (`WiredLevelFailComponent`),
      so a future repossession can't leave a stale binding acting on the wrong pawn.

## Validation evidence

`harness/ci.py` full mode: `GATE_OK`, `UNIT_PASSED tests=66` (including the expanded
`KrowdKontrol.Unit.LevelFailed` and `KrowdKontrol.Unit.LevelClearTimeSubsystem`),
`UE_AUTOMATION_RESULT passed=1 total=1`, `UE_AUTOMATION_OK`, `E2E_PASSED steps=1`. No
hard invariants touched by this diff.

## Pass-1 E2E feedback (PR #183)

Pass-1's live-PIE E2E holdout could not confirm the zero-energy path because it tested
combat exclusively against `ATrooperEnemy` and `ARunnerEnemy` — neither of which calls
`UPlayerEnergyComponent::ApplyContactDamage` anywhere in the current codebase (their
`OnTrooperRayFired`/`OnRunnerDrainFired` delegates are cosmetic-only telegraphs today;
nothing production-side is bound to them). That's a pre-existing gap unrelated to this
issue's own diff — `ABomberEnemy::TriggerExplosion()` and `ARootSurgeBoss`'s attack
already call `ApplyContactDamage` unconditionally once their attack telegraph
completes, so a live-PIE zero-energy path already exists via those two enemy types.
Worth its own issue (Trooper/Runner ray/drain attacks not actually damaging the
player) — out of scope here, since this issue is about reacting to zero energy, not
which enemies can cause it.

To give future E2E/holdout passes a deterministic path that doesn't depend on picking
the right enemy type: added `AKrowdKontrolPlayerController::Cheat_ZeroPlayerEnergy()`
(`UFUNCTION(Exec)`, stripped from Shipping). It drains the possessed pawn's energy to 0
through repeated `ApplyContactDamage()` calls only — never a direct setter, keeping
`PlayerEnergyComponent`'s "no public mutator besides `ApplyContactDamage`" invariant
intact — then the existing `OnEnergyChanged` → `OnLevelFailed` → `HandleLevelFailed`
chain fires exactly as it would from real combat. Covered by a new case (e) in
`KrowdKontrolLevelFailedTest.cpp`.

## Deviations from plan

- `KrowdKontrolLevelFailedTest.cpp`'s test World (`FAutomationEditorCommonUtils::CreateNewMap()`)
  was missing `World->InitializeActorsForPlay(FURL())`, without which
  `AKrowdKontrolPlayerController::HandleLevelFailed` (an actor-targeted dynamic-delegate
  handler) silently never ran — `AActor::ProcessEvent` no-ops reflection-dispatched calls
  until `World->AreActorsInitialized()` is true. Fixed by adding that call up front, the
  same established fix already used by `KrowdKontrolDualZoneBossTest.cpp` and
  `KrowdKontrolMusicSubsystemTest.cpp` for the identical gotcha. No test assertions changed,
  no production code affected. See validation.md for the full root-cause account.
- Code review caught issue #177's `PunishmentManagerComponent` construction/delegate-bind
  wiring (present as legitimate, uncommitted work in the shared `app/` symlink) leaked into
  this PR's own `app-source-tracked/` mirror across both pawns and `PlayerEnergyComponent.h`'s
  friend-class list. Reverted from `app-source-tracked/` only — `app/` itself was left
  untouched since that WIP belongs to a different, concurrent task and isn't this PR's to
  edit or destroy.
- Review also flagged two test-coverage gaps (`ResolveLevelClearTimeSubsystem`'s
  resolve-from-scratch/warn-once path, and `DiscardLevelTimer`'s no-op-on-missing-timer
  contract) and a copy-paste-slip risk (`Paper2DPrototypePawn`'s wiring was never driven by
  any test). All three now have coverage: `KrowdKontrolLevelFailedTest.cpp` gained a
  no-pawn/no-subsystem degrade-safety case and a full `APaper2DPrototypePawn` wiring case;
  `KrowdKontrolLevelClearTimeSubsystemTest.cpp` gained `DiscardLevelTimer` cases mirroring
  its sibling method's existing "no active timer" coverage. The `APaper2DPrototypePawn`
  spawn needed an explicit `AlwaysSpawn` collision override — this test's
  `InitializeActorsForPlay` call activates real collision checks, and the default origin
  spawn point already has the first test pawn's `StaticMeshComponent` sitting there.
