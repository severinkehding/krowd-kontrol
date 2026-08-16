// Confirms APaper2DPrototypePawn (issue #55) spawns correctly and is wired the way
// PRD 14 REQ-1's Paper2D comparison needs: sprite root, movement component actually
// driving that root, and a camera boom genuinely locked to a straight-down
// orthographic view rather than player-adjustable or merely steeply pitched.
//
// Needs a real UWorld to spawn into (SpringArmComponent attachment/registration is
// safer exercised inside a spawned actor in a real world than via bare NewObject), so
// like the flat-camera-3D prototype's own smoke test
// (KrowdKontrolFlatCamera3DPipelineSmokeTest.cpp, already merged in this repo) this
// uses FAutomationEditorCommonUtils::CreateNewMap() rather than the NewObject-only
// approach KrowdKontrolPlaceholderCubeActorTest.cpp uses.
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
#include "EngineUtils.h"
#include "Editor.h"
#include "GameFramework/PlayerController.h"

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

	TestEqual(TEXT("TopDownCamera OrthoWidth should match the configured top-down framing"),
		TopDownCameraComponent->OrthoWidth, 1024.0f);

	TestFalse(TEXT("CameraBoom rotation should be locked, not player-controlled"),
		Pawn->CameraBoom->bUsePawnControlRotation);
	TestFalse(TEXT("CameraBoom should not collision-test, to avoid zooming through geometry"),
		Pawn->CameraBoom->bDoCollisionTest);
	TestFalse(TEXT("TopDownCamera rotation should also be locked, not player-controlled"),
		TopDownCameraComponent->bUsePawnControlRotation);

	// Confirms SetupPlayerInputComponent's BindAxis calls actually register AND that the
	// bound delegates actually fire and accumulate the expected world-space input, not
	// just that a binding with this name exists - mirrors
	// KrowdKontrolFlatCamera3DPipelineSmokeTest.cpp's own delegate-invocation assertion
	// (issue #56's post-review pattern).
	UInputComponent* InputComponent = NewObject<UInputComponent>(Pawn);
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

	TestTrue(TEXT("SetupPlayerInputComponent should bind a MoveForward axis"), MoveForwardBinding != nullptr);
	TestTrue(TEXT("SetupPlayerInputComponent should bind a MoveRight axis"), MoveRightBinding != nullptr);

	// Invokes the bound delegates directly (as UPlayerInput::ProcessInputStack would each
	// frame) and checks the deliberate world-space-vs-actor-relative design
	// Paper2DPrototypePawn.cpp's MoveForward()/MoveRight() call out in their own comment:
	// AddMovementInput(FVector::ForwardVector/RightVector, Value) accumulates into the
	// pawn's pending movement input in world space, so a plausible-looking "fix" to
	// actor-relative movement (GetActorForwardVector()) would change this result and
	// fail here.
	if (MoveForwardBinding && MoveRightBinding)
	{
		Pawn->ConsumeMovementInputVector();
		MoveForwardBinding->AxisDelegate.Execute(1.0f);
		MoveRightBinding->AxisDelegate.Execute(1.0f);

		const FVector PendingInput = Pawn->GetPendingMovementInputVector();
		TestEqual(TEXT("MoveForward/MoveRight should accumulate world-space ForwardVector + RightVector, not actor-relative"),
			PendingInput, FVector::ForwardVector + FVector::RightVector);
	}

	return true;
}

