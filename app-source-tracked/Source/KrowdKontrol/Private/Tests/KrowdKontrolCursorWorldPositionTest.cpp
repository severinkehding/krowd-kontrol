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

#endif // WITH_DEV_AUTOMATION_TESTS
