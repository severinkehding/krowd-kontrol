# Issue #62: Add an optional environmental-storytelling terminal-log actor for the Drain reveal

Adds `APlaceholderTerminalActor` (PRD 07 REQ-4): a single reusable, placeholder
interactable "Terminal" that reveals a short piece of foreshadowing log text exactly
once when `Interact()` is called, and never gates any level-critical-path logic. It
carries its own `FGizmoBark`-shaped content (`TerminalLog`) and its own
`FOnBarkTriggered`-shaped delegate (`OnTerminalLogRevealed`), reusing the narrative
system's (issue #57/PR #98) data shape and delegate signature without routing through
`UGizmoNarrativeSubsystem` itself — `GetGameInstance()` is null in this project's
`CreateNewMap()`-based Automation Framework test worlds, which would make that route
silently un-testable (verified against UE 5.8 engine source; see
`investigation.md`'s "Approach Chosen" for the full account). No concrete interaction
trigger (overlap volume, input action), no real Drain-foreshadowing copy, and no
HUD/widget rendering of the revealed text — all explicitly out of this issue's scope.

## Files changed

The real Unreal project lives under `app/` (gitignored symlink, D-003) and is what
the harness actually builds/tests against — `app/` itself is unchanged by this PR's
tracking. What this PR's diff actually contains is a **copy** of the new source, per
D-009, at `app-source-tracked/<same path under app/Source/>` — that's what's listed
below and what a reviewer is actually looking at in this diff.

| File (under `app-source-tracked/Source/KrowdKontrol/`) | Action | What it contains |
|------|--------|-------------------|
| `PlaceholderTerminalActor.h` | CREATE | `APlaceholderTerminalActor : public AActor` declaration — `MeshComponent`, `TerminalLog` (`FGizmoBark`), `OnTerminalLogRevealed` (`FOnBarkTriggered`), `Interact()`, inline `HasBeenInteracted()` |
| `PlaceholderTerminalActor.cpp` | CREATE | Constructor (mirrors `APlaceholderCubeActor`/`APlaceholderTargetZoneActor`: cylinder mesh, root component, tick disabled) + `Interact()`'s flip-before-broadcast no-replay logic (mirrors `UGizmoNarrativeSubsystem::TriggerBark`) |
| `Private/Tests/KrowdKontrolPlaceholderTerminalActorTest.cpp` | CREATE | `KrowdKontrol.Unit.PlaceholderTerminalActor` — covers all acceptance criteria below |
| `KrowdKontrol.Build.cs` | NONE | No new module dependency — `Engine`/`CoreUObject` already cover `AActor`/`UStaticMeshComponent`; reuses `GizmoBark.h`/`GizmoNarrativeSubsystem.h` types from already-merged issue #57 |

## Acceptance criteria

- [x] **`APlaceholderTerminalActor` exists, constructs a placeholder mesh, and
      exposes `Interact()`.** `PlaceholderTerminalActor.h`/`.cpp` — mirrors the
      existing placeholder-actor constructor pattern, reusing the
      `/Engine/BasicShapes/Cylinder.Cylinder` mesh already used by
      `PlaceholderTargetZoneActor`.
- [x] **Interacting with the terminal is never required to progress.** `Interact()`
      touches only `TerminalLog`/`OnTerminalLogRevealed`; no door/objective/
      progression API exists anywhere in the class to depend on it — a structural
      fact, not just documented behavior.
- [x] **`Interact()` displays (broadcasts) its log text exactly once.** No-replay
      guard (`if TerminalLog.bHasBeenTriggered: return`, else flip-then-broadcast)
      copied from `TriggerBark`'s exact ordering; proven by the Automation test's
      "fires once, second call no-ops" assertions.
- [x] **`KrowdKontrol.Unit.PlaceholderTerminalActor` passes.**
      `KrowdKontrolPlaceholderTerminalActorTest.cpp` — construction + mesh wiring,
      not-yet-interacted state, first `Interact()` broadcasts once with correct
      `BarkID`/`Lines`, second `Interact()` is a no-op, minimal public surface.
- [x] **No regressions in the existing `KrowdKontrol.Unit.*` suite.** Full gate run
      (`harness/ci.py`, mode=full) passed all 16 unit tests plus the new one via real
      Editor automation.
- [x] **Code mirrors existing patterns exactly.** Constructor pattern from
      `PlaceholderCubeActor`; no-replay idiom from `GizmoNarrativeSubsystem`;
      "caller invokes directly, no opinion on trigger source" doc-comment phrasing
      from `StationPowerUpComponent`; test structure reuses `UGizmoBarkTestListener`
      verbatim.

## Validation

Full gate result from `dark-factory-validate` (`harness/ci.py`, mode=full):

```
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=16
APP_STARTED driver=cli
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
HOLDOUT_ABSENT no .factory/holdout/run.py - NOTHING above the independence line ran.
MUTATIONS_ABSENT no harness/mutations/run.py - this gate has never been shown to fail.
GATE_OK mode=full
```

MISSION.md Hard Invariants reviewed by inspection: `APlaceholderTerminalActor` is a
narrative terminal (foreshadowing log text), not an enemy, ability, or
gameplay-colour object, and never touches progression/gating logic. Invariants #2
(no-kill), #3 (5-colour lock), #4 (ability roster), #5 (enemy roster), and #6
(engine/2D) are not implicated by this diff. No regression found.

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