// Regression coverage for the acceptance criterion that L_Paper2DPrototype.umap itself
// (not just the pawn class in a throwaway map) contains a correctly-configured placed
// pawn instance. CreateNewMap()-based tests above prove the class works but can't catch
// a future edit to the shipped level (e.g. an instance-level override resetting the
// boom pitch) - only opening the real map can. Mirrors
// KrowdKontrolFlatCamera3DPipelineSmokeTest.cpp's own level test (issue #56's
// post-review pattern).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolPaper2DPipelineLevelTest,
	"KrowdKontrol.Unit.Paper2DPipelineLevelHasConfiguredPawn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolPaper2DPipelineLevelTest::RunTest(const FString& Parameters)
{
	FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/Maps/L_Paper2DPrototype"));

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("L_Paper2DPrototype should load into a valid World"), World))
	{
		return false;
	}

	APaper2DPrototypePawn* PlacedPawn = nullptr;
	for (TActorIterator<APaper2DPrototypePawn> It(World); It; ++It)
	{
		PlacedPawn = *It;
		break;
	}

	if (!TestNotNull(TEXT("L_Paper2DPrototype should contain a placed APaper2DPrototypePawn"), PlacedPawn))
	{
		return false;
	}

	TestTrue(TEXT("Placed pawn's CameraBoom pitch should be genuinely top-down (<= -45 degrees)"),
		PlacedPawn->CameraBoom->GetRelativeRotation().Pitch <= -45.0f);
	TestFalse(TEXT("Placed pawn's CameraBoom rotation should be locked, not player-controlled"),
		PlacedPawn->CameraBoom->bUsePawnControlRotation);
	TestFalse(TEXT("Placed pawn's CameraBoom should not collision-test, to avoid zooming through geometry"),
		PlacedPawn->CameraBoom->bDoCollisionTest);
	TestFalse(TEXT("Placed pawn's TopDownCamera rotation should also be locked, not player-controlled"),
		PlacedPawn->TopDownCamera->bUsePawnControlRotation);
	TestTrue(TEXT("Placed pawn's SpriteComponent should be rotated into the ground plane (<= -45 degrees pitch)"),
		PlacedPawn->SpriteComponent->GetRelativeRotation().Pitch <= -45.0f);
	TestEqual(TEXT("Placed pawn's TopDownCamera should use orthographic projection"),
		PlacedPawn->TopDownCamera->ProjectionMode, ECameraProjectionMode::Orthographic);

	return true;
}

// Regression coverage for the acceptance criterion that WASD/arrow input actually
// moves the pawn at runtime, not just that the bound delegates accumulate a pending
// movement vector (which KrowdKontrol.Unit.Paper2DPipelineSmoke above already covers).
// Simulates one frame of held-forward input the way UPlayerInput::ProcessInputStack
// would invoke the bound delegate, then ticks UFloatingPawnMovement directly the way
// its own per-frame component tick would, and asserts the pawn's world location
// actually changed. Mirrors KrowdKontrolFlatCamera3DPipelineSmokeTest.cpp's
// KrowdKontrol.Unit.FlatCamera3DPipelineMovement test.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolPaper2DPipelineMovementTest,
	"KrowdKontrol.Unit.Paper2DPipelineMovement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolPaper2DPipelineMovementTest::RunTest(const FString& Parameters)
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

	// UFloatingPawnMovement::TickComponent only applies pending input when the pawn has
	// a local controller (Engine/Private/FloatingPawnMovement.cpp) - CreateNewMap()'s
	// editor world has no player-start/GameMode flow to auto-possess AutoPossessPlayer,
	// so possess explicitly to exercise the same runtime gate a live PIE session hits.
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	if (!TestNotNull(TEXT("Should be able to spawn a controller to possess the pawn"), Controller))
	{
		return false;
	}
	Controller->Possess(Pawn);
	Controller->SetAsLocalPlayerController();

	UInputComponent* InputComponent = NewObject<UInputComponent>(Pawn);
	InputComponent->RegisterComponent();
	Pawn->SetupPlayerInputComponent(InputComponent);

	FInputAxisBinding* MoveForwardBinding = nullptr;
	for (FInputAxisBinding& Binding : InputComponent->AxisBindings)
	{
		if (Binding.AxisName == TEXT("MoveForward"))
		{
			MoveForwardBinding = &Binding;
			break;
		}
	}

	if (!TestTrue(TEXT("SetupPlayerInputComponent should bind a MoveForward axis"), MoveForwardBinding != nullptr))
	{
		return false;
	}

	const FVector StartLocation = Pawn->GetActorLocation();

	MoveForwardBinding->AxisDelegate.Execute(1.0f);
	Pawn->MovementComponent->TickComponent(0.1f, LEVELTICK_All, nullptr);

	const FVector EndLocation = Pawn->GetActorLocation();
	TestTrue(TEXT("Pawn's world location should change after simulated forward input and a movement tick"),
		!EndLocation.Equals(StartLocation));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
