# Issue #262: In-Game Cursor with World-Position Deprojection API

## Summary

Adds a visible in-game mouse cursor and a single reusable "cursor world position on
the gameplay floor plane" query, both owned by the existing top-down prototype pawn
(`AFlatCamera3DPrototypePawn`) and its controller (`AKrowdKontrolPlayerController`).
This is REQ-1 of `docs/prd-cursor-aiming.md`; REQ-2 (facing) and REQ-3 (targeting
indicators) are separate, out-of-scope issues that will call this same API once it
lands.

`AKrowdKontrolPlayerController::BeginPlay()` now sets `bShowMouseCursor = true` so a
hardware cursor renders during PIE/gameplay, independent of `DefaultInput.ini`'s
mouse-capture/lock settings (orthogonal Unreal systems).

`AFlatCamera3DPrototypePawn` gains two new public members:
- `GetCursorWorldPosition(FVector& OutWorldPosition) const` — the production entry
  point every consumer calls each frame. Delegates the camera-to-ray step to
  `APlayerController::DeprojectMousePositionToWorld()` — the engine's own
  already-correct, already-tested path (it walks `ULocalPlayer::GetProjectionData()`,
  so it honours the project's real `AspectRatioAxisConstraint` instead of
  approximating it) — then defers to the function below for the ray/floor math.
- `static IntersectRayWithGroundPlane(...)` — the only hand-rolled math in the
  project, and deliberately narrow: pure ray/plane intersection given a
  `RayOrigin`/`RayDirection`/`GroundPlaneZ`, no camera or viewport data at all. This
  is what makes the math unit-testable headlessly in this project's
  `CreateNewMap()`-based Automation Framework tests (real `APlayerController`
  deprojection needs a live `ViewportClient`, unavailable in those test Worlds) —
  the camera->ray step itself is left entirely to the engine rather than
  hand-reconstructed, since a hand-rolled reconstruction of the projection matrix
  turned out not to reproduce the engine's actual `AspectRatio_MaintainYFOV`
  behavior on non-16:9 viewports (caught in review; original approach reimplemented
  view-projection math by hand and diverged from the live rendering path off
  screen-center).

A new automation test file (`KrowdKontrolCursorWorldPositionTest.cpp`) exercises the
pure ray/plane math directly with synthetic rays, plus two smaller World-based tests
for the pawn wrapper's negative path and the controller's cursor-visibility flag.

## Acceptance Criteria

| Criterion | Status |
|---|---|
| `AKrowdKontrolPlayerController::BeginPlay()` sets `bShowMouseCursor = true` | Done |
| `AFlatCamera3DPrototypePawn::GetCursorWorldPosition(FVector&)` exists, is public, and is the only deprojection entry point on the pawn | Done |
| `AFlatCamera3DPrototypePawn::IntersectRayWithGroundPlane(...)` exists as a `static` function taking a plain ray (no `UWorld`/viewport/camera-view dependency) | Done |
| All 3 new automation tests pass via `harness/run_ue_automation.sh "KrowdKontrol.Unit."` | Done — `KrowdKontrol.Unit.CursorGroundPlaneDeprojectionMath`, `KrowdKontrol.Unit.FlatCamera3DPrototypePawnCursorWorldPosition`, `KrowdKontrol.Unit.PlayerControllerShowsMouseCursor` |
| No existing `KrowdKontrol.Unit.*` test regresses | Done — see Validation Evidence below |
| Code mirrors existing patterns exactly (forward-declare style, `ApplyCameraFraming()`-style extraction of pure logic, comment density) | Done |
| Manually verified in PIE: cursor renders and mouse capture/lock still behaves as before | **Not verified this run — no interactive PIE session available in this environment; flagged in the PR body per the plan's Risks section.** Lower-risk than originally flagged: the camera-to-ray step now delegates to the engine's own tested `DeprojectMousePositionToWorld()` rather than a hand-rolled projection-matrix reconstruction, so there is no longer a custom aspect-ratio computation for a PIE check to catch. |

## Post-Review Fix (self-fix pass)

Code review (against the actual UE 5.8 engine source) found that the original
`DeprojectScreenPositionToGroundPlane`'s hand-rolled `CameraView.AspectRatio`
override did not reproduce the engine's real `AspectRatio_MaintainYFOV` projection
behavior — the two only coincided on an exactly-16:9 viewport, so the deprojected
cursor position would silently diverge from what's rendered on any other window
shape. No automation test could catch this because every test case probed the
degenerate screen-center point. Fixed by deleting the hand-rolled projection-matrix
reconstruction entirely: `GetCursorWorldPosition()` now calls
`APlayerController::DeprojectMousePositionToWorld()` (the engine's own
already-correct, already-tested path) for the camera->ray step, and the project's
own static helper (renamed `IntersectRayWithGroundPlane`) was narrowed to pure
ray/plane intersection with no aspect-ratio dependency left to get wrong. The
existing closed-form-trig test cases were converted to feed rays directly instead of
camera views/screen positions and remain valid unchanged.

## Validation Evidence

`harness/run_ue_automation.sh "KrowdKontrol.Unit."` → `UE_BUILD_OK`, then
`UE_AUTOMATION_RESULT passed=86 total=86`, then `UE_AUTOMATION_OK`.

See `implementation.md` in the workflow run artifacts for the full record of the
original implementation pass.
