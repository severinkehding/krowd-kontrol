# Issue #170: Wire level clear-time tracking to level-begin/level-clear signals

`ULevelClearTimeSubsystem::StartLevelTimer`/`StopLevelTimerAndRecordClear` were fully
implemented and tested in isolation but had no caller — a level could be played and
cleared and no clear time was ever recorded. This issue wires them to
`ULevelLifecycleSubsystem`'s existing `OnLevelBegin`/`OnLevelClear` delegates (from
issue #169) via a new `ULevelClearTimeSubsystem::SubscribeToLevelLifecycle()` method,
called once per level from `AKrowdKontrolPlayerController::BeginPlay()` alongside the
sibling issue #171 wiring already in that method.

## Files changed

| File | Action | Contains |
|------|--------|----------|
| `LevelClearTimeSubsystem.h`/`.cpp` | UPDATE | New `SubscribeToLevelLifecycle(ULevelLifecycleSubsystem*)`, `HandleLevelBegin(FName)`/`HandleLevelClear()` handlers, `CurrentLevelID` member |
| `KrowdKontrolPlayerController.cpp` | UPDATE | `BeginPlay()` resolves this level's `ULevelLifecycleSubsystem` and calls `SubscribeToLevelLifecycle()` on the resolved `ULevelClearTimeSubsystem` |
| `EnemyBase.h` | UPDATE | Added `friend class FKrowdKontrolLevelClearTimeWiringTest;` (needed to drive the level-clear precondition deterministically in the new test, mirroring the existing lifecycle-test friend grant) |
| `Private/Tests/KrowdKontrolLevelClearTimeWiringTest.cpp` | CREATE | `KrowdKontrol.Unit.LevelClearTimeWiring` — end-to-end test proving `OnLevelBegin` starts the timer and `OnLevelClear` records/persists the best, through the real delegates |
| `Private/Tests/KrowdKontrolLevelFailedTest.cpp` | UPDATE | Reordered subsystem injection before `DispatchBeginPlay()` — `BeginPlay()` now resolves the subsystem itself, so injecting after left it briefly unresolved and doubled the "no subsystem" warning count the test asserts on |

## Acceptance criteria

- [x] `ULevelClearTimeSubsystem` subscribes to `ULevelLifecycleSubsystem`'s
      `OnLevelBegin`/`OnLevelClear` delegates: `LevelClearTimeSubsystem.cpp`,
      `SubscribeToLevelLifecycle()` (`AddUniqueDynamic` binding of both handlers).
- [x] On `OnLevelBegin(mapName)`, `StartLevelTimer` is called for the current map:
      `HandleLevelBegin()` stores `MapName` in `CurrentLevelID` and calls
      `StartLevelTimer(MapName)`.
- [x] On `OnLevelClear`, `StopLevelTimerAndRecordClear` is called, recording the clear
      and persisting the personal best: `HandleLevelClear()` calls
      `StopLevelTimerAndRecordClear(CurrentLevelID)`.
- [x] Automation test asserts both halves: `KrowdKontrolLevelClearTimeWiringTest.cpp`
      drives `OnWorldBeginPlay`/`RefreshLevelClearState()` through a real
      `ULevelLifecycleSubsystem` and asserts `GetBestClearTimeSeconds` is false before
      and true (non-negative) after — which can only pass if both handlers actually
      fired via the real delegate wiring, not a direct method call.
- [ ] **PR body real-PIE-playthrough evidence — not completed, see below.**
- [x] `python harness/ci.py` (full mode) passes: `GATE_OK mode=full`, `UNIT_PASSED
      tests=75`.
- [x] No hard invariant violated — pure signal-plumbing plus one additive test-only
      friend declaration; see `validation.md` Phase 3.

## Manual PIE verification — outstanding

The issue requires the PR body to describe a real PIE playthrough that was manually
verified to produce a save file in `Saved/SaveGames/` with a recorded clear time. This
step needs the Unreal Editor open on the Windows host with the MCP server started
(`ModelContextProtocol.StartServer`, per the `unreal-agent-harness` skill — "the one
remaining step, and it's manual on purpose"). At PR-creation time in this session,
`http://127.0.0.1:8000/mcp` refused the connection (`http_code=000`), meaning no Editor
instance was open/serving MCP to drive this from — so it could not be performed here.
**This is flagged explicitly in the PR body as an open item for the human reviewer to
complete before merge**, rather than being fabricated or silently skipped.

## Validation evidence

See `validation.md` in the run artifacts: `harness/ci.py` full mode passed clean on the
first run (`UNIT_PASSED tests=75`, `UE_BUILD_OK`, `UE_AUTOMATION_RESULT passed=1
total=1`, `E2E_PASSED steps=1`, `GATE_OK mode=full`), no fixes required.
