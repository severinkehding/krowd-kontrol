# Issue #56: Prototype: minimal flat-camera 3D top-down movement test scene

Prototype/spike work for PRD 14 REQ-1 (Paper2D vs. flat-camera-3D pipeline
comparison) — deliberately does not make that pick itself (MISSION.md Hard
Invariant 6). Adds `AFlatCamera3DPrototypePawn` (mesh root, `UFloatingPawnMovement`,
spring-arm-mounted top-down camera locked via `bUsePawnControlRotation = false`,
WASD/arrow-bound `MoveForward`/`MoveRight`), the matching `DefaultInput.ini`
`AxisMappings`, and `KrowdKontrol.Unit.FlatCamera3DPipelineSmoke` proving the pawn
spawns and wires its components correctly. Companion friction/iteration-speed notes
in `docs/flat-camera-3d-prototype-notes.md` for later comparison against
`docs/paper2d-prototype-notes.md` (issue #55).

## Files changed (all under `app/`, gitignored per D-003 — this is the tracked-repo
record of that change, see the closing note below)

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/FlatCamera3DPrototypePawn.h` | CREATE | `AFlatCamera3DPrototypePawn : public APawn` declaration — `MeshComponent`/`MovementComponent`/`CameraBoom`/`TopDownCamera`, `SetupPlayerInputComponent` override, private `MoveForward`/`MoveRight` handlers |
| `app/Source/KrowdKontrol/FlatCamera3DPrototypePawn.cpp` | CREATE | Constructor wiring (mesh root via `ConstructorHelpers::FObjectFinder`, spring arm with fixed relative rotation and `bDoCollisionTest=false`, `UpdatedComponent` set explicitly on the movement component) and axis-bound movement handlers |
| `app/Config/DefaultInput.ini` | UPDATE | `MoveForward`/`MoveRight` `AxisMappings=` entries under `[/Script/Engine.InputSettings]` |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolFlatCamera3DPipelineSmokeTest.cpp` | CREATE | `KrowdKontrol.Unit.FlatCamera3DPipelineSmoke` — spawns the pawn in a fresh map, asserts all four components exist and `MovementComponent->UpdatedComponent == MeshComponent` |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolStationPowerUpComponentTest.cpp` | UPDATE (out-of-plan fix) | Pre-existing test (issue #60) called `AActor::InputEnabled()`/`EnableInput(nullptr)`, neither of which do what the test needed under UE 5.8 on a bare `AActor`; replaced with a directly-constructed `InputComponent` as the enabled/disabled proxy. Also fixed `AddExpectedError(TEXT("OrderedLights[1] is null"), ...)`, which was silently failing to match because `IsRegex` defaults to `true` and `[1]` was being parsed as a regex character class |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolGizmoNarrativeSubsystemTest.cpp` | UPDATE (out-of-plan fix) | Pre-existing test (issue #57) constructed `UGizmoNarrativeSubsystem` (`UCLASS(Within = GameInstance)`) via a bare `NewObject<>()` with no Outer, failing Outer-class validation at runtime; fixed by constructing a `NewObject<UGameInstance>()` and passing it as the Outer |
| `docs/flat-camera-3d-prototype-notes.md` | CREATE (tracked directly, not under `app/`) | Friction/iteration-speed notes: ~1hr for the headless-buildable portion, Enhanced-Input-vs-legacy-`BindAxis` finding still unconfirmed in PIE, camera-lock setup easier than expected |

`app/Content/Maps/L_FlatCamera3DPrototype.umap` was **not** created — see Acceptance
Criteria below, environment-blocked, not mirrored here (binary Content assets are
excluded from the `app-source-tracked/` convention regardless).

## Acceptance criteria

- [ ] **A minimal test level (`L_FlatCamera3DPrototype`) exists containing one
      primitive 3D Pawn that moves in top-down space via WASD/arrow input, with a
      camera fixed to a top-down angle.** Blocked on environment: no live Unreal
      Editor/MCP session was available in this session (`mcp__unreal-mcp__*` tools
      absent, `curl http://127.0.0.1:8000/mcp` timed out) — level authoring cannot be
      done by editing text files. Needs a follow-up human/interactive-session pass to
      create the level, place one `AFlatCamera3DPrototypePawn` instance, and save.
- [x] **`KrowdKontrol.Unit.FlatCamera3DPipelineSmoke` confirms the prototype pawn
      class exists and spawns correctly.** Headless-buildable, no environment
      dependency — passes as part of the harness run below.
- [x] **`docs/flat-camera-3d-prototype-notes.md` records setup friction and
      iteration speed.** Done — see the file for time-per-task breakdown and the
      open Enhanced-Input finding.
- [x] **Level 1-3 validation commands pass with exit 0** (the level-authoring task
      is not required for the harness gate). Confirmed below.
- [x] **Code mirrors existing patterns exactly** — pawn class structure mirrors
      `APlaceholderCubeActor`'s `ConstructorHelpers::FObjectFinder` pattern; the test
      mirrors `KrowdKontrolRoomEnemyBudgetControllerTest.cpp`'s `CreateNewMap()` +
      `SpawnActor` pattern.
- [x] **No regressions in the pre-existing `KrowdKontrol.Unit.*` tests.** All pass —
      see validation output (`tests=12`, up from a pre-existing baseline once two
      unrelated pre-existing bugs, described above, were fixed to get a trustworthy
      build at all).
- [x] **Does not violate MISSION.md Hard Invariant 6 or the 5-colour lock.** No
      Paper2D-vs-flat-camera-3D decision is made by this issue; the prototype uses
      only the engine's default gray cube material, no colour choices made.

## Validation

```
$ python harness/ci.py
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=12
APP_STARTED driver=cli
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
HOLDOUT_ABSENT no .factory/holdout/run.py - NOTHING above the independence line ran.
MUTATIONS_ABSENT no harness/mutations/run.py - this gate has never been shown to fail.
GATE_OK mode=full
```

`UNIT_PASSED tests=12` covers all pre-existing `KrowdKontrol.Unit.*` tests plus this
issue's new `FlatCamera3DPipelineSmoke` test. Getting a trustworthy build required
fixing two pre-existing, unrelated test bugs first (see the two UPDATE rows above) —
the harness's `cli` driver had been silently reporting a stale pass count against an
out-of-date `.dll` rather than actually compiling changed source; flagged as a
repo-level harness gap for a follow-up issue, not fixed in this pass (outside this
issue's scope).

---

Source lives under `app/` (gitignored, D-003) — this file and
`app-source-tracked/` are the tracked-repo record of that change, not a substitute
for reading the actual code. `app/Content/Maps/L_FlatCamera3DPrototype.umap` remains
outstanding pending a live-Editor pass.
