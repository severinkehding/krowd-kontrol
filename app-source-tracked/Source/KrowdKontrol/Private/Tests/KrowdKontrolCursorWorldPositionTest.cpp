// Confirms the acceptance criteria of issue #262 (PRD "Cursor & Aiming Foundation"
// REQ-1): AKrowdKontrolPlayerController::BeginPlay() shows the mouse cursor, and
// AFlatCamera3DPrototypePawn::IntersectRayWithGroundPlane() is a correct, pure
// ray/floor-plane intersection - the only hand-rolled deprojection-adjacent math in
// the project (the camera->ray step itself is delegated to the engine's own
// APlayerController::DeprojectMousePositionToWorld(), see GetCursorWorldPosition()'s
// header comment).
//
// Test 1 needs no UWorld at all - IntersectRayWithGroundPlane takes a ray as plain
// data, matching KrowdKontrolAbilityDataTest.cpp's pure-function shape. Tests 2 and 3
// need a real UWorld (CreateNewMap()) to spawn the pawn/controller into, matching
// KrowdKontrolFlatCamera3DPipelineSmokeTest.cpp and KrowdKontrolLevelFailedTest.cpp's
// controller-bootstrap pattern respectively.
//
// Test 2 can only exercise GetCursorWorldPosition()'s no-possessing-controller
// negative path - the true positive path needs a live viewport, which this
// project's CreateNewMap() Worlds don't have (see
// KrowdKontrolOvercrowdAudioVisualSyncTest.cpp's PlayerCameraManager bootstrap
// comment for the same class of limitation, and why the ray/floor math is instead
// factored into the pure static function Test 1 exercises directly).
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "FlatCamera3DPrototypePawn.h"
#include "KrowdKontrolPlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Camera/CameraComponent.h"
#include "Math/InverseRotationMatrix.h"
#include "SceneView.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolCursorGroundPlaneDeprojectionMathTest,
	"KrowdKontrol.Unit.CursorGroundPlaneDeprojectionMath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolCursorGroundPlaneDeprojectionMathTest::RunTest(const FString& Parameters)
{
	// Case A: ray straight down -> exact result, no trig needed. A camera looking
	// straight down sends its ray along -Z, so the world X/Y must exactly match the
	// ray origin's X/Y.
	{
		const FVector RayOrigin(1000.0f, 500.0f, 800.0f);
		const FVector RayDirection(0.0f, 0.0f, -1.0f);

		FVector OutWorldPosition;
		const bool bResult = AFlatCamera3DPrototypePawn::IntersectRayWithGroundPlane(
			RayOrigin, RayDirection, 0.0f, OutWorldPosition);

		TestTrue(TEXT("Case A (straight-down ray) should succeed"), bResult);
		TestTrue(TEXT("Case A world position should match the ray origin's X/Y on the floor"),
			OutWorldPosition.Equals(FVector(1000.0f, 500.0f, 0.0f), 0.5f));
	}

	// Case B: ray matching the pawn's default -60 degree boom pitch -> closed-form
	// trig result. Forward vector is (cos(-60deg), 0, sin(-60deg)) =
	// (0.5, 0, -0.8660254); t = (0 - 1000) / -0.8660254 = 1154.7005;
	// WorldPosition = (0,0,1000) + t*(0.5,0,-0.8660254) = (577.35, 0, 0).
	{
		const FVector RayOrigin(0.0f, 0.0f, 1000.0f);
		const FVector RayDirection(0.5f, 0.0f, -0.8660254f);

		FVector OutWorldPosition;
		const bool bResult = AFlatCamera3DPrototypePawn::IntersectRayWithGroundPlane(
			RayOrigin, RayDirection, 0.0f, OutWorldPosition);

		TestTrue(TEXT("Case B (pitched ray) should succeed"), bResult);
		TestTrue(TEXT("Case B world position should match the closed-form trig result"),
			OutWorldPosition.Equals(FVector(577.35f, 0.0f, 0.0f), 0.5f));
	}

	// Case C: ray parallel to the floor (looking exactly horizontal) -> must fail,
	// not divide by ~0.
	{
		const FVector RayOrigin(0.0f, 0.0f, 1000.0f);
		const FVector RayDirection(1.0f, 0.0f, 0.0f);

		FVector OutWorldPosition;
		const bool bResult = AFlatCamera3DPrototypePawn::IntersectRayWithGroundPlane(
			RayOrigin, RayDirection, 0.0f, OutWorldPosition);

		TestFalse(TEXT("Case C (ray parallel to floor) should fail"), bResult);
	}

	// Case D: floor plane behind the ray's origin along its direction of travel ->
	// must fail, not a backwards/nonsensical intersection.
	{
		const FVector RayOrigin(0.0f, 0.0f, 0.0f);
		const FVector RayDirection(0.5f, 0.0f, -0.8660254f);

		FVector OutWorldPosition;
		const bool bResult = AFlatCamera3DPrototypePawn::IntersectRayWithGroundPlane(
			RayOrigin, RayDirection, 1000.0f, OutWorldPosition);

		TestFalse(TEXT("Case D (floor behind ray origin) should fail"), bResult);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolFlatCamera3DPrototypePawnCursorWorldPositionTest,
	"KrowdKontrol.Unit.FlatCamera3DPrototypePawnCursorWorldPosition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolFlatCamera3DPrototypePawnCursorWorldPositionTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	AFlatCamera3DPrototypePawn* Pawn = World->SpawnActor<AFlatCamera3DPrototypePawn>();
	if (!TestNotNull(TEXT("Pawn should spawn"), Pawn))
	{
		return false;
	}

	FVector OutPosition;
	TestFalse(TEXT("GetCursorWorldPosition should fail gracefully with no possessing PlayerController"),
		Pawn->GetCursorWorldPosition(OutPosition));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolPlayerControllerShowsMouseCursorTest,
	"KrowdKontrol.Unit.PlayerControllerShowsMouseCursor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolPlayerControllerShowsMouseCursorTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	AFlatCamera3DPrototypePawn* Pawn = World->SpawnActor<AFlatCamera3DPrototypePawn>();
	if (!TestNotNull(TEXT("Pawn should spawn"), Pawn))
	{
		return false;
	}

	AKrowdKontrolPlayerController* Controller = World->SpawnActor<AKrowdKontrolPlayerController>();
	if (!TestNotNull(TEXT("Controller should spawn"), Controller))
	{
		return false;
	}

	Controller->Possess(Pawn);
	Controller->Player = NewObject<ULocalPlayer>(GEngine);
	Controller->SetAsLocalPlayerController();
	Controller->DispatchBeginPlay();

	TestTrue(TEXT("BeginPlay should show the mouse cursor"), Controller->bShowMouseCursor);

	return true;
}

// Positive-path coverage for the pass-1 review gap: nothing above exercises the
// screen-space-to-ray step GetCursorWorldPosition() delegates to
// (APlayerController::DeprojectMousePositionToWorld() ->
// UGameplayStatics::DeprojectScreenToWorld(), PlayerController.cpp:2182 /
// GameplayStatics.cpp:3235 in UE 5.8). That real call needs a live ULocalPlayer with
// a non-null UGameViewportClient (ULocalPlayer::GetProjectionData() requires
// ViewportClient->Viewport, LocalPlayer.cpp:1238-1285) - this project's headless
// CreateNewMap() Worlds never have one (no PIE, no viewport), the same class of
// limitation KrowdKontrolOvercrowdAudioVisualSyncTest.cpp's PlayerCameraManager
// bootstrap comment documents for a different subsystem, and exactly why
// IntersectRayWithGroundPlane above was deliberately factored out to need no
// viewport at all.
//
// So rather than calling GetCursorWorldPosition() itself (impossible headlessly),
// this test reconstructs the same real engine call chain by hand from this pawn's
// own live, gameplay-positioned TopDownCamera: build the FSceneViewProjectionData
// the same way ULocalPlayer::GetProjectionData() does (ViewOrigin/ViewRotationMatrix
// via FInverseRotationMatrix, then FMinimalViewInfo::CalculateProjectionMatrixGivenViewRectangle -
// the exact engine function GetProjectionData() calls, given an explicit view
// rectangle instead of a live FViewport*), then calls the same
// FSceneView::DeprojectScreenToWorld() that UGameplayStatics::DeprojectScreenToWorld()
// (and so DeprojectMousePositionToWorld()) calls internally, and feeds the resulting
// ray into IntersectRayWithGroundPlane() exactly as GetCursorWorldPosition() does.
//
// A square view rectangle plus AspectRatio_MaintainXFOV sidesteps the aspect-ratio
// correction branch entirely (XAxisMultiplier == YAxisMultiplier == 1), so both
// expected results below follow from CameraFieldOfView/CameraArmLength alone, with
// no dependency on the pawn's CameraBoomPitch value (the boom's Right axis is
// unaffected by pitch when Yaw/Roll are both 0, which ApplyCameraFraming() never
// changes) - see the per-case comments for the derivation.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolCursorScreenToWorldDeprojectionPipelineTest,
	"KrowdKontrol.Unit.CursorScreenToWorldDeprojectionPipeline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolCursorScreenToWorldDeprojectionPipelineTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	AFlatCamera3DPrototypePawn* Pawn = World->SpawnActor<AFlatCamera3DPrototypePawn>();
	if (!TestNotNull(TEXT("Pawn should spawn"), Pawn))
	{
		return false;
	}

	// This pawn's own real, gameplay-positioned camera - CameraBoom/TopDownCamera are
	// already wired by the constructor's ApplyCameraFraming() call, so this is exactly
	// the transform a live gameplay frame would have.
	FMinimalViewInfo ViewInfo;
	Pawn->TopDownCamera->GetCameraView(0.0f, ViewInfo);

	const FIntRect ViewRect(0, 0, 1000, 1000);
	FSceneViewProjectionData ProjectionData;
	ProjectionData.SetViewRectangle(ViewRect);
	ProjectionData.ViewOrigin = ViewInfo.Location;
	ProjectionData.ViewRotationMatrix = FInverseRotationMatrix(ViewInfo.Rotation) * FMatrix(
		FPlane(0, 0, 1, 0),
		FPlane(1, 0, 0, 0),
		FPlane(0, 1, 0, 0),
		FPlane(0, 0, 0, 1));
	FMinimalViewInfo::CalculateProjectionMatrixGivenViewRectangle(
		ViewInfo, AspectRatio_MaintainXFOV, ViewRect, ProjectionData);
	const FMatrix InvViewProjMatrix = ProjectionData.ComputeViewProjectionMatrix().InverseFast();

	// Case A: screen-space centre. This pawn's own CameraBoom always looks straight at
	// this pawn's own actor location (that's what a spring arm does), so the
	// centre-screen ray must land exactly there - regardless of FOV or arm length.
	{
		FVector RayOrigin, RayDirection;
		FSceneView::DeprojectScreenToWorld(FVector2D(500.0, 500.0), ViewRect, InvViewProjMatrix, RayOrigin, RayDirection);

		FVector OutWorldPosition;
		const bool bResult = AFlatCamera3DPrototypePawn::IntersectRayWithGroundPlane(
			RayOrigin, RayDirection, Pawn->GetActorLocation().Z, OutWorldPosition);

		TestTrue(TEXT("Centre-screen deprojection should reach the floor plane"), bResult);
		TestTrue(TEXT("Centre-screen deprojection should land on the pawn's own actor location"),
			OutWorldPosition.Equals(Pawn->GetActorLocation(), 2.0f));
	}

	// Case B: an off-centre screen point (3/4 of the way across, vertically centred -
	// ndc_x == 0.5, ndc_y == 0). This only lands on the expected result if
	// CameraFieldOfView genuinely drives the projection matrix this deprojection
	// uses - a hand-rolled projection matrix with wrong FOV/aspect handling (the HIGH
	// bug this file's Case A/B/C/D math cases were narrowed down from - see the "fix:
	// address review findings" commit) would fail this case while still passing Case
	// A above. Expected offset derivation: with Yaw/Roll == 0, the boom's Right axis
	// is always exactly the world Y axis regardless of pitch, so an ndc_x screen
	// offset moves the ray's floor intersection purely along world Y by
	// ArmLength * tan(HalfFOV) * ndc_x - the ray's floor-plane travel distance is
	// still exactly ArmLength (the boom's Right axis has no Z component either, so it
	// can't change how far the ray has to travel to reach the floor).
	{
		FVector RayOrigin, RayDirection;
		FSceneView::DeprojectScreenToWorld(FVector2D(750.0, 500.0), ViewRect, InvViewProjMatrix, RayOrigin, RayDirection);

		FVector OutWorldPosition;
		const bool bResult = AFlatCamera3DPrototypePawn::IntersectRayWithGroundPlane(
			RayOrigin, RayDirection, Pawn->GetActorLocation().Z, OutWorldPosition);

		TestTrue(TEXT("Off-centre deprojection should reach the floor plane"), bResult);

		const float HalfFOVRadians = FMath::DegreesToRadians(Pawn->CameraFieldOfView / 2.0f);
		const float ExpectedYOffset = Pawn->CameraArmLength * FMath::Tan(HalfFOVRadians) * 0.5f;
		const FVector ExpectedWorldPosition = Pawn->GetActorLocation() + FVector(0.0f, ExpectedYOffset, 0.0f);

		TestTrue(TEXT("Off-centre deprojection should offset along the camera's Right axis by the FOV-derived amount"),
			OutWorldPosition.Equals(ExpectedWorldPosition, 2.0f));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
