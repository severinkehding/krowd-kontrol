# Issue #21: Add UWaveSpawnerComponent (Reusable Wave Spawner)

Adds `UWaveSpawnerComponent`, a `UActorComponent` that spawns a caller-configured list
of enemy "waves" — each wave a `(EnemyType, EnemyClass, Count, DelaySeconds)` entry —
in sequence. It follows the exact shape already established by
`URoomEnemyBudgetController` (issue #82) and `UStationPowerUpComponent` (issue #60): a
placeable component with no opinion on *when* it's triggered, an idempotent
`StartWaves()` entry point for deterministic Automation-test driving, and a public
`TriggerNextWave()` entry point any caller can invoke to advance the sequence
immediately. The only genuinely new piece is time-based sequencing (`DelaySeconds` per
wave via `FTimerManager`), chained wave-to-wave — zero-delay waves resolve
synchronously within one call, nonzero-delay waves schedule a timer that
`TriggerNextWave()` can preempt at any time. The component never branches on
`EEnemyType` or references `ARoomActor`/`ABossBase` anywhere, so the same component
serves both a room's setup and a future boss encounter's setup.

## Files changed (all under `app/`, gitignored per D-003 — this is the tracked-repo
record of that change, see the closing note below)

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/WaveSpawnerComponent.h` | CREATE | `FWaveEntry` struct (`EnemyType`/`EnemyClass`/`Count`/`DelaySeconds`), `FOnWaveSpawned`/`FOnAllWavesComplete` dynamic delegates, `UWaveSpawnerComponent` declaration: `Waves`, public `StartWaves()`/`TriggerNextWave()`, `GetSpawnedActors()`/`GetNextWaveIndex()` accessors, private `SpawnWave()`/`ScheduleWave()` |
| `app/Source/KrowdKontrol/WaveSpawnerComponent.cpp` | CREATE | Sequencing implementation: idempotent `StartWaves()`, timer-or-synchronous `ScheduleWave()`, `SpawnWave()`'s warn-then-continue spawn guard, `TriggerNextWave()`'s clear-then-fire preemption, `EndPlay()` timer cleanup |
| `app/Source/KrowdKontrol/Private/Tests/WaveSpawnerTestListener.h`/`.cpp` | CREATE | Small `UObject` test helper — dynamic delegates require a `UFUNCTION`-bound `AddDynamic` target, not a lambda; captures both `OnWaveSpawned`'s `int32` param and `OnAllWavesComplete`'s call count |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolWaveSpawnerComponentTest.cpp` | CREATE | `KrowdKontrol.Unit.WaveSpawnerComponent` — covers all 4 acceptance criteria plus the two misconfiguration edge cases this codebase's other spawner components already established a precedent for |

**No `KrowdKontrol.Build.cs` change was required** — `TimerManager.h`/`FTimerManager` and
`EndPlay`/`EEndPlayReason` ship with the already-linked `Engine` module; confirmed by a
real `UnrealBuildTool` invocation succeeding with no new dependency added.

## Deviation from the plan

The plan's `GetSpawnedActors()` accessor was specified to return `const TArray<AActor*>&`
backed by a private `TArray<TObjectPtr<AActor>> SpawnedActors` member. Those are
distinct template instantiations in C++ — a `TArray<TObjectPtr<AActor>>` cannot bind to
a `TArray<AActor*>&` reference without a conversion, so this doesn't compile as
literally specified. Changed the accessor to return `const TArray<TObjectPtr<AActor>>&`
directly (matching the storage type) — same capability (callers/tests can still read
`.Num()` and each spawned actor), just a type that actually compiles.

## Acceptance criteria

- [x] **A spawner component (`UWaveSpawnerComponent`) exists that accepts a
      configurable list of `(EnemyType, EnemyClass, Count, DelaySeconds)` entries and
      spawns them as one or more sequenced waves.** `Waves` is an `EditDefaultsOnly`
      `TArray<FWaveEntry>`; `StartWaves()` begins the sequence.
- [x] **The spawner never branches on `EEnemyType` internally.** `SpawnWave()` only
      reads `Entry.EnemyClass`/`Entry.Count` — `EnemyType` is stored as a tag only,
      never read by any control-flow in `WaveSpawnerComponent.cpp`.
- [x] **The spawner is usable from both a room's setup and a boss encounter's setup.**
      No reference to `ARoomActor` or `ABossBase` anywhere in either new file.
- [x] **At least one `KrowdKontrol.Unit.*` Automation Framework test confirms a
      configured wave spawns the correct enemy count and types.** Test case (1) in
      `KrowdKontrolWaveSpawnerComponentTest.cpp` configures two zero-delay waves (2 +
      3 actors) and asserts `GetSpawnedActors().Num() == 5`.
- [x] **`python harness/ci.py` (full mode) passes with `GATE_OK`.** See Validation below.
- [x] **`app-source-tracked/` mirror is byte-identical to the real `app/` files
      (D-009).** Verified via `diff` on all 5 new files — all empty.
- [x] **`app-changelog/issue-21.md` documents the change per the established format.**
      This file.

## Validation

A real `UnrealBuildTool` compile was run explicitly (the harness's `run_ue_automation.sh`
launches `UnrealEditor-Cmd.exe` directly against the precompiled module DLL and does not
itself trigger a rebuild when source changes — confirmed by checking `UnrealEditor-
KrowdKontrol.dll`'s timestamp against the new source files before building) via:

```
$ Build.bat KrowdKontrolEditor Win64 Development -project=<KrowdKontrol.uproject> -waitmutex
...
[1/7] Compile [x64] WaveSpawnerTestListener.cpp
[2/7] Compile [x64] WaveSpawnerComponent.cpp
[3/7] Compile [x64] KrowdKontrolWaveSpawnerComponentTest.cpp
[4/7] Compile [x64] Module.KrowdKontrol.cpp
[5/7] Link [x64] UnrealEditor-KrowdKontrol.lib
[6/7] Link [x64] UnrealEditor-KrowdKontrol.dll
[7/7] WriteMetadata KrowdKontrolEditor.target [NoUba]
Result: Succeeded
```

Then the real full gate:

```
$ python harness/ci.py
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=30
APP_STARTED driver=cli
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
HOLDOUT_ABSENT no .factory/holdout/run.py - NOTHING above the independence line ran. Every check in this gate is one the builder could read and iterate against.
MUTATIONS_ABSENT no harness/mutations/run.py - this gate has never been shown to fail. A gate that has never failed is a gate nobody has tested.
GATE_OK mode=full
```

`UNIT_PASSED tests=30` covers the new `KrowdKontrol.Unit.WaveSpawnerComponent` test
(30 = the prior baseline of 29 plus this one) — no regression in any pre-existing test.

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change, per
D-009. Not a substitute for reading `app-source-tracked/` directly.
