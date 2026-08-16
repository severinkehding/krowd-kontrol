# Issue #16: Implement Overcrowd Trigger Detection ("Panic Overload" State)

Adds the detection/state-machine half of PRD 08 Punishment 3 ("Overcrowd",
MISSION.md `08`) — a new `UOvercrowdDetectionComponent`, attached to the player pawn
(same placement convention as `UPlayerEnergyComponent`), that counts how many hot but
uncontrolled enemies (`AEnemyBase::GetEnemyState() == Alert || Attack`, explicitly
*not* `Controlled`) sit within a tunable radius of the player, and flips a new
`EPanicOverloadState` from `Inactive` to `Active` (firing `OnPanicOverloadStateChanged`
exactly once) once that count has held at/above a tunable threshold for a tunable
continuous duration. No damage, audio, visual, or recovery (`Active` → `Inactive`)
logic is included — both are explicitly deferred to later issues per the issue body.

## Files changed (all under `app/`, gitignored per D-003 — this is the tracked-repo
record of that change, see the closing note below)

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/EnemyBase.h` | UPDATE | One new `friend class FKrowdKontrolOvercrowdDetectionComponentTest;` line, alongside the 4 existing friend grants, so the new test can drive enemies into `Alert`/`Attack` via the private `TickCheckDetection()` (an unrelated `IThreatState`/`GetThreatState()` implementation and a `FKrowdKontrolMusicSubsystemTest` friend grant briefly leaked in from concurrent, unmerged issue #25 work sharing the same `app/` tree — caught in review and stripped back out; see Validation below) |
| `app/Source/KrowdKontrol/OvercrowdDetectionComponent.h` | CREATE | `EPanicOverloadState` enum, `FOnPanicOverloadStateChanged` delegate, `UOvercrowdDetectionComponent` declaration: tunable `OvercrowdCrowdThreshold`/`OvercrowdRadiusUnits`/`OvercrowdUncontrolledDurationSeconds`, `GetPanicOverloadState()`, private `AdvancePanicOverloadState()`/`CountHotUncontrolledEnemiesNearby()` |
| `app/Source/KrowdKontrol/OvercrowdDetectionComponent.cpp` | CREATE | Tick-driven accumulator (mirrors `UAbilityCooldownComponent`) and `TActorIterator<AEnemyBase>` aggregation (mirrors `UMusicSubsystem::IsAnyEnemyInCombat()`), using `GetEnemyState()` (never `GetThreatState()`, which reports `Controlled` as `Hot` too) |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolOvercrowdDetectionComponentTest.cpp` | CREATE | `KrowdKontrol.Unit.OvercrowdDetectionComponent` — cases (a)-(i) below |
| `app/Source/KrowdKontrol/Private/Tests/PanicOverloadStateTestListener.h`/`.cpp` | CREATE | Small `UObject` test helper — `FOnPanicOverloadStateChanged` is a dynamic multicast delegate, which only binds `UFUNCTION`s via `AddDynamic`, never a capturing lambda (mirrors `UMusicStateTestListener`/`UEnemyBankedTestListener`) |

**No `KrowdKontrol.Build.cs` change was required** — `TActorIterator`/`EngineUtils.h`
ship with the already-linked `Engine` module.

## Deviation from the plan

