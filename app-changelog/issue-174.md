# Issue #174: Track and persist per-level Crowd Mastery (largest simultaneous Controlled crowd)

Adds a new "Crowd Mastery" stat: the largest number of enemies a player has ever had
simultaneously `Controlled` in a given level. A new world-scoped `UCrowdMasterySubsystem`
keeps a running max, sampled on ability-cast-applied and Controlled-state-expiry events,
and resets on `ULevelLifecycleSubsystem::OnLevelBegin`. The best value persists through
the existing `ULevelClearTimeSaveGame`/`ULevelClearTimeSubsystem` pair (no new save-game
class), mirroring the existing clear-time persistence path with the comparison flipped
to track a maximum instead of a minimum.

## Acceptance criteria

- [x] Sample simultaneous Controlled enemy counts on ability-cast-applied events and on
      Controlled-state expiry — `UCrowdMasterySubsystem::HandleAbilityCastApplied` /
      `HandleEnemyControlledExpired`, both calling `SampleControlledCount()`
      (`CrowdMasterySubsystem.cpp`).
- [x] Maintain a running max per level, event/call-driven (not tick-based) —
      `RunningMaxControlledCount` in `UCrowdMasterySubsystem`, a plain `UWorldSubsystem`.
- [x] Persist a "Crowd Mastery best" via the save-game system, extending the existing
      class rather than adding a new one — `BestCrowdMasteryByLevel` added to
      `ULevelClearTimeSaveGame`; `RecordCrowdMasteryCount`/`GetBestCrowdMasteryCount`
      added to `ULevelClearTimeSubsystem`, reusing the existing save slot and
      `LoadOrCreateSaveGame()` path.
- [x] Subscribe to `OnLevelBegin` to reset per-level state — real subscription wired in
      `UCrowdMasterySubsystem::Initialize()` to
      `ULevelLifecycleSubsystem::OnLevelBegin`.
- [x] Automation test coverage for tracking and reset behavior —
      `KrowdKontrolCrowdMasterySubsystemTest.cpp` (peak tracking across overlapping
      casts, peak survives expiry, true-peak accuracy, `OnLevelBegin` reset, delegate
      fires exactly once); extended `KrowdKontrolEnemyBaseTest.cpp` (new
      `OnEnemyControlledExpired` delegate assertions) and
      `KrowdKontrolLevelClearTimeSubsystemTest.cpp` (Crowd Mastery persistence cases).

## Scope notes

Foundation only, matching `ULevelClearTimeSubsystem`'s own precedent: this issue does
not wire `HandleAbilityCastApplied`/`HandleEnemyControlledExpired` to a live
`UAbilityCastComponent`/`AEnemyBase` instance (no per-instance delegate-binding registry
exists yet), does not call `RecordCrowdMasteryCount` automatically on level clear, and
does not display the stat anywhere. All three are explicitly out of this issue's AC and
match the still-open gap already documented for the sibling clear-time stat.

## Validation

`harness/ci.py` full gate: `GATE_OK` — `UNIT_PASSED tests=73` (includes the new
`KrowdKontrol.Unit.CrowdMasterySubsystem` test and the extended `EnemyBase` and
`LevelClearTimeSubsystem` tests), `UE_AUTOMATION_OK`, `E2E_PASSED steps=1`. Scope check
confirmed the diff touches only `app-source-tracked/Source/KrowdKontrol/**`, no
protected files. Hard invariants reviewed against the diff (no enemy kill path, no
colour/ability/enemy roster changes) — pass, no regressions.
