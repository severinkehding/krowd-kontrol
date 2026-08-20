# Issue #179: Punishment 2 — run-speed reduction on contact damage

Adds `USpeedReductionPunishmentComponent`, a new `UActorComponent` that binds to
`UPunishmentManagerComponent::OnPunishmentTriggered` (issue #177, merged PR #182) and
reduces the owning pawn's `UFloatingPawnMovement::MaxSpeed` by a configurable factor for
a fixed duration, then restores it. It is wired as a sibling component in both prototype
pawns' constructors (`AFlatCamera3DPrototypePawn`, `APaper2DPrototypePawn`), following
the exact idiom `PunishmentManagerComponent` itself established one issue ago:
`CreateDefaultSubobject`, explicit reference wiring at the construction call site,
`AddDynamic` binding. Re-triggering while already active refreshes the duration without
re-deriving/re-applying the reduction factor, so the speed cannot compound below the
intended floor. This is PRD "Punishment System (Punishments 1 & 2 + arbitration)" REQ-3
(P0). Single-active-punishment arbitration (REQ-4) is explicitly out of scope — this
punishment activates independently on every trigger.

## Files changed

The real Unreal project lives under `app/` (gitignored symlink, D-003) and is what the
harness actually builds/tests against — `app/` itself is unchanged by this PR's
tracking. What this PR's diff actually contains is a **copy** of the new/changed
source, per D-009, at `app-source-tracked/<same path under app/Source/>`.

| File (under `app-source-tracked/Source/KrowdKontrol/`) | Action | What it contains |
|------|--------|-------------------|
| `SpeedReductionPunishmentComponent.h` | CREATE | `USpeedReductionPunishmentComponent : public UActorComponent` — `SpeedMultiplierWhileActive`/`SpeedReductionDurationSeconds` tunables, `MovementComponent` runtime-wiring pointer, `HandlePunishmentTriggered()` UFUNCTION, public `IsSpeedReductionTimerActive()` test-support accessor (mirrors `WaveSpawnerComponent::IsWaveTimerActive()`), private `RestoreOriginalSpeed`/`FTimerHandle`/`OriginalMaxSpeed`, test friend-grant |
| `SpeedReductionPunishmentComponent.cpp` | CREATE | Constructor (tick disabled), `HandlePunishmentTriggered` (IsTimerActive-guarded capture+reduce, unconditional SetTimer refresh, `UE_LOG` warning if `MovementComponent` is unwired), `RestoreOriginalSpeed` (`UE_LOG` warning on the same unwired case), `EndPlay` (clear timer), `IsSpeedReductionTimerActive` |
| `FlatCamera3DPrototypePawn.h` / `.cpp` | UPDATE | Forward-declares/constructs `SpeedReductionPunishmentComponent`, wires it to this pawn's own `MovementComponent`, binds it to `PunishmentManagerComponent->OnPunishmentTriggered` via `AddDynamic` |
| `Paper2DPrototypePawn.h` / `.cpp` | UPDATE | Same wiring as above, for the second prototype pawn |
| `Private/Tests/KrowdKontrolSpeedReductionPunishmentComponentTest.cpp` | CREATE | `KrowdKontrol.Unit.SpeedReductionPunishmentComponent` — activation applies factor, re-trigger while active doesn't compound, expiry restores original `MaxSpeed`, sanity that `OriginalMaxSpeed` wasn't re-captured from an already-reduced value, (d) `EndPlay()` clears a pending restore timer (`DispatchBeginPlay()` + `IsSpeedReductionTimerActive()`, mirroring `WaveSpawnerComponent`'s case (7) and its documented `check(bHasBegunPlay)` trap), (e) an unwired `MovementComponent` no-ops rather than crashes |
| `Private/Tests/KrowdKontrolFlatCamera3DPipelineSmokeTest.cpp` | UPDATE | Extends the existing pawn-spawn smoke test with a pawn-level wiring assertion: this pawn's own real `ApplyContactDamage` call reduces this same pawn's own `MovementComponent->MaxSpeed` |
| `Private/Tests/KrowdKontrolPaper2DPipelineSmokeTest.cpp` | UPDATE | Same, second pawn |

## Acceptance criteria

- [x] **On `OnPunishmentTriggered` firing, the player pawn's movement speed is reduced by
      a configurable factor (default 50%) for a fixed duration, then restored.** See
      `SpeedReductionPunishmentComponent.cpp`'s `HandlePunishmentTriggered`/
      `RestoreOriginalSpeed`.
- [x] **Both `AFlatCamera3DPrototypePawn` and `APaper2DPrototypePawn` get the effect via
      the same shared `USpeedReductionPunishmentComponent`**, with no pawn-specific
      movement-modifier logic duplicated between them. Verified in both pawns' `.cpp`
      diffs and both smoke tests.
- [x] **Re-triggering while already active refreshes the duration at the same reduced
      speed rather than compounding the factor.** `HandlePunishmentTriggered`'s
      `IsTimerActive` guard; covered by `KrowdKontrol.Unit.SpeedReductionPunishmentComponent`
      case (c).
- [x] **Automation tests cover (a) factor applies on activation, (b) speed restores on
      expiry, (c) re-triggering while active does not stack/compound, (d) `EndPlay()`
      clears a pending restore timer, (e) an unwired `MovementComponent` no-ops.** All
      five in `KrowdKontrolSpeedReductionPunishmentComponentTest.cpp`.
- [x] **Level 1-3 validation passes.** `harness/ci.py` full mode: `UNIT_PASSED tests=70`,
      `UE_BUILD_OK`, `UE_AUTOMATION_RESULT passed=1 total=1`, `GATE_OK`.
- [x] **Code mirrors existing patterns** — `AbilityCastVFXComponent`'s
      `FTimerHandle`/`SetTimer`/`ClearTimer`/`EndPlay` idiom, `WaveSpawnerComponent`'s
      `IsTimerActive` guard idiom and `IsWaveTimerActive()`-accessor test-support idiom,
      `PunishmentManagerComponent`'s sibling-wiring idiom, and both sibling components'
      `UE_LOG(LogTemp, Warning, ...)`-on-unexpected-null-wiring idiom.
- [x] **No regressions in the existing automation suite.** Post-review-fix validation hit
      one transient, unrelated failure (`KrowdKontrol.Unit.OvercrowdDetectionComponent`,
      untouched by this PR) that cleared on immediate rerun with no code changes —
      consistent with the pre-existing suite flakiness already noted below from the
      original pass (`KrowdKontrol.Unit.EnemyBase`, `KrowdKontrol.Unit.LevelLifecycleSubsystem`).
- [x] **`app/` and `app-source-tracked/` copies of every changed/new file are identical**
      — confirmed via `diff` (no output) for all 9 touched files.

## Review follow-up

Self-fix pass addressing `consolidated-review.md` (PR #196 review artifacts):

- **HIGH** — `EndPlay()`'s timer-clear had no regression test. Added a public
  `IsSpeedReductionTimerActive()` accessor (mirrors `WaveSpawnerComponent::IsWaveTimerActive()`)
  and test case (d), using `DispatchBeginPlay()` per the documented `check(bHasBegunPlay)`
  trap this codebase already hit once with `WaveSpawnerComponent`'s own case (7).
- **MEDIUM** — Both `MovementComponent`-null guards (`HandlePunishmentTriggered`,
  `RestoreOriginalSpeed`) were silent no-ops with no logging, unlike the sibling
  components this PR mirrors. Added `UE_LOG(LogTemp, Warning, ...)` to both, matching
  `WaveSpawnerComponent`'s unset-`EnemyClass` precedent.
- **LOW** — `HandlePunishmentTriggered()`'s null-`MovementComponent` early return was
  untested. Added test case (e): an unwired component's trigger call completes without
  crashing.
- Fixing case (e) surfaced a real test-authoring bug caught during validation, not part
  of the original review findings: it reused the test's original `World` pointer, but
  case (d) had already called `FAutomationEditorCommonUtils::CreateNewMap()` again,
  replacing the editor's current map and invalidating that pointer — reusing it hit an
  engine-side `Assertion failed: CurrentLevel` (`LevelActor.cpp`) in `SpawnActor`. Fixed
  by giving case (e) its own fresh `CreateNewMap()`, same as case (d).

## Validation evidence

`python harness/ci.py` (full mode, real headless Unreal Editor rebuild + Automation
Framework run):

```
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=70
APP_STARTED driver=cli
UE_BUILD_START KrowdKontrolEditor Win64 Development
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
HOLDOUT_ABSENT no .factory/holdout/run.py
MUTATIONS_ABSENT no harness/mutations/run.py
GATE_OK mode=full
```

Green after one rerun — the intervening run hit `KrowdKontrol.Unit.EnemyBase` /
`KrowdKontrol.Unit.LevelLifecycleSubsystem` failures, in neither of which this PR touches
any code; both passed clean on immediate rerun with zero changes in between, confirming
pre-existing suite flakiness rather than a regression from this change. Hard invariants
(MISSION.md #1-#8) reviewed directly against the diffs, all intact — no new third-party
dependency, no `AbilityData` colour bypass, no direct energy mutation outside
`ApplyContactDamage`.
