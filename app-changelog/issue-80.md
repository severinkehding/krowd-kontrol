# Issue #80: Add ATargetZone actor that banks controlled actors on colour-matched arrival

Adds `ATargetZone`, an `AActor` subclass that detects a controlled, colour-matched
`IHerdable` (issue #79) actor physically arriving in a zone and broadcasts a public
`OnActorBanked(AActor*)` delegate. This is the spatial "Bank" trigger from PRD 01 loop
step 5 / REQ-2. Detection and announcement only: the class never destroys, pools, or
otherwise mutates the overlapping actor, and does not wire itself to
`URoomEnemyBudgetController` — that integration is explicitly out of scope per the
issue's Notes section.

## Files changed (all under `app/`, gitignored per D-003 — this is the tracked-repo
record of that change, see the closing note below)

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/TargetZone.h` | CREATE | `ATargetZone` declaration: `UBoxComponent` root (`ZoneCollisionComponent`), `EditDefaultsOnly FName ZoneColourTag`, `BlueprintAssignable FOnActorBanked OnActorBanked` delegate, private `HandleZoneOverlap` UFUNCTION. |
| `app/Source/KrowdKontrol/TargetZone.cpp` | CREATE | Constructor sets up the box collision (`OverlapAllDynamic` profile, overlap events enabled) and binds `HandleZoneOverlap` in the constructor (not `BeginPlay()`, so it fires under Automation Framework tests that never call `World->BeginPlay()`). `HandleZoneOverlap` casts the overlapping actor to `IHerdable`, checks `IsControlled()` and `GetHerdColourTag() == ZoneColourTag`, and broadcasts `OnActorBanked` only when both hold. No destruction or mutation of the overlapping actor. |
| `app/Source/KrowdKontrol/Private/Tests/TargetZoneTestActor.h`/`.cpp` | CREATE | Minimal test-only `AActor` implementing `IHerdable`, with `SetControlled()`/`SetHerdColourTag()` setters and a collision component so it can physically overlap a spawned `ATargetZone`. |
| `app/Source/KrowdKontrol/Private/Tests/TargetZoneBankedTestListener.h`/`.cpp` | CREATE | Test-only `UObject` that binds to `OnActorBanked` and records whether/how many times it fired, so the test can assert on delegate invocation. |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolTargetZoneTest.cpp` | CREATE | `KrowdKontrol.Unit.TargetZone` — spawns an `ATargetZone` and a `TargetZoneTestActor`, moves the test actor into overlap via a real physical sweep, and asserts: (a) `OnActorBanked` fires when controlled + colour tag matches; (b) it does not fire on colour mismatch; (c) it does not fire when uncontrolled. See the inline comment at the top of this file for why the test spins up world play state (`InitializeActorsForPlay` + `SetBegunPlay(true)`) up front — required for real `OnComponentBeginOverlap` dispatch in the bare Automation Framework editor world, discovered via engine source reading, documented in `implementation.md`. |

No `.Build.cs` change needed — `Components/BoxComponent.h` and the existing `IHerdable`
dependency are already covered by the module's existing dependencies.

## Acceptance criteria

- [x] **`ATargetZone` compiles, placeable in a level.** Confirmed via `harness/ci.py`
      full-mode gate (`UE_BUILD_OK`, module builds as part of `UE_AUTOMATION_OK`).
- [x] **`KrowdKontrol.Unit.TargetZone` Automation Framework test spawns an
      `ATargetZone` and a test-only `IHerdable`-implementing actor, moves the test
      actor into overlap, and confirms all three cases** — (a) fires when controlled
      + colour match, (b) does not fire on colour mismatch, (c) does not fire when
      uncontrolled. All three exercised via the same real physical sweep.
- [x] **No destruction or mutation of the banked actor**, and no partial credit on
      colour mismatch. Confirmed by inspection of `TargetZone.cpp` (37 lines).

## Validation

```
$ python harness/ci.py
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=42
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

`UE_AUTOMATION_RESULT passed=1 total=1` covers the new `KrowdKontrol.Unit.TargetZone`
test; full 42/42 `KrowdKontrol.Unit.*` suite passed, no regressions. MISSION.md hard
invariants reviewed in `validation.md`: no governance files touched, no kill logic, no
6th colour introduced, no ability or enemy type added, no engine/dimensionality/
networking change, `app/` remains untracked in git (only the `.h`/`.cpp` mirror under
`app-source-tracked/` per the D-009 exception).

---

Source lives under `app/` (gitignored, D-003) — this file is the tracked-repo record
of that change, not a substitute for reading the actual code.
