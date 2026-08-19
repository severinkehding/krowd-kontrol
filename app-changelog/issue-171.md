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
      (`PunishmentManagerComponent` and friends) left untouched, not included in this
      issue's own mirrored diff — see validation.md's "Concurrent-task state check".

## Validation evidence

`harness/ci.py` full mode: `GATE_OK`, `UNIT_PASSED tests=66` (including the new
`KrowdKontrol.Unit.LevelFailed`), `UE_AUTOMATION_RESULT passed=1 total=1`,
`UE_AUTOMATION_OK`, `E2E_PASSED steps=1`. No hard invariants touched by this diff.

## Deviations from plan

- `KrowdKontrolLevelFailedTest.cpp`'s test World (`FAutomationEditorCommonUtils::CreateNewMap()`)
  was missing `World->InitializeActorsForPlay(FURL())`, without which
  `AKrowdKontrolPlayerController::HandleLevelFailed` (an actor-targeted dynamic-delegate
  handler) silently never ran — `AActor::ProcessEvent` no-ops reflection-dispatched calls
  until `World->AreActorsInitialized()` is true. Fixed by adding that call up front, the
  same established fix already used by `KrowdKontrolDualZoneBossTest.cpp` and
  `KrowdKontrolMusicSubsystemTest.cpp` for the identical gotcha. No test assertions changed,
  no production code affected. See validation.md for the full root-cause account.
