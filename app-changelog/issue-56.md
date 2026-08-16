# Issue #56: Flat-camera-3D pipeline prototype

Adds the flat-camera-3D half of PRD 14 REQ-1's Paper2D-vs-flat-camera-3D pipeline
comparison: `AFlatCamera3DPrototypePawn`, a minimal top-down prototype pawn (primitive
cube mesh, `UFloatingPawnMovement`-driven WASD/arrow input in world space, and a
`USpringArmComponent` locked to a fixed `-80°` top-down pitch, non-collision-testing,
not player-adjustable), an Automation Framework smoke test confirming the wiring, and
`L_FlatCamera3DPrototype.umap`, a test level containing a placed pawn instance. Does
not itself decide Paper2D vs flat-camera-3D — per Hard Invariant 6, that's a human call
made by comparing this against the companion Paper2D prototype (issue #55).

This is a verify-and-land pass, not a from-scratch implementation. The substantive
engineering (pawn class, movement wiring, camera lock, input mappings, smoke test) was
already built and already survived one review cycle in a prior, closed PR (#99) — the
`SetUpdatedComponent()` fix and axis-binding-assertion extension in the mirrored files
below both carry over from that cycle. That prior attempt was rejected for bundling
unrelated fixes to `StationPowerUpComponent`/`GizmoNarrativeSubsystem` test files, not
for a defect in the flat-camera-3D work itself; those fixes have since landed
independently on `main` via issues #60 and #57. This pass is scoped to exactly the
flat-camera-3D files, confirms the previously-unverified level actually contains a
placed pawn (via live Unreal MCP — `AFlatCamera3DPrototypePawn_0` present,
`CameraBoom.relativeRotation.pitch = -80`, `bUsePawnControlRotation = false`), and adds
the friction-notes doc the issue's third acceptance criterion requires.

## Files changed (all under `app/`, gitignored per D-003 — this is the tracked-repo
record of that change, see the closing note below)

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/FlatCamera3DPrototypePawn.h` | CREATE | Pawn declaration: `MeshComponent` (root), `MovementComponent` (`UFloatingPawnMovement`), `CameraBoom` (`USpringArmComponent`), `TopDownCamera` (`UCameraComponent`), `SetupPlayerInputComponent()` override |
| `app/Source/KrowdKontrol/FlatCamera3DPrototypePawn.cpp` | CREATE | Implementation: default cube mesh via `ConstructorHelpers::FObjectFinder` (placeholder-first pattern), `MovementComponent->SetUpdatedComponent(MeshComponent)` (goes through the engine setter so `UpdatedPrimitive` and the physics-volume-changed delegate are populated/bound, not just a raw field write), `CameraBoom` at `-80°` pitch/`800` arm length/no collision test/no pawn-control rotation, `MoveForward`/`MoveRight` bound to world-space `AddMovementInput` |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolFlatCamera3DPipelineSmokeTest.cpp` | CREATE | `KrowdKontrol.Unit.FlatCamera3DPipelineSmoke` — spawns the pawn via `FAutomationEditorCommonUtils::CreateNewMap()`, asserts all 4 components are non-null, mesh is root, movement component drives the mesh root (not just references it), camera boom pitch `<= -45°`, boom rotation locked (`bUsePawnControlRotation == false`), and that `SetupPlayerInputComponent` actually registers both `MoveForward`/`MoveRight` axis bindings |
| `app/Config/DefaultInput.ini` | CREATE (first tracked copy) | 8 `AxisMappings` entries binding `MoveForward`/`MoveRight` to WASD and arrow keys |
| `app/Content/Maps/L_FlatCamera3DPrototype.umap` | app/-only, not mirrored (binary, not text-diffable) | Test level; verified via live Unreal MCP to contain a placed `FlatCamera3DPrototypePawn_0` instance with the expected `-80°` boom pitch |

No `app/Source/KrowdKontrol/KrowdKontrol.Build.cs` change was needed —
`UFloatingPawnMovement`, `USpringArmComponent`, and `UCameraComponent` are all
already-covered `Engine`-module types.

## Acceptance criteria

- [x] **`L_FlatCamera3DPrototype.umap` exists and contains a placed
      `AFlatCamera3DPrototypePawn` movable via WASD/arrow input, with a top-down-locked
      camera** — confirmed live via Unreal MCP: `find_actors` in the loaded level
      returns `FlatCamera3DPrototypePawn_0`; its `CameraBoom.relativeRotation.pitch` is
      `-80` and `bUsePawnControlRotation` is `false`. Viewport capture additionally
      shows the pawn's components placed at the level origin on the floor plane.
- [x] **`KrowdKontrol.Unit.FlatCamera3DPipelineSmoke` passes** — re-confirmed via a full
      `harness/ci.py` run (see Validation below).
- [x] **`docs/flat-camera-3d-prototype-notes.md` exists and records real
      friction/iteration notes.**
- [x] **`app-source-tracked/` mirror + this changelog entry exist** so the PR has a
      real, reviewable diff.

## Validation

```
$ python harness/ci.py
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

`UNIT_PASSED tests=16` covers `KrowdKontrol.Unit.FlatCamera3DPipelineSmoke` alongside
every other pre-existing `KrowdKontrol.Unit.*`/`KrowdKontrol.Smoke.*` test — no
regression.

---

Source lives under `app/` (gitignored, D-003) — this file is the tracked-repo record
of that change, not a substitute for reading the actual code.
