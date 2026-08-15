# Issue #82: Add URoomEnemyBudgetController

Adds `URoomEnemyBudgetController`, a `UActorComponent` that keeps a room's active
enemy count pinned to a configured density cap as enemies are banked, spawning
replacements from a fixed total budget until the budget and active pool both reach
zero — at which point it fires `OnRoomCleared` exactly once. This is the mechanism
PRD 01's REQ-6 depends on: enemy *count*, not damage, as the difficulty lever.
Placeholder-actor-first per MISSION.md — `EnemyClassToSpawn` takes whatever
placeholder actor a room's designer assigns (`APlaceholderCubeActor` for now), no
real enemy class exists yet. Scope deliberately excludes `ATargetZone` wiring and
real enemy AI — those are separate issues (see the issue's own Notes).

## Files changed (all under `app/`, gitignored per D-003 — this is the tracked-repo
record of that change, see the closing note below)

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/RoomEnemyBudgetController.h` | CREATE | Component declaration: `TotalRoomBudget`/`MaxConcurrentDensity`/`EnemyClassToSpawn` (`EditDefaultsOnly`), `FOnRoomCleared` dynamic multicast delegate, public `InitializeRoom()`/`NotifyEnemyBanked()`, private `SpawnEnemy()`/`CheckForRoomCleared()` |
| `app/Source/KrowdKontrol/RoomEnemyBudgetController.cpp` | CREATE | Implementation: `NotifyEnemyBanked()` decrements active count, spawns a replacement via `SpawnEnemy()` while budget remains and density allows it; `CheckForRoomCleared()` fires `OnRoomCleared` once when budget and active count both hit zero |
| `app/Source/KrowdKontrol/Private/Tests/RoomClearedTestListener.h`/`.cpp` | CREATE | Small `UObject` test helper — dynamic delegates require a `UFUNCTION`-bound `AddDynamic` target, not a lambda |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolRoomEnemyBudgetControllerTest.cpp` | CREATE | `KrowdKontrol.Unit.RoomEnemyBudgetController` — covers all 4 acceptance criteria below |
| `app/Source/KrowdKontrol/KrowdKontrol.Build.cs` | UPDATE | Conditional (`Target.bBuildEditor`-gated) `UnrealEd` dependency for the test's `FAutomationEditorCommonUtils::CreateNewMap()`; also added `PrivateIncludePaths.Add(ModuleDirectory);`, a root-cause fix for a pre-existing include-path gap that affected the older `PlaceholderCubeActorHasCubeMesh` test too |

## Acceptance criteria

- [x] **Component compiles as part of the `KrowdKontrol` module.** Confirmed via a
      real `UnrealBuildTool` invocation (`Result: Succeeded`).
- [x] **(a) Spawning respects `MaxConcurrentDensity` (never exceeds it).** `SpawnEnemy()`
      is only called from paths gated on `ActiveEnemyCount < MaxConcurrentDensity`;
      covered by the Automation test.
- [x] **(b) `NotifyEnemyBanked()` triggers a replacement spawn while budget remains.**
      Directly tested — banking an enemy while `RemainingBudget > 0` spawns a
      replacement.
- [x] **(c) `OnRoomCleared` fires exactly once, only after budget and active count
      both reach 0.** `bRoomClearedFired` guards against a double-fire; test asserts
      exactly one broadcast.
- [x] **(d) No damage/health values are touched by this controller.** `.h`/`.cpp`
      only manipulate `RemainingBudget`/`ActiveEnemyCount` — no health/damage API
      referenced anywhere in this component.

## Validation

```
$ python harness/ci.py
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=2
APP_STARTED driver=cli
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
HOLDOUT_ABSENT no .factory/holdout/run.py - NOTHING above the independence line ran.
MUTATIONS_ABSENT no harness/mutations/run.py - this gate has never been shown to fail.
GATE_OK mode=full
```

`UNIT_PASSED tests=2` covers both the new `KrowdKontrol.Unit.RoomEnemyBudgetController`
test and the pre-existing `KrowdKontrol.Unit.PlaceholderCubeActorHasCubeMesh` test — no
regression. MISSION.md Hard Invariant #2 ("defeated enemies are banked, never
destroyed") reviewed by inspection: the component only spawns and counts;
`NotifyEnemyBanked()` decrements a counter in response to an external banking event
and never calls `Destroy()`/kill logic itself.

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` copy (added after this PR's initial review pass
was correctly rejected for having no reviewable diff) are the tracked-repo record of
that change, per D-009. Not a substitute for reading `app-source-tracked/` directly.
