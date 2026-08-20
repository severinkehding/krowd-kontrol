# Issue #191: Add visible in-world markers for door connectors

**Type**: feature

## Summary

Adds a visible doorway marker — a small placeholder-sphere `DoorMarkerMeshComponent`
plus an attached `DoorMarkerLightComponent` — to `ADoorConnectorActor`, positioned above
the existing floor-strip midpoint (`RecomputeConnectorGeometry()`, extended alongside
the floor logic it already computes). Because the marker lives on the actor class
itself rather than a new hand-placed actor, every already-placed `ADoorConnectorActor`
instance in `L_Level01` (2 doors) and `L_Level02` (3 doors) picks it up automatically —
no live Editor/MCP session was needed to satisfy the "applied to both levels" criterion.
The marker's light colour is a desaturated warm neutral (`(0.75, 0.65, 0.5)`), chosen to
avoid re-opening the unresolved "6th saturated colour" ambiguity from issue #72, mirroring
issue #186's already-shipped desaturated-colour precedent instead.

During validation, an additional bug was found and fixed at the root: the marker light's
visibility was never toggled in lockstep with the marker mesh, so a freshly-placed
`ADoorConnectorActor` with no rooms assigned yet would still show a lit point light with
no mesh to justify it. Fixed by driving `DoorMarkerLightComponent`'s visibility through
the same branches as `DoorMarkerMeshComponent` in the constructor and
`RecomputeConnectorGeometry()`, with 3 new test assertions covering the light-visibility
lifecycle.

## Acceptance criteria

- [x] `DoorConnectorActor` has a visible in-world marker (mesh + point light), distinct
      from the existing floor strip — `DoorMarkerMeshComponent` + `DoorMarkerLightComponent`
- [x] Marker colour is not one of the 5 reserved gameplay-information colours — verified
      by an automated colour-lock test (`ContainsByPredicate` against
      `ReservedGameplayColours::GetAll()`), `validation.md` Phase 3
- [x] Applied to `L_Level01` (2 doors) and `L_Level02` (3 doors) — via
      `CheckDoorsHaveVisibleMarker()` regression assertions against the live,
      already-placed door instances; no map-file edit required
- [x] `python3 harness/ci.py` (full mode) passes, no regressions —
      `GATE_OK mode=full`, `UNIT_PASSED tests=71`
- [x] Marker mesh has no collision (`SetCollisionEnabled(ECollisionEnabled::NoCollision)`,
      mirroring `ARoomActor`'s wall meshes)

## Validation evidence

```
$ python harness/ci.py
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

Full detail in `implementation.md` and `validation.md` for this run
(`artifacts/runs/008ae110db20d0fd4124455649952f37/`).

## Files

| File | Action |
|------|--------|
| `app/Source/KrowdKontrol/DoorConnectorActor.h` | UPDATE |
| `app/Source/KrowdKontrol/DoorConnectorActor.cpp` | UPDATE |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolDoorConnectorActorTest.cpp` | UPDATE |
| `app/Source/KrowdKontrol/Private/Tests/LevelStructureTestUtils.h` | UPDATE |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolLevel01Test.cpp` | UPDATE |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolLevel02Test.cpp` | UPDATE |
