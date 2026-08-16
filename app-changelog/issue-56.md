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
| `app/Source/KrowdKontrol/FlatCamera3DPrototypePawn.cpp` | CREATE | Implementation: default cube mesh via `ConstructorHelpers::FObjectFinder` (placeholder-first pattern), `MovementComponent->SetUpdatedComponent(MeshComponent)` (defensive explicit call — verified against UE 5.8 engine source that `OnRegister()`'s auto-detection would reach the same state via the same setter either way), `CameraBoom` at `-80°` pitch/`800` arm length/no collision test/no pawn-control rotation, `MoveForward`/`MoveRight` bound to world-space `AddMovementInput` |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolFlatCamera3DPipelineSmokeTest.cpp` | CREATE/UPDATE (this pass) | `KrowdKontrol.Unit.FlatCamera3DPipelineSmoke` — spawns the pawn via `FAutomationEditorCommonUtils::CreateNewMap()`, asserts all 4 components are non-null, mesh non-null with the correct default-cube asset path, mesh is root, movement component drives the mesh root, camera boom pitch `<= -45°` with rotation/collision-test locked, camera's own rotation lock, that `SetupPlayerInputComponent` registers both axis bindings against the project's actual configured input component class (`UInputSettings::GetDefaultInputComponentClass()`, resolving to `UEnhancedInputComponent`) rather than a bare `UInputComponent`, and that invoking the bound delegates accumulates world-space movement input, not actor-relative. Also adds `KrowdKontrol.Unit.FlatCamera3DPipelineLevelHasConfiguredPawn`, which loads the actual shipped `L_FlatCamera3DPrototype.umap` and re-asserts the camera lock against the placed instance rather than a throwaway spawn |
| `app/Config/DefaultInput.ini` | app/-only, not mirrored (`.ini`, not `.h`/`.cpp`/`.Build.cs`, and outside `app/Source/` — falls outside the `app-source-tracked/` carve-out CLAUDE.md documents) | 8 `AxisMappings` entries binding `MoveForward`/`MoveRight` to WASD and arrow keys |
| `app/Content/Maps/L_FlatCamera3DPrototype.umap` | app/-only, not mirrored (binary, not text-diffable) | Test level; verified via live Unreal MCP to contain a placed `FlatCamera3DPrototypePawn_0` instance with the expected `-80°` boom pitch; now also covered by the automation test above |

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

Re-run after a self-fix pass addressing review findings on PR #102 (extended smoke-test
coverage, added the level-load test, closed the OnRegister/InitializeComponent doc
inaccuracy — see below). `scripts/ue_editor_close.sh` closed the live Editor session
first so `Build.bat KrowdKontrolEditor Win64 Development -waitmutex` was a genuine
recompile, not a stale-`.dll` reuse (`[1/5] Compile FlatCamera3DPrototypePawn.cpp`,
`[2/5] Compile KrowdKontrolFlatCamera3DPipelineSmokeTest.cpp`, `Result: Succeeded`),
before `harness/ci.py` ran:

```
$ python harness/ci.py
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=18
APP_STARTED driver=cli
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
HOLDOUT_ABSENT no .factory/holdout/run.py - NOTHING above the independence line ran.
MUTATIONS_ABSENT no harness/mutations/run.py - this gate has never been shown to fail.
GATE_OK mode=full
```

`UNIT_PASSED tests=18` covers `KrowdKontrol.Unit.FlatCamera3DPipelineSmoke` and the new
`KrowdKontrol.Unit.FlatCamera3DPipelineLevelHasConfiguredPawn` (the two flat-camera-3D
tests) alongside every other pre-existing `KrowdKontrol.Unit.*`/`KrowdKontrol.Smoke.*`
test — no regression. This number is one higher than the 17 previously reported in this
PR's description because that run predated this changelog's own second test; both
numbers are now reconciled against this single fresh run rather than living in two
places that could drift.

### Findings addressed in this self-fix pass (PR #102 review)

- **Enhanced Input compatibility, previously untested (CRITICAL)**: the smoke test now
  constructs against `UInputSettings::GetDefaultInputComponentClass()` (resolves to
  `UEnhancedInputComponent` per `DefaultInput.ini`) instead of a bare `UInputComponent`,
  mirroring `APawn::CreatePlayerInputComponent()`'s own construction pattern
  (`Engine/Private/Pawn.cpp`). The friction-notes doc's "it works" claim is reworded to
  reflect what's now actually verified vs. still open.
- **`OnRegister()` vs. `InitializeComponent()` comment conflict (MEDIUM)**: resolved by
  reading UE 5.8's actual `MovementComponent.cpp` — `OnRegister()` does call
  `SetUpdatedComponent()` again unconditionally in a game world (even correcting a
  prior raw field write), so the explicit constructor call is defensive, not required.
  Comment and doc corrected accordingly.
- **Missing mesh-asset assertion (HIGH)**: added, mirroring
  `KrowdKontrolPlaceholderCubeActorTest.cpp`'s non-null + path-equality pattern.
- **No coverage of the real `.umap` (HIGH)**: added
  `KrowdKontrol.Unit.FlatCamera3DPipelineLevelHasConfiguredPawn`, which loads
  `/Game/Maps/L_FlatCamera3DPrototype` via `FAutomationEditorCommonUtils::LoadMap()` and
  re-asserts the camera lock against the actually-placed pawn instance.
- **Partial camera-lock assertions (MEDIUM)**: added `bDoCollisionTest` and the camera's
  own `bUsePawnControlRotation` checks (previously only the boom's flags were checked).
- **Movement bindings untested for behavior (MEDIUM)**: added a check that invoking the
  bound `MoveForward`/`MoveRight` delegates accumulates world-space
  `ForwardVector`+`RightVector` into the pawn's pending movement input, not an
  actor-relative equivalent — the deliberate design a source comment already called out.
- **Not addressed (protected paths)**: `app-changelog/` missing from `CLAUDE.md`'s Repo
  Layout tree, and the Conventions section's stale `TBD` banner — both require editing
  `CLAUDE.md`, a protected path this PR cannot touch (see `CLAUDE.md`'s own Protected
  Paths section). Left as follow-up issues.
- **Not addressed (structural, not this PR's defect)**: axis-name strings having no
  single tracked source of truth against `DefaultInput.ini` — inherent to `app/Config/`
  being outside the `app-source-tracked/` carve-out (D-009), not fixable without
  mirroring `.ini` files.

---

Source lives under `app/` (gitignored, D-003) — this file is the tracked-repo record
of that change, not a substitute for reading the actual code.
