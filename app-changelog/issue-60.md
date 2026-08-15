# Issue #60: Add UStationPowerUpComponent

Adds `UStationPowerUpComponent`, a `UActorComponent` that holds an ordered list of
level-placed light actors and reveals them one at a time as an external caller
notifies it that a player-triggered stage occurred — never via a Sequencer/Matinee
cutscene, camera lock, or forced pan, and never touching any player-input API. This
implements PRD 07 REQ-1's "interactive from frame one" requirement for the station
power-up beat: the player's own actions, not a cutscene, drive each light turning on.
Mirrors `URoomEnemyBudgetController`'s established "component never wires its own
trigger/overlap plumbing — callers invoke a public notify method directly" pattern
(issue #82). Scope is the reusable component and its Automation Framework test only —
the authored Opening Scene level layout (placing real lights, wiring a real trigger
volume) is level-design content and explicitly out of scope, per the issue's own
Notes.

## Files changed (all under `app/`, gitignored per D-003 — this is the tracked-repo
record of that change, see the closing note below)

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/StationPowerUpComponent.h` | CREATE | Component declaration: `OrderedLights` (`EditInstanceOnly` — level-actor references, not Blueprint defaults), `FOnLightEnabled`/`FOnPowerUpSequenceComplete` dynamic multicast delegates, public `InitializeSequence()`/`NotifyPowerUpStageTriggered()`, read-only accessors `GetEnabledLightCount()`/`IsSequenceComplete()` |
| `app/Source/KrowdKontrol/StationPowerUpComponent.cpp` | CREATE | Implementation: `InitializeSequence()` idempotently hides every configured light (warns if `OrderedLights` is empty); `NotifyPowerUpStageTriggered()` reveals the next light in order, broadcasts `OnLightEnabled`, and fires `OnPowerUpSequenceComplete` exactly once after the last light |
| `app/Source/KrowdKontrol/Private/Tests/StationPowerUpTestListener.h`/`.cpp` | CREATE | Small `UObject` test helper — dynamic delegates require a `UFUNCTION`-bound `AddDynamic` target, not a lambda |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolStationPowerUpComponentTest.cpp` | CREATE | `KrowdKontrol.Unit.StationPowerUpComponent` — covers all 4 acceptance criteria below |

No `app/Source/KrowdKontrol/KrowdKontrol.Build.cs` change was needed — `Core`/`CoreUObject`/`Engine`
already cover `AActor`, and the existing conditional `UnrealEd` dependency already covers
the test's `FAutomationEditorCommonUtils::CreateNewMap()`.

## Acceptance criteria

- [x] **`UStationPowerUpComponent` holds an ordered list of light actor references
      (`OrderedLights`) and enables them progressively via a public
      `NotifyPowerUpStageTriggered()` entry point** — not a timeline/Sequencer-driven
      cutscene. Covered by the Automation test's stage-by-stage assertions.
- [x] **The component never calls `DisableInput`, `SetInputMode`, or sets
      `bBlockInput`/similar anywhere in `StationPowerUpComponent.h`/`.cpp`** —
      confirmed by inspection: no such API is referenced anywhere in either file. The
      test demonstrates this concretely: a player-input stand-in actor (no Pawn class
      exists in this codebase yet) stays `InputEnabled()` across every stage of a full
      sequence run.
- [x] **No `LevelSequence`/`Matinee`/camera-lock/forced-pan API appears anywhere in the
      new files** — confirmed by inspection: no such API is referenced anywhere in
      either file.
- [x] **`KrowdKontrol.Unit.StationPowerUpComponent` passes**, confirming: player input
      stand-in remains enabled throughout; lights enable strictly in order in response
      to `NotifyPowerUpStageTriggered()` calls; the completion delegate fires exactly
      once; further triggers past completion are no-ops; an empty `OrderedLights` warns
      (not crashes) and never fires completion.

## Validation

```
$ python harness/ci.py
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=9
APP_STARTED driver=cli
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
HOLDOUT_ABSENT no .factory/holdout/run.py - NOTHING above the independence line ran.
MUTATIONS_ABSENT no harness/mutations/run.py - this gate has never been shown to fail.
GATE_OK mode=full
```

`UNIT_PASSED tests=9` covers the new `KrowdKontrol.Unit.StationPowerUpComponent` test
alongside every pre-existing `KrowdKontrol.Unit.*`/`KrowdKontrol.Smoke.*` test — no
regression.

---

Source lives under `app/` (gitignored, D-003) — this file is the tracked-repo record
of that change, not a substitute for reading the actual code.
