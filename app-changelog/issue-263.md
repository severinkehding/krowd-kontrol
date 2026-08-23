# Issue #263: Make player pawn face the cursor each frame

Adds a yaw-only, per-tick facing update to `AFlatCamera3DPrototypePawn` (PRD
"Cursor & Aiming Foundation" REQ-2), depending on issue #262/PR #266's
`GetCursorWorldPosition()`. Enables `PrimaryActorTick.bCanEverTick` (previously
`false`) and adds a `Tick(float DeltaTime)` override that calls
`GetCursorWorldPosition()` each frame and, on success, computes a yaw-only
`FRotator` via a new pure static helper `ComputeFacingRotation()` (mirroring
`IntersectRayWithGroundPlane()`'s bool+out-param shape and `DoorConnectorActor.cpp`'s
degenerate-delta-guard + `.Rotation()` idiom) and applies it with
`SetActorRotation()`. `MoveForward`/`MoveRight` are untouched - they already use
fixed world axes (`FVector::ForwardVector`/`FVector::RightVector`), not
actor-relative ones, so WASD movement stays rotation-independent by construction.

## Acceptance criteria

- [x] The player pawn's yaw updates every tick to face the cursor's current world
      position, sourced from `GetCursorWorldPosition()` (issue #262) - see
      `AFlatCamera3DPrototypePawn::Tick()`.
- [x] WASD movement stays exactly as it is today - world-relative, unaffected by
      facing. `MoveForward`/`MoveRight` are unmodified.
- [x] Automation test: given a fixed cursor world position, the computed facing
      yaw matches the expected facing angle toward that position - covered by
      `KrowdKontrol.Unit.FlatCamera3DPrototypePawnFacingRotationMath` (5 cases:
      axis-aligned, off-axis, degenerate).
- [x] Automation test: pawn movement direction/velocity for a given WASD input is
      identical regardless of the pawn's facing rotation - covered by
      `KrowdKontrol.Unit.FlatCamera3DPrototypePawnFacingDoesNotAffectMovement`.
- [x] `app/` and `app-source-tracked/` are byte-identical for every changed file.
- [x] No regressions expected in existing `FlatCamera3DPipeline*`/
      `CursorWorldPosition*` automation tests - the only existing line changed is
      `PrimaryActorTick.bCanEverTick`'s constructor value; no existing method
      bodies changed.

---

The real Unreal project stays under `app/` (gitignored, D-003) - this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
