// Confirms APaper2DPrototypePawn (issue #55) spawns correctly and is wired the way
// PRD 14 REQ-1's Paper2D comparison needs: sprite root, movement component actually
// driving that root, and a camera boom genuinely locked to a straight-down
// orthographic view rather than player-adjustable or merely steeply pitched.
//
// Needs a real UWorld to spawn into (SpringArmComponent attachment/registration is
// safer exercised inside a spawned actor in a real world than via bare NewObject), so
// like the flat-camera-3D prototype's own smoke test (issue #56; note: #56's PR was
// not merged, so that file isn't in this tracked repo - see docs/paper2d-prototype-
// notes.md) this uses FAutomationEditorCommonUtils::CreateNewMap() rather than the
// NewObject-only approach KrowdKontrolPlaceholderCubeActorTest.cpp uses.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "Paper2DPrototypePawn.h"
#include "PaperSpriteComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolPaper2DPipelineSmokeTest,
	"KrowdKontrol.Unit.Paper2DPipelineSmoke",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolPaper2DPipelineSmokeTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	APaper2DPrototypePawn* Pawn = World->SpawnActor<APaper2DPrototypePawn>();
	if (!TestNotNull(TEXT("APaper2DPrototypePawn should spawn into the test World"), Pawn))
	{
		return false;
	}

	UPaperSpriteComponent* SpriteComponent = Pawn->SpriteComponent;
	UFloatingPawnMovement* MovementComponent = Pawn->MovementComponent;
	USpringArmComponent* CameraBoomComponent = Pawn->CameraBoom;
	UCameraComponent* TopDownCameraComponent = Pawn->TopDownCamera;

	if (!TestNotNull(TEXT("Pawn should have a SpriteComponent"), SpriteComponent))
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

	TestEqual(TEXT("SpriteComponent should be the pawn's root component"),
		Pawn->GetRootComponent(), static_cast<USceneComponent*>(SpriteComponent));

	TestEqual(TEXT("MovementComponent should drive the sprite root component"),
		static_cast<USceneComponent*>(MovementComponent->UpdatedComponent),
		static_cast<USceneComponent*>(SpriteComponent));

	TestTrue(TEXT("CameraBoom pitch should be genuinely top-down (<= -45 degrees), not side-on"),
		Pawn->CameraBoom->GetRelativeRotation().Pitch <= -45.0f);

	TestTrue(TEXT("SpriteComponent should be rotated into the ground plane (<= -45 degrees pitch), not edge-on"),
		SpriteComponent->GetRelativeRotation().Pitch <= -45.0f);

	TestEqual(TEXT("TopDownCamera should use orthographic projection"),
		TopDownCameraComponent->ProjectionMode, ECameraProjectionMode::Orthographic);

	TestTrue(TEXT("TopDownCamera OrthoWidth should be a positive, non-degenerate value"),
		TopDownCameraComponent->OrthoWidth > 0.0f);

	TestFalse(TEXT("CameraBoom rotation should be locked, not player-controlled"),
		Pawn->CameraBoom->bUsePawnControlRotation);

	// Confirms SetupPlayerInputComponent's BindAxis calls actually register, not just
	// that the pawn has an InputComponent - same concrete signal the flat-camera-3D
	// prototype's own smoke test (issue #56, unmerged - see docs/paper2d-prototype-
	// notes.md) uses for its own Enhanced-Input-vs-legacy-BindAxis compatibility
	// question.
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
