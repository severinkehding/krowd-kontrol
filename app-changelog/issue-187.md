# Issue #187: Greybox floor and wall shells for rooms and connector paths

Extends `ARoomActor` and `ADoorConnectorActor` (issue #39) with real greybox geometry
so `L_Level01`/`L_Level02` stop rendering as void with floating target-zone beacons and
enemies (PRD `docs/prd-level-playability-presentation.md` REQ-3).

**Approach (a) — extending `ARoomActor`/`ADoorConnectorActor` directly — was chosen**,
per the issue's stated preference, over approach (b) (a separate room-shell generator
acting on the maps). Reasons: (1) it is generic, not tied to the current linear-chain
topology, so it doesn't need to be revisited when the P1 room-pool shuffler introduces
non-linear layouts; (2) because the geometry is added purely as new
`CreateDefaultSubobject` components on an existing class, the already-placed
`ARoomActor`/`ADoorConnectorActor` instances baked into `L_Level01.umap` and
`L_Level02.umap` pick it up automatically the next time those maps load — no map
regeneration or hand re-authoring step was needed.

`ARoomActor`'s constructor now spawns a flat floor `UStaticMeshComponent` sized by a
new `EditAnywhere` `RoomFloorExtent` (half-extents, cm) property, plus four
non-collidable wall `UStaticMeshComponent`s (North/South/East/West) sized by
`RoomFloorExtent`/`RoomWallHeight`/`RoomWallThickness`. `ADoorConnectorActor` gained a
`ConnectorFloorMeshComponent` and a new `RecomputeConnectorGeometry()` function that
positions/rotates/scales it to span the straight line between `RoomA`/`RoomB`'s live
`GetActorLocation()`, called from both `OnConstruction` (editor placement/property-edit
visibility) and `BeginPlay` (guarantees correctness at actual play time regardless of
load-time construction-script timing). Both use the codebase's existing
`ConstructorHelpers::FObjectFinder` + `/Engine/BasicShapes/Cube.Cube` pattern already
used by `PlaceholderCubeActor`/`PlaceholderTargetZoneActor` — no new mesh assets, no
`Build.cs` changes, no procedural mesh component.

**Walls have collision disabled.** `ARoomActor` has no data about which side of a room
a given door sits on, so a solid wall would seal off the very connector paths this
issue also has to make walkable. This is a stated, scoped-down greybox limitation, not
an oversight — a follow-up issue can add a per-door "which wall side" property once
someone actually needs door-shaped gaps. The floor keeps its default collision so it
still acts as a visible/physical ground plane.