The plan's test case (h) (radius exclusion) assumed `AActor::SetActorLocation()` would
work directly on an `AEnemyBaseTestActor`. `AEnemyBase` carries no `RootComponent` by
design (no visual representation yet), and `SetActorLocation()` is a silent no-op when
`RootComponent` is null (confirmed against UE 5.8's own `Actor.cpp`) — the originally
planned "move one enemy far away" step did nothing, so the test initially failed. Fixed
by giving that one test-local enemy instance its own minimal `USceneComponent` root
(`NewObject` + `RegisterComponent` + `SetRootComponent`), scoped entirely to the test
file — no change to `AEnemyBase`/`AEnemyBaseTestActor` production or shared test code.

## Acceptance criteria

- [x] **A system tracks, per tick or on-demand, how many hot-and-uncontrolled enemies
      are within a tunable radius of the player, excluding `Controlled` enemies.**
      `CountHotUncontrolledEnemiesNearby()` reads `GetEnemyState() == Alert || Attack`
      only. Test case (i) proves `Controlled` enemies never count — the issue's core
      AC.
- [x] **Sustaining that count at/above the threshold for the configured duration flips
      the state to `Active` and fires a subscribable delegate, plus a plain getter.**
      `AdvancePanicOverloadState()` + `OnPanicOverloadStateChanged` +
      `GetPanicOverloadState()`. Test cases (d)/(e)/(f) cover count-met-but-duration-not,
      the flip itself (exactly one broadcast), and no re-broadcast on a no-op refresh.
- [x] **No direct damage penalty.** `OvercrowdDetectionComponent.cpp` has zero
      references to `PlayerEnergyComponent`/`ApplyContactDamage`.
- [x] **Tunable values are named `UPROPERTY(EditDefaultsOnly, ...)` fields with
      `ClampMin` meta, no inline magic numbers.** `OvercrowdCrowdThreshold`,
      `OvercrowdRadiusUnits`, `OvercrowdUncontrolledDurationSeconds`.
- [x] **`KrowdKontrol.Unit.OvercrowdDetectionComponent` Automation Framework test
      exists and asserts `Active` under a simulated converging-crowd scenario.** Cases
      (a)-(i): default state, below-threshold count, count-met/duration-pending, the
      flip + single broadcast, no-op no-rebroadcast, count-drop timer reset, radius
      exclusion, and controlled-exclusion. Two more added during review: an inclusive
      radius-boundary case (an enemy exactly at `OvercrowdRadiusUnits` still counts) and
      a no-owning-Actor case (`CountHotUncontrolledEnemiesNearby()`'s `GetOwner()` null
      guard doesn't crash and leaves the component `Inactive`).
- [ ] **`python harness/ci.py` (full mode) passes with `GATE_OK`.** Does not currently
      pass — see Validation below. Root cause is unrelated to this diff.
- [x] **`app-source-tracked/` mirror is byte-identical to the real `app/` files
      (D-009).**
- [x] **`app-changelog/issue-16.md` documents the change per the established format.**
      This file.

## Validation

`python harness/ci.py` (full mode) currently reports `GATE_FAILED: unit`, but for a
reason entirely unrelated to this diff: `KrowdKontrolWaveSpawnerComponentTest.cpp`
(added by already-merged PR #123 / commit `7301871`, not touched by this branch) case
(7) calls `UWaveSpawnerComponent::EndPlay()` without driving `BeginPlay()` first, which
trips `UActorComponent::EndPlay()`'s unconditional `bHasBegunPlay` assert and crashes
the whole Automation Framework process rather than failing that one test. Confirmed
pre-existing and unrelated by running `KrowdKontrol.Unit.WaveSpawnerComponent` in
complete isolation (same crash, zero interaction with this issue's diff). This means
`harness/ci.py`'s `unit` rung cannot report `GATE_OK` for *any* PR against this repo
right now, not just this one — recommend a follow-up issue to fix that test case.

Substitute verification performed instead (this diff's own correctness), re-run after
review caught and this PR fixed the leaked `IThreatState`/`FKrowdKontrolMusicSubsystemTest`
content described above:

```
$ harness/run_ue_automation.sh KrowdKontrol.Unit.OvercrowdDetectionComponent
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK

$ harness/run_ue_automation.sh "KrowdKontrol.Unit.EnemyBase+KrowdKontrol.Unit.SniperEnemy+KrowdKontrol.Unit.BomberEnemy+KrowdKontrol.Unit.ThreatState"
UE_AUTOMATION_RESULT passed=4 total=4
UE_AUTOMATION_OK

# All KrowdKontrol.Unit.* tests joined via '+', excluding only WaveSpawnerComponent:
UE_AUTOMATION_RESULT passed=30 total=30
UE_AUTOMATION_OK
```

30/30 passed, including `EnemyBase`, `SniperEnemy`, `BomberEnemy`, and `ThreatState`
(the tests most exposed to `EnemyBase.h`, including the removal of its stray
`IThreatState` inheritance) — no regressions from this diff. `app-source-tracked/` was
also re-diffed against the real `app/` files for every changed `.h`/`.cpp` and
confirmed byte-identical (D-009) after the fix. A human (or the workflow's
deterministic infra-backstop) should decide whether to route this PR to
`factory:needs-human` given the gate can't produce a clean `GATE_OK` right now for
reasons entirely outside this diff.

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change, per
D-009. Not a substitute for reading `app-source-tracked/` directly.
