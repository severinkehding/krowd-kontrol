# Issue #190: Increase target-zone beacon visibility for wayfinding

**Type**: enhancement

## Summary

`APlaceholderTargetZoneActor`'s beacon was only visible up close: a floor-level
`BeaconLightComponent` with a 300uu attenuation radius, smaller than a single room
(`ARoomActor::RoomWallHeight` = 300uu). PRD `Level Playability & Presentation` REQ-6
(P1) wants the beacon to guide players across a dark level, so this adds a new
vertical `BeaconColumnMeshComponent` (400uu tall — taller than `RoomWallHeight`, so it
physically pokes above a room's walls) and relocates `BeaconLightComponent` to crown
its top, alongside a 3x attenuation-radius increase (300 → 900) and a 3x-preserved-
ratio intensity bump (baseline 3000 → 5000, intensified 9000 → 15000). No new art
assets — the column reuses the same engine `Cylinder` mesh already referenced for the
disc, scaled thin-and-tall instead of flattened. The beacon's colour is deliberately
left untouched: the issue's acceptance criteria only requires not introducing a
reserved colour for a *new* purpose, and the pre-existing saturated green already
shipped in issue #72 with an unresolved "6th colour" question that remains a
human-ruling call, not something this issue re-litigates.

During implementation, a real bug was found and fixed at the root before it ever
reached the engine: the plan's original constructor sketch nested
`BeaconColumnMeshComponent` under `BeaconMeshComponent` (the existing floor disc).
That disc is itself flattened to a 0.05 Z-scale, and Unreal composes a child
component's relative transform through its parent's scale — so a tall column nested
under a squashed parent would itself render squashed to roughly 5% of its intended
height, and the light re-parented onto that column would land far off its intended
position, not at the column's top. Fixed by giving the actor a new, plain, unscaled
root component (`TargetZoneRootComponent`, mirroring `ADoorConnectorActor`'s own
`DoorConnectorRoot` pattern already established in this codebase) with
`BeaconMeshComponent` and `BeaconColumnMeshComponent` attached as siblings under it,
so the column's scale is never contaminated by the disc's. `BeaconLightComponent`'s
relative-location offset onto the column was likewise derived from the *unscaled*
100uu engine cylinder's own local half-height (50), not `BeaconColumnHeight * 0.5f`,
since the light is a child of the column and its relative location is already
composed through the column's own height-dependent scale.

## Acceptance criteria

- [x] Beacon is visibly more prominent than before — `BeaconColumnMeshComponent`
      (400uu, taller than `RoomWallHeight`=300uu) crowned by the relocated light,
      plus 3x `AttenuationRadius` (300→900) and a 3x-preserved-ratio intensity bump
      (3000/9000 → 5000/15000)
- [x] Visible from a meaningfully greater distance — `AttenuationRadius` increase is
      the directly-testable proxy; perceptual confirmation is a PIE/holdout-level
      check, not a `-nullrhi` Automation assertion (see Notes)
- [x] Does not use any of the 5 reserved gameplay-information colours for a *new*
      purpose — no new colour is introduced by this change; the pre-existing green is
      untouched and out of scope
- [x] `python3 harness/ci.py` (full mode) passes, no regressions — `GATE_OK mode=full`
- [x] Existing `KrowdKontrol.Unit.PlaceholderTargetZoneActorHasVisibleBeacon` test
      updated in lockstep with the actor changes, plus new structural assertions for
      the column (mesh, visibility, attach-parent, scale, no-collision, height vs.
      `RoomWallHeight`)
- [x] `app-changelog/issue-190.md` created
- [x] `app-source-tracked/` mirrors match `app/` exactly (verified via `diff`, no
      output)

## Validation evidence

```
$ python3 harness/ci.py
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=71
APP_STARTED driver=cli
UE_BUILD_START KrowdKontrolEditor Win64 Development
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
GATE_OK mode=full
```

Automation tests run `-nullrhi` and cannot assert perceptual/rendered brightness or
distance-visibility, only structural properties (component wiring, scale, attach
parents, `Intensity`/`AttenuationRadius` values) — mirrors the same caveat already
documented in `LevelLightingRigActor.cpp`. The "visible from meaningfully greater
distance" half of the acceptance criteria is satisfied by the directly-testable
`AttenuationRadius` proxy plus a PIE/holdout-level visual check, not by this
automated suite alone.

## Notes

- Test count stays the same (71) — this change adds assertions to an existing test
  file rather than introducing a new one.
- No map-file edits were needed: the beacon visual lives on the actor class itself,
  so every already-placed `APlaceholderTargetZoneActor` instance in any level picks
  up the change automatically.

## Files

| File | Action |
|------|--------|
| `app/Source/KrowdKontrol/PlaceholderTargetZoneActor.h` | UPDATE |
| `app/Source/KrowdKontrol/PlaceholderTargetZoneActor.cpp` | UPDATE |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolPlaceholderTargetZoneActorTest.cpp` | UPDATE |
| `app-source-tracked/Source/KrowdKontrol/PlaceholderTargetZoneActor.h` | UPDATE |
| `app-source-tracked/Source/KrowdKontrol/PlaceholderTargetZoneActor.cpp` | UPDATE |
| `app-source-tracked/Source/KrowdKontrol/Private/Tests/KrowdKontrolPlaceholderTargetZoneActorTest.cpp` | UPDATE |
| `app-changelog/issue-190.md` | CREATE |
