# Issue #39: Foundational Room and Door/Connector actor classes for level construction

Adds the minimal, reusable C++ building blocks every hand-authored krowd-kontrol level
will be built from: a placeable `ARoomActor` that can hold an arbitrary number of
tagged "target zone" marker children, and a placeable `ADoorConnectorActor` that
references exactly two rooms it connects. This is structural/topology-only work — no
enemy AI, ability, or HUD logic — and unblocks the hand-authored Level 1/2/3 issues
from PRD `05-level-design-and-progression.md` that depend on it. Also adds `EEnemyType`,
the first place the MISSION.md Hard Invariant 5 locked 4-enemy roster becomes a real
C++ type rather than only prose/colour-accessor comments.

## Files changed (all under `app/`, gitignored per D-003 — this is the tracked-repo
record of that change, see the closing note below)

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/EnemyType.h` | CREATE | `EEnemyType`, a `UENUM(BlueprintType)` with exactly the 4 locked codenames (`RU_NNR`, `TR_UPR`, `B0_0MR`, `SN_1PR`), each with a `UMETA(DisplayName = "...")` preserving the hyphenated codename in the Editor/Blueprint UI. No `Count` sentinel — the roster is locked, not iterated. |
| `app/Source/KrowdKontrol/RoomActor.h` | CREATE | `FRoomTargetZone` struct (`EnemyType` + `MarkerActor`) and `ARoomActor` declaration: `AddTargetZone(EEnemyType, TSubclassOf<AActor>)`, `GetTargetZones()`, private `TargetZones` array |
| `app/Source/KrowdKontrol/RoomActor.cpp` | CREATE | Constructor creates a `USceneComponent` root (`RoomRoot`); `AddTargetZone` spawns the given class (or `APlaceholderTargetZoneActor` if none given), attaches it to the room via `SnapToTargetNotIncludingScale` so the marker starts at the room's origin rather than the world origin, and records the `(EnemyType, MarkerActor)` pair |
| `app/Source/KrowdKontrol/DoorConnectorActor.h` | CREATE | `ADoorConnectorActor` declaration: `EditInstanceOnly` `RoomA`/`RoomB` (`TObjectPtr<ARoomActor>`), inline `ConnectsValidRooms()` (`RoomA && RoomB && RoomA != RoomB`) |
| `app/Source/KrowdKontrol/DoorConnectorActor.cpp` | CREATE | Constructor creates a `USceneComponent` root (`DoorConnectorRoot`) — no other logic; no placeholder visual required by the acceptance criteria |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolRoomActorTest.cpp` | CREATE | `KrowdKontrol.Unit.RoomActor` — spawns a room in a real `UWorld`, adds one then a second tagged target zone, asserts the marker is spawned, attached (`IsAttachedTo`), tagged, and tracked |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolDoorConnectorActorTest.cpp` | CREATE | `KrowdKontrol.Unit.DoorConnectorActor` — spawns two distinct rooms and a door, asserts `ConnectsValidRooms()` is false by default, true with two distinct rooms assigned, and false again when both slots point at the same room |

No `app/Source/KrowdKontrol/KrowdKontrol.Build.cs` change was needed — `UnrealEd` (for
`FAutomationEditorCommonUtils::CreateNewMap()`) and the module-root `PrivateIncludePaths`
fix were already present from issue #82's work.

## Post-review fixes

Addressed during PR review (see PR #105 review discussion):

- **`AddTargetZone` marker mislocation (HIGH)** — the marker spawns at world `(0,0,0)`
  (no `FTransform` passed to `SpawnActor`), so attaching with `KeepWorldTransform` left
  it stuck at the world origin, disconnected from any room not itself placed at
  `(0,0,0)`. Both existing tests spawn their test room at the origin too, so this never
  surfaced. Fixed by attaching with `SnapToTargetNotIncludingScale` instead, so the
  marker starts at the room's origin and can still be freely repositioned afterward.
- **`AddTargetZone`'s `MarkerClass` override branch untested (MEDIUM)** — added a third
  `KrowdKontrol.Unit.RoomActor` assertion that passes `APlaceholderCubeActor::StaticClass()`
  explicitly and checks the returned actor's class, closing the only branch of the
  spawn ternary with zero prior coverage.
- **`ConnectsValidRooms()`'s single-room-assigned state untested (LOW)** — added a
  `TestFalse` assertion to `KrowdKontrol.Unit.DoorConnectorActor` after only `RoomA` is
  set, completing the truth table (default → one set → two set → same-room).

## Acceptance criteria

- [x] **`ARoomActor` exists, is placeable, and supports an arbitrary number of child
      target-zone marker actors, each taggable with one of the 4 locked `EEnemyType`
      values** — `AddTargetZone` may be called any number of times; `KrowdKontrol.Unit.RoomActor`
      exercises two calls with independent tags.
- [x] **Target-zone marker visual uses a placeholder primitive, no 6th information
      colour introduced** — `AddTargetZone` defaults to the existing, already-validated
      `APlaceholderTargetZoneActor`.
- [x] **`ADoorConnectorActor` exists, is placeable, and references exactly two
      `ARoomActor` instances** — `RoomA`/`RoomB` named properties, guaranteed at the
      type level rather than a runtime-checked collection size.
- [x] **`KrowdKontrol.Unit.RoomActor` and `KrowdKontrol.Unit.DoorConnectorActor` tests
      exist and pass** — confirmed via `harness/ci.py` full mode (see Validation below).
- [x] **No enemy AI, ability, or HUD logic added** — confirmed by inspection; the new
      files reference only `AActor`/`USceneComponent`/the existing placeholder marker.
- [x] **Level 1-2 validation commands pass with `GATE_OK`** — see Validation below.
- [x] **No regressions in any pre-existing `KrowdKontrol.Unit.*`/`KrowdKontrol.Smoke.*`
      test.**

## Validation

```
$ python harness/ci.py
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=20
APP_STARTED driver=cli
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
HOLDOUT_ABSENT no .factory/holdout/run.py
MUTATIONS_ABSENT no harness/mutations/run.py
GATE_OK mode=full
```

`UNIT_PASSED tests=20` includes both new `KrowdKontrol.Unit.RoomActor` /
`KrowdKontrol.Unit.DoorConnectorActor` tests alongside every pre-existing
`KrowdKontrol.Unit.*`/`KrowdKontrol.Smoke.*` test, confirmed via the `unit` step's
`KrowdKontrol.Unit.` filter — no regressions. The gate passed on the first run; no
fixes were required during validation.

MISSION.md Hard Invariants reviewed against the diff: #5 (enemy roster exactly 4, no
net-new types) — `EEnemyType` declares exactly the 4 locked codenames; #3 (5-colour
lock), #2 (no enemy killed), #7 (no networking) — all N/A, no such logic in this diff;
#8 (Unreal project not tracked in git) — all new files stayed under the untracked
`app/` symlink, mirrored here only as plain-text source copies per D-009.

---

Source lives under `app/` (gitignored, D-003) — this file is the tracked-repo record
of that change, not a substitute for reading the actual code.
