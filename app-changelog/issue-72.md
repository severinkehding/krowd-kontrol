# Issue #72: Add APlaceholderTargetZoneActor

Adds `APlaceholderTargetZoneActor`, a placeholder-first actor carrying a world-space
"beacon" visual — a flattened static-mesh floor marker (`BeaconMeshComponent`) plus a
glowing point light (`BeaconLightComponent`) attached to it — so PRD 13 REQ-6
("target-zone indicators are world-space UI... not screen-space HUD") has something
concrete to render and test against. No real target-zone gameplay logic (detection
radius, banking) is built here; that's explicitly out of scope per the issue and
tracked separately by the core-gameplay-loop PRD. The beacon's light colour is a
saturated green (`FLinearColor(0.2f, 1.0f, 0.3f)`), chosen specifically because it is
not one of MISSION.md Hard Invariant 3's five reserved gameplay-information colours
(Purple/Teal/Orange/Blue/White).

## Files changed (all under `app/`, gitignored per D-003 — this is the tracked-repo
record of that change, see the closing note below)

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/PlaceholderTargetZoneActor.h` | CREATE | Declares `APlaceholderTargetZoneActor` with `BeaconMeshComponent` (`UStaticMeshComponent`) and `BeaconLightComponent` (`UPointLightComponent`), mirroring `APlaceholderCubeActor`'s shape |
| `app/Source/KrowdKontrol/PlaceholderTargetZoneActor.cpp` | CREATE | Constructor wires a flattened `/Engine/BasicShapes/Cylinder.Cylinder` mesh as the root component and a point light attached to it, coloured a non-reserved green |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolPlaceholderTargetZoneActorTest.cpp` | CREATE | `KrowdKontrol.Unit.PlaceholderTargetZoneActorHasVisibleBeacon` — spawns the actor into a real test map (`FAutomationEditorCommonUtils::CreateNewMap()`) and asserts the mesh and light components are present, visible, and correctly coloured |

No existing file needed an UPDATE — `KrowdKontrol.Build.cs` already has the conditional
`UnrealEd` dependency this test's `CreateNewMap()` call needs (added for issue #82).

## Acceptance criteria

- [x] **World-space visual beacon (glowing floor marker) rendered at target-zone
      locations, distinct from any screen-space HUD element.** Satisfied by
      `BeaconMeshComponent` + `BeaconLightComponent` on `APlaceholderTargetZoneActor`, a
      pure `AActor` with no UMG/Slate involvement.
- [x] **A minimal placeholder target-zone actor with the beacon visual attached,
      sufficient to demonstrate the beacon rendering in a test level.** Satisfied by
      `APlaceholderTargetZoneActor` itself.
- [x] **The beacon visual does not use any of the five reserved
      gameplay-information colours.** Satisfied by the chosen
      `FLinearColor(0.2f, 1.0f, 0.3f)` green, documented in-code and locked in by the
      test's `TestEqual` assertion.
- [x] **An Automation Framework test confirms a beacon-bearing actor is present and
      visible in a test map.** Satisfied by
      `KrowdKontrol.Unit.PlaceholderTargetZoneActorHasVisibleBeacon`.

## Validation

```
$ python harness/ci.py
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=6
APP_STARTED driver=cli
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
HOLDOUT_ABSENT no .factory/holdout/run.py - NOTHING above the independence line ran.
MUTATIONS_ABSENT no harness/mutations/run.py - this gate has never been shown to fail.
GATE_OK mode=full
```

`UNIT_PASSED tests=6` covers the new
`KrowdKontrol.Unit.PlaceholderTargetZoneActorHasVisibleBeacon` test alongside every
pre-existing `KrowdKontrol.Unit.*` test — no regression.

---

Source lives under `app/` (gitignored, D-003) — this file is the tracked-repo record
of that change, not a substitute for reading the actual code.
