# Issue #57: Add a Gizmo narrative bark data system and trigger subsystem

Adds the reusable foundation for Krowd Kontrol's narrative delivery model (PRD 07): a
plain `FGizmoBark` struct (bark ID, 2-4 lines of display text, a "has fired" flag) and
`UGizmoNarrativeSubsystem` (`UGameInstanceSubsystem`), which lets callers register bark
definitions and trigger them by ID, broadcasting a `BlueprintAssignable` delegate any
future listener (HUD widget, boss-fight reactive-bark code, Crowd Mastery flavor text)
can bind to. Data + trigger layer only — no UI, no bark content, no gameplay-event
wiring. Those are follow-up issues #59, #61, #62, which depend on this landing.

## Files changed (all under `app/`, gitignored per D-003 — this is the tracked-repo
record of that change, see the closing note below)

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/GizmoBark.h` | CREATE | `FGizmoBark` — `USTRUCT(BlueprintType)` with `BarkID` (`FName`), `Lines` (`TArray<FString>`), `bHasBeenTriggered` (`bool`, defaults `false`). Header-only, no `.cpp` needed. |
| `app/Source/KrowdKontrol/GizmoNarrativeSubsystem.h` | CREATE | `UGizmoNarrativeSubsystem : public UGameInstanceSubsystem` declaration — `RegisterBark(const FGizmoBark&)`, `TriggerBark(FName)`, `HasBarkFired(FName) const`, `FOnBarkTriggered OnBarkTriggered` (`BlueprintAssignable`, carries `FName BarkID, TArray<FString> Lines`), private `TMap<FName, FGizmoBark> RegisteredBarks` |
| `app/Source/KrowdKontrol/GizmoNarrativeSubsystem.cpp` | CREATE | Implementation: `TriggerBark` logs a warning and no-ops on an unknown ID, silently no-ops on an already-fired ID, otherwise flips `bHasBeenTriggered` and broadcasts exactly once |
| `app/Source/KrowdKontrol/Private/Tests/GizmoBarkTestListener.h`/`.cpp` | CREATE | Test-only `UObject` listener — dynamic multicast delegates require a `UFUNCTION`-bound `AddDynamic` target, not a lambda, mirroring `StationPowerUpTestListener` |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolGizmoNarrativeSubsystemTest.cpp` | CREATE | `KrowdKontrol.Unit.GizmoNarrativeSubsystem` — covers all acceptance criteria below |
| `app/Source/KrowdKontrol/KrowdKontrol.Build.cs` | NONE | `Engine` (owner of `Subsystems/GameInstanceSubsystem.h`) already a `PublicDependencyModuleNames` entry — verified, not edited |

## Acceptance criteria

- [x] **`FGizmoBark` struct with a unique bark ID, 2-4 lines of display text, and a
      "has been triggered" flag.** `GizmoBark.h` — `BarkID` (`FName`), `Lines`
      (`TArray<FString>`), `bHasBeenTriggered` (`bool`).
- [x] **`UGizmoNarrativeSubsystem` (`UGameInstanceSubsystem`) exposes `TriggerBark(FName
      BarkID)` and an `OnBarkTriggered` multicast delegate carrying the bark's text
      lines.** `GizmoNarrativeSubsystem.h`/`.cpp`.
- [x] **Triggering a bark ID that doesn't exist is a no-op (no crash).**
      `TriggerBark`'s unknown-ID branch logs a warning and returns; covered by the
      Automation test's case 3 (`AddExpectedError` on the warning, asserts no
      broadcast).
- [x] **Triggering the same bark ID twice only broadcasts once.** `bHasBeenTriggered`
      guard; covered by the Automation test's case 2.
- [x] **`KrowdKontrol.Unit.GizmoNarrativeSubsystem` Automation Framework test confirms
      all of the above.** `KrowdKontrolGizmoNarrativeSubsystemTest.cpp` — three cases:
      known-ID trigger broadcasts once with correct text and marks fired; second
      trigger of the same ID doesn't re-broadcast; unknown-ID trigger doesn't crash,
      doesn't broadcast, logs a warning.

## Validation

Inline sanity check from the implementation step:

```
$ python harness/ci.py --quick
GATE_OK (STATIC_SKIPPED, UNIT_PASSED tests=9)
```

**Note on full-gate validation:** this workflow's automated `dark-factory-validate`
run recorded a `GATE_OK mode=full` result, but on inspection that run's hard-invariant
review and diff discussion refer to `AbilityData.cpp/h` and `StationPowerUpComponent.cpp/h`
— unrelated files from issues #60/#63 — not this issue's `GizmoBark`/
`GizmoNarrativeSubsystem` files. That happened because the validation step's local
`main` ref was two commits stale at the time it ran (missing the since-merged PRs for
#60/#63), which made `git diff main...HEAD` pick up those commits' files instead of
this branch's actual (`app/`-only) change. That artifact does not cover this issue and
should not be read as validation of this diff.

The new `KrowdKontrol.Unit.GizmoNarrativeSubsystem` Automation Framework test itself
still requires manual verification via `UnrealEditor-Cmd.exe` / Window → Test
Automation on the Windows host — the same documented, pre-existing gap noted in the
plan's Risks section for every `KrowdKontrol.Unit.*` test in this repo
(`harness.config.json`'s `unit` command isn't yet wired to Unreal's Automation
Framework).

MISSION.md Hard Invariants reviewed by inspection: this change adds no enemy, kill, or
colour-channel logic and touches no existing file's behavior — Hard Invariants 2-5 are
not implicated. No `UDataAsset`/`Content/` authoring involved.

---

Source lives under `app/` (gitignored, D-003) — this file is the tracked-repo record
of that change, not a substitute for reading the actual code.
