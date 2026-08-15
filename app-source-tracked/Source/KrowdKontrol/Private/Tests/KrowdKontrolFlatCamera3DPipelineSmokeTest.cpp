// Confirms AFlatCamera3DPrototypePawn (issue #56) spawns correctly and is wired the
// way PRD 14 REQ-1's flat-camera-3D comparison needs: mesh root, movement component
// actually driving that root, and a camera boom genuinely locked to a top-down pitch
// rather than player-adjustable.
//
// Needs a real UWorld to spawn into (SpringArmComponent attachment/registration is
// safer exercised inside a spawned actor in a real world than via bare NewObject), so
// like KrowdKontrolRoomEnemyBudgetControllerTest.cpp this uses
// FAutomationEditorCommonUtils::CreateNewMap() rather than the NewObject-only approach
// KrowdKontrolPlaceholderCubeActorTest.cpp uses.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "FlatCamera3DPrototypePawn.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolFlatCamera3DPipelineSmokeTest,
	"KrowdKontrol.Unit.FlatCamera3DPipelineSmoke",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolFlatCamera3DPipelineSmokeTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	AFlatCamera3DPrototypePawn* Pawn = World->SpawnActor<AFlatCamera3DPrototypePawn>();
	if (!TestNotNull(TEXT("AFlatCamera3DPrototypePawn should spawn into the test World"), Pawn))
	{
		return false;
	}

	UStaticMeshComponent* MeshComponent = Pawn->MeshComponent;
	UFloatingPawnMovement* MovementComponent = Pawn->MovementComponent;
	USpringArmComponent* CameraBoomComponent = Pawn->CameraBoom;
	UCameraComponent* TopDownCameraComponent = Pawn->TopDownCamera;

	if (!TestNotNull(TEXT("Pawn should have a MeshComponent"), MeshComponent))
	{
		return false;
	}
	if (!TestNotNull(TEXT("Pawn should have a MovementComponent"), MovementComponent))
	{
		return false;
	}
	if (!TestNotNull(TEXT("Pawn should have a CameraBoom"), CameraBoomComponent))
	{
		return false;
	}
	if (!TestNotNull(TEXT("Pawn should have a TopDownCamera"), TopDownCameraComponent))
	{
		return false;
	}

	TestEqual(TEXT("MeshComponent should be the pawn's root component"),
		Pawn->GetRootComponent(), static_cast<USceneComponent*>(MeshComponent));

	TestEqual(TEXT("MovementComponent should drive the mesh root component"),
		static_cast<USceneComponent*>(MovementComponent->UpdatedComponent),
		static_cast<USceneComponent*>(MeshComponent));

	TestTrue(TEXT("CameraBoom pitch should be genuinely top-down (<= -45 degrees), not side-on"),
		Pawn->CameraBoom->GetRelativeRotation().Pitch <= -45.0f);

	TestFalse(TEXT("CameraBoom rotation should be locked, not player-controlled"),
		Pawn->CameraBoom->bUsePawnControlRotation);

	// Confirms SetupPlayerInputComponent's BindAxis calls actually register, not just
	// that the pawn has an InputComponent - this is the concrete signal for the
	// Enhanced-Input-vs-legacy-BindAxis compatibility question docs/flat-camera-3d-prototype-notes.md
	// leaves open.
	UInputComponent* InputComponent = NewObject<UInputComponent>(Pawn);
	InputComponent->RegisterComponent();
	Pawn->SetupPlayerInputComponent(InputComponent);

	bool bHasMoveForwardBinding = false;
	bool bHasMoveRightBinding = false;
	for (const FInputAxisBinding& Binding : InputComponent->AxisBindings)
	{
		bHasMoveForwardBinding |= (Binding.AxisName == TEXT("MoveForward"));
		bHasMoveRightBinding |= (Binding.AxisName == TEXT("MoveRight"));
	}

	TestTrue(TEXT("SetupPlayerInputComponent should bind a MoveForward axis"), bHasMoveForwardBinding);
	TestTrue(TEXT("SetupPlayerInputComponent should bind a MoveRight axis"), bHasMoveRightBinding);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