## Files changed (all under `app/`, gitignored per D-003 — this is the tracked-repo
record of that change, see the closing note below)

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/RoomActor.h` | UPDATE | New `RoomFloorExtent`/`RoomFloorThickness`/`RoomWallHeight`/`RoomWallThickness` properties, 5 new `UStaticMeshComponent*` members (floor + 4 walls) |
| `app/Source/KrowdKontrol/RoomActor.cpp` | UPDATE | Constructs and sizes the floor and 4 wall mesh components from `RoomRoot`, sourced from `/Engine/BasicShapes/Cube.Cube` |
| `app/Source/KrowdKontrol/DoorConnectorActor.h` | UPDATE | New `ConnectorFloorWidth`/`ConnectorFloorThickness` properties, `ConnectorFloorMeshComponent`, `RecomputeConnectorGeometry()`, `OnConstruction`/`BeginPlay` overrides |
| `app/Source/KrowdKontrol/DoorConnectorActor.cpp` | UPDATE | Constructs the connector floor mesh; implements `RecomputeConnectorGeometry()`, `OnConstruction`, `BeginPlay` |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolRoomActorTest.cpp` | UPDATE | Asserts floor + 4 wall mesh components exist with a valid `UStaticMesh` set |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolDoorConnectorActorTest.cpp` | UPDATE | Gives the two test rooms distinct locations (previously both spawned at the world origin), calls `RecomputeConnectorGeometry()` after assigning rooms, asserts connector floor mesh visibility/scale, and asserts it hides again once rooms become invalid |
| `app/Source/KrowdKontrol/Private/Tests/LevelStructureTestUtils.h` | UPDATE | New `CheckRoomsHaveFloorGeometry`/`CheckDoorsHaveConnectorGeometry` shared helpers |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolLevel01Test.cpp` | UPDATE | Calls the two new shared helpers against the real `L_Level01` rooms/doors |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolLevel02Test.cpp` | UPDATE | Same, against `L_Level02` |
| `app-source-tracked/Source/KrowdKontrol/RoomActor.h` | UPDATE | Plain-text mirror of the above, per Hard Invariant 8's D-009 carve-out |
| `app-source-tracked/Source/KrowdKontrol/RoomActor.cpp` | UPDATE | Mirror |
| `app-source-tracked/Source/KrowdKontrol/DoorConnectorActor.h` | UPDATE | Mirror |
| `app-source-tracked/Source/KrowdKontrol/DoorConnectorActor.cpp` | UPDATE | Mirror |
| `app-source-tracked/Source/KrowdKontrol/Private/Tests/KrowdKontrolRoomActorTest.cpp` | UPDATE | Mirror |
| `app-source-tracked/Source/KrowdKontrol/Private/Tests/KrowdKontrolDoorConnectorActorTest.cpp` | UPDATE | Mirror |
| `app-source-tracked/Source/KrowdKontrol/Private/Tests/LevelStructureTestUtils.h` | UPDATE | Mirror |
| `app-source-tracked/Source/KrowdKontrol/Private/Tests/KrowdKontrolLevel01Test.cpp` | UPDATE | Mirror |
| `app-source-tracked/Source/KrowdKontrol/Private/Tests/KrowdKontrolLevel02Test.cpp` | UPDATE | Mirror |

No `.umap` changes — `L_Level01`/`L_Level02`'s already-placed `ARoomActor`/
`ADoorConnectorActor` instances pick up the new components automatically on next load
via CDO diffing, per the Solution Statement above.

## Deviations from the plan

`KrowdKontrolDoorConnectorActorTest.cpp`'s existing scaffold spawns both `RoomOne` and
`RoomTwo` via `World->SpawnActor<ARoomActor>()` with no explicit `FTransform`, so both
land at the world origin. `RecomputeConnectorGeometry()`'s `KINDA_SMALL_NUMBER` guard
against a zero-length span (an intentional, already-planned edge case for degenerate
geometry) meant the connector mesh never became visible in the test as originally
written — not a bug in `RecomputeConnectorGeometry()`, but a gap in the test setup.
Fixed by adding `RoomTwo->SetActorLocation(FVector(3000.f, 0.f, 0.f));` right after
`RoomTwo` spawns, so the two rooms have a genuine, non-degenerate span for the
visibility/scale assertions to exercise.

Also: `GetStaticMesh()` on `UStaticMeshComponent` returns `TObjectPtr<UStaticMesh>` in
this engine version (UE 5.8), not a raw pointer — `FAutomationTestBase::TestNotNull`'s
template argument deduction can't implicitly convert `TObjectPtr` to `ValueType*`
during deduction, so `TestNotNull(..., Component->GetStaticMesh())` failed to compile.
Fixed by assigning to a local `UStaticMesh*` variable first (matching the pattern
already used by `KrowdKontrolPlaceholderCubeActorTest.cpp` and other existing test
files) before passing it to `TestNotNull`.

## Acceptance criteria

- [x] **Every room in `L_Level01` and `L_Level02` has a visible floor and simple wall
      shells** — `ARoomActor`'s constructor-spawned `FloorMeshComponent` + 4 wall
      components, verified by `KrowdKontrol.Unit.Level01Structure`/`Level02Structure`'s
      `CheckRoomsHaveFloorGeometry` assertions against the real maps.
- [x] **Connector paths between rooms have a walkable, visible floor strip** —
      `ADoorConnectorActor::ConnectorFloorMeshComponent` + `RecomputeConnectorGeometry()`,
      verified by the same tests' `CheckDoorsHaveConnectorGeometry` assertions.
- [x] **Implementer picks approach (a) or (b), states which and why** — approach (a)
      chosen; rationale above.
- [ ] **Player can no longer see void anywhere along the playable path** — structurally
      covered by the above (mesh presence, correct scale/visibility, all gated), but
      genuine visual confirmation is NOT automatable by the current gate (no
      visual/screenshot holdout wired up, `.factory/decisions.md` D-005) and was not
      spot-checked in a live Unreal Editor session during this change. Should be
      spot-checked manually when a live Editor/MCP session is available.
- [x] **Level 1-3 validation commands pass with exit 0** — see Validation below.
- [x] **No regressions in existing tests** — 67/67 unit tests pass. This change adds
      assertions to four already-existing test functions
      (`KrowdKontrol.Unit.RoomActor`/`DoorConnectorActor`/`Level01Structure`/
      `Level02Structure`), not new standalone tests, so the registered test count
      (67) is unchanged from before this issue.

## Validation

```
$ python harness/ci.py
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=67
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

`UNIT_PASSED tests=67` — no regressions (this change only added assertions to four
already-existing tests, it did not add new `IMPLEMENT_SIMPLE_AUTOMATION_TEST` entries,
so the total registered test count is unchanged from before this issue; during
implementation, before the `KrowdKontrolDoorConnectorActorTest.cpp` zero-distance
test-setup gap above was fixed, the run reported `passed=66 total=67`).

MISSION.md Hard Invariants reviewed against the diff: no-kill rule, 5-colour lock,
5-ability roster, 4-type enemy roster, and engine/dimensionality lock are all N/A (no
such logic touched); `app/` not tracked in git (#8) — all changes stayed under the
untracked `app/` symlink, mirrored here only as a plain-text source copy per D-009's
carve-out.

---

Source lives under `app/` (gitignored, D-003) — this file is the tracked-repo record
of that change, not a substitute for reading the actual code.
