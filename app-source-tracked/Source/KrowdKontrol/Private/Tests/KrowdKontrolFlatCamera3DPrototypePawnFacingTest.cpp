// Confirms the acceptance criteria of issue #263 (PRD "Cursor & Aiming Foundation"
// REQ-2): AFlatCamera3DPrototypePawn's yaw updates every tick to face the cursor's
// current world position, while WASD movement stays exactly as it is today -
// world-relative, unaffected by facing.
//
// Test 1 needs no UWorld at all - ComputeFacingRotation() takes plain FVector/FRotator
// data, matching IntersectRayWithGroundPlane()'s own pure-math test shape in
// KrowdKontrolCursorWorldPositionTest.cpp.
//
// Test 2 needs a real UWorld (CreateNewMap()) to spawn the pawn into, and proves
// Tick() is wired to GetCursorWorldPosition() and no-ops gracefully (no crash, no
// rotation) when there's no possessing controller/viewport - the same negative-path
// limitation FKrowdKontrolFlatCamera3DPrototypePawnCursorWorldPositionTest already
// documents for GetCursorWorldPosition() itself. Tick()'s own call to
// GetCursorWorldPosition() stays untestable here for the same reason (no live
// viewport in a headless CreateNewMap() world) - but the rest of the per-frame
// wiring (ComputeFacingRotation() -> SetActorRotation()) is split into
// ApplyFacingTowardCursor() and exercised directly with a fixed cursor position by
// Test 3 below, so a broken "apply" step is still caught even though the
// "acquire a cursor position" step can't be.
//
// Test 3 calls ApplyFacingTowardCursor() directly with a known cursor position and
// asserts the resulting rotation, proving the wiring Tick() dispatches to
// (ComputeFacingRotation() -> SetActorRotation()) end-to-end rather than only its
// two halves in isolation.
//
// Test 4 reuses the AxisDelegate.Execute() trick from
// KrowdKontrolFlatCamera3DPipelineSmokeTest.cpp to invoke the private
// MoveForward/MoveRight without a friend declaration, after rotating the pawn away
// from its spawn default, proving WASD input/velocity is identical regardless of
// facing.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "FlatCamera3DPrototypePawn.h"
#include "Engine/World.h"
#include "Components/InputComponent.h"
#include "GameFramework/InputSettings.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolFlatCamera3DFacingRotationMathTest,
	"KrowdKontrol.Unit.FlatCamera3DPrototypePawnFacingRotationMath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolFlatCamera3DFacingRotationMathTest::RunTest(const FString& Parameters)
{
	// Cursor directly ahead on +X.
	{
		FRotator OutFacingRotation;
		const bool bResult = AFlatCamera3DPrototypePawn::ComputeFacingRotation(
			FVector(0.0f, 0.0f, 0.0f), FVector(100.0f, 0.0f, 0.0f), OutFacingRotation);

		TestTrue(TEXT("Cursor on +X should succeed"), bResult);
		TestEqual(TEXT("Cursor on +X should yield Yaw 0"), OutFacingRotation.Yaw, 0.0);
	}

	// Cursor on +Y.
	{
		FRotator OutFacingRotation;
		const bool bResult = AFlatCamera3DPrototypePawn::ComputeFacingRotation(
			FVector(0.0f, 0.0f, 0.0f), FVector(0.0f, 100.0f, 0.0f), OutFacingRotation);

		TestTrue(TEXT("Cursor on +Y should succeed"), bResult);
		TestEqual(TEXT("Cursor on +Y should yield Yaw 90"), OutFacingRotation.Yaw, 90.0);
	}

	// Cursor on -X.
	{
		FRotator OutFacingRotation;
		const bool bResult = AFlatCamera3DPrototypePawn::ComputeFacingRotation(
			FVector(0.0f, 0.0f, 0.0f), FVector(-100.0f, 0.0f, 0.0f), OutFacingRotation);

		TestTrue(TEXT("Cursor on -X should succeed"), bResult);
		TestEqual(TEXT("Cursor on -X should yield Yaw 180"), OutFacingRotation.Yaw, 180.0);
	}

	// Off-axis (45 degree diagonal).
	{
		FRotator OutFacingRotation;
		const bool bResult = AFlatCamera3DPrototypePawn::ComputeFacingRotation(
			FVector(500.0f, 500.0f, 50.0f), FVector(600.0f, 600.0f, 50.0f), OutFacingRotation);

		TestTrue(TEXT("Off-axis cursor should succeed"), bResult);
	}

	// Dead zone (PR #279 review): a cursor within the 10-unit facing dead zone
	// must not produce a rotation - pins the gameplay-units threshold that
	// replaced the ~0.1mm KINDA_SMALL_NUMBER guard.
	{
		FRotator OutFacingRotation = FRotator::ZeroRotator;
		const bool bResult = AFlatCamera3DPrototypePawn::ComputeFacingRotation(
			FVector(0.0f, 0.0f, 0.0f), FVector(5.0f, 5.0f, 0.0f), OutFacingRotation);
		TestFalse(TEXT("Cursor inside the facing dead zone should not rotate"), bResult);
		TestEqual(TEXT("Off-axis cursor should yield Yaw 45"), OutFacingRotation.Yaw, 45.0);
	}

	// Cursor on -Y.
	{
		FRotator OutFacingRotation;
		const bool bResult = AFlatCamera3DPrototypePawn::ComputeFacingRotation(
			FVector(0.0f, 0.0f, 0.0f), FVector(0.0f, -100.0f, 0.0f), OutFacingRotation);

		TestTrue(TEXT("Cursor on -Y should succeed"), bResult);
		TestEqual(TEXT("Cursor on -Y should yield Yaw -90"), OutFacingRotation.Yaw, -90.0);
	}

	// Degenerate: same X/Y, different Z - yaw undefined, must fail cleanly.
	{
		FRotator OutFacingRotation;
		const bool bResult = AFlatCamera3DPrototypePawn::ComputeFacingRotation(
			FVector(100.0f, 100.0f, 50.0f), FVector(100.0f, 100.0f, 999.0f), OutFacingRotation);

		TestFalse(TEXT("Cursor coincident with actor on X/Y should fail"), bResult);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolFlatCamera3DFacingTickNoCursorTest,
	"KrowdKontrol.Unit.FlatCamera3DPrototypePawnFacingTickNoCursor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolFlatCamera3DFacingTickNoCursorTest::RunTest(const FString& Parameters)
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

	// No possessing controller/viewport, so GetCursorWorldPosition() fails and
	// Tick() should no-op rather than crash or rotate.
	Pawn->Tick(0.1f);

	TestEqual(TEXT("Tick() with no cursor should leave rotation at default"),
		Pawn->GetActorRotation(), FRotator::ZeroRotator);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolFlatCamera3DFacingTickAppliesRotationTest,
	"KrowdKontrol.Unit.FlatCamera3DPrototypePawnFacingTickAppliesRotation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolFlatCamera3DFacingTickAppliesRotationTest::RunTest(const FString& Parameters)
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

	// Cursor directly on +Y relative to the pawn's spawn location - expect Yaw 90.
	// Exercises the same ComputeFacingRotation() -> SetActorRotation() wiring Tick()
	// dispatches to each frame, without needing a live viewport to reach it.
	Pawn->ApplyFacingTowardCursor(Pawn->GetActorLocation() + FVector(0.0f, 100.0f, 0.0f));

	TestEqual(TEXT("ApplyFacingTowardCursor should rotate the pawn to face the given cursor position"),
		Pawn->GetActorRotation().Yaw, 90.0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolFlatCamera3DFacingDoesNotAffectMovementTest,
	"KrowdKontrol.Unit.FlatCamera3DPrototypePawnFacingDoesNotAffectMovement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolFlatCamera3DFacingDoesNotAffectMovementTest::RunTest(const FString& Parameters)
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

	// Simulate a rotated facing (as Tick() would produce toward some cursor position).
	Pawn->SetActorRotation(FRotator(0.0f, 90.0f, 0.0f));

	UClass* InputComponentClass = UInputSettings::GetDefaultInputComponentClass();
	if (!TestNotNull(TEXT("Project should have a configured default InputComponent class"), InputComponentClass))
	{
		return false;
	}

	UInputComponent* InputComponent = NewObject<UInputComponent>(Pawn, InputComponentClass);
	InputComponent->RegisterComponent();
	Pawn->SetupPlayerInputComponent(InputComponent);

	FInputAxisBinding* MoveForwardBinding = nullptr;
	FInputAxisBinding* MoveRightBinding = nullptr;
	for (FInputAxisBinding& Binding : InputComponent->AxisBindings)
	{
		if (Binding.AxisName == TEXT("MoveForward"))
		{
			MoveForwardBinding = &Binding;
		}
		else if (Binding.AxisName == TEXT("MoveRight"))
		{
			MoveRightBinding = &Binding;
		}
	}

	if (!TestTrue(TEXT("SetupPlayerInputComponent should bind a MoveForward axis"), MoveForwardBinding != nullptr) ||
		!TestTrue(TEXT("SetupPlayerInputComponent should bind a MoveRight axis"), MoveRightBinding != nullptr))
	{
		return false;
	}

	Pawn->ConsumeMovementInputVector();
	MoveForwardBinding->AxisDelegate.Execute(1.0f);
	MoveRightBinding->AxisDelegate.Execute(1.0f);

	const FVector PendingInput = Pawn->GetPendingMovementInputVector();
	TestEqual(TEXT("MoveForward/MoveRight should accumulate world-space ForwardVector + RightVector even with a non-default facing"),
		PendingInput, FVector::ForwardVector + FVector::RightVector);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
