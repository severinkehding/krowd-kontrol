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
  point every consumer calls each frame. Resolves the possessing
  `APlayerController`'s live mouse position and `TopDownCamera`'s live view, then
  defers to the function below.
- `static DeprojectScreenPositionToGroundPlane(...)` — the actual math, and the only
  deprojection implementation in the project. Takes a camera view as plain data (no
  `UWorld`/viewport required), builds the same view/projection matrices
  `UGameplayStatics::DeprojectScreenToWorld` would build internally, deprojects to a
  world-space ray, then intersects that ray with the floor plane by hand. This is
  what makes the deprojection math unit-testable headlessly in this project's
  `CreateNewMap()`-based Automation Framework tests (real `APlayerController`
  deprojection needs a live `ViewportClient`, unavailable in those test Worlds).

A new automation test file (`KrowdKontrolCursorWorldPositionTest.cpp`) exercises the
pure math directly with synthetic camera transforms, plus two smaller World-based
tests for the pawn wrapper's negative path and the controller's cursor-visibility
flag.

## Acceptance Criteria

| Criterion | Status |
|---|---|
| `AKrowdKontrolPlayerController::BeginPlay()` sets `bShowMouseCursor = true` | Done |
| `AFlatCamera3DPrototypePawn::GetCursorWorldPosition(FVector&)` exists, is public, and is the only deprojection entry point on the pawn | Done |
| `AFlatCamera3DPrototypePawn::DeprojectScreenPositionToGroundPlane(...)` exists as a `static` function taking plain camera-view data (no `UWorld`/viewport dependency) | Done |
| All 3 new automation tests pass via `harness/run_ue_automation.sh "KrowdKontrol.Unit."` | Done — `KrowdKontrol.Unit.CursorGroundPlaneDeprojectionMath`, `KrowdKontrol.Unit.FlatCamera3DPrototypePawnCursorWorldPosition`, `KrowdKontrol.Unit.PlayerControllerShowsMouseCursor` |
| No existing `KrowdKontrol.Unit.*` test regresses | Done — 86/86 pass |
| Code mirrors existing patterns exactly (forward-declare style, `ApplyCameraFraming()`-style extraction of pure logic, comment density) | Done |
| Manually verified in PIE: cursor renders and mouse capture/lock still behaves as before | **Not verified this run — no interactive PIE session available in this environment; flagged in the PR body per the plan's Risks section.** |

## Validation Evidence

`harness/run_ue_automation.sh "KrowdKontrol.Unit."` → `UE_BUILD_OK`, then
`UE_AUTOMATION_RESULT passed=86 total=86`, then `UE_AUTOMATION_OK`.

See `implementation.md` in the workflow run artifacts for the full record.
