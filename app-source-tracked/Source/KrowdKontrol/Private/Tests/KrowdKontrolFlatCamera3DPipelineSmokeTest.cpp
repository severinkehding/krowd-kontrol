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
#include "Engine/StaticMesh.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/InputSettings.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Editor.h"

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

	UStaticMesh* StaticMesh = MeshComponent->GetStaticMesh();
	if (!TestNotNull(TEXT("MeshComponent should have a static mesh assigned"), StaticMesh))
	{
		return false;
	}
	TestEqual(TEXT("MeshComponent should use the engine's default cube mesh"),
		StaticMesh->GetPathName(), FString(TEXT("/Engine/BasicShapes/Cube.Cube")));

	TestEqual(TEXT("MovementComponent should drive the mesh root component"),
		static_cast<USceneComponent*>(MovementComponent->UpdatedComponent),
		static_cast<USceneComponent*>(MeshComponent));

	TestTrue(TEXT("CameraBoom pitch should be genuinely top-down (<= -45 degrees), not side-on"),
		Pawn->CameraBoom->GetRelativeRotation().Pitch <= -45.0f);

	TestFalse(TEXT("CameraBoom rotation should be locked, not player-controlled"),
		Pawn->CameraBoom->bUsePawnControlRotation);
	TestFalse(TEXT("CameraBoom should not collision-test, to avoid zooming through geometry"),
		Pawn->CameraBoom->bDoCollisionTest);
	TestFalse(TEXT("TopDownCamera rotation should also be locked, not player-controlled"),
		Pawn->TopDownCamera->bUsePawnControlRotation);

	// Constructs against the project's actual configured input component class
	// (UInputSettings::GetDefaultInputComponentClass(), resolving to
	// UEnhancedInputComponent per DefaultInput.ini) rather than a bare UInputComponent,
	// mirroring exactly how APawn::CreatePlayerInputComponent() constructs the real one
	// (Engine/Private/Pawn.cpp). This closes the gap the previous version of this test
	// left open: legacy BindAxis() is now proven to register against the same concrete
	// class the project actually uses at runtime, not just the legacy base class.
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

	TestTrue(TEXT("SetupPlayerInputComponent should bind a MoveForward axis"), MoveForwardBinding != nullptr);
	TestTrue(TEXT("SetupPlayerInputComponent should bind a MoveRight axis"), MoveRightBinding != nullptr);

	// Invokes the bound delegates directly (as UPlayerInput::ProcessInputStack would
	// each frame) and checks the deliberate world-space-vs-actor-relative design
	// FlatCamera3DPrototypePawn.cpp's MoveForward()/MoveRight() call out in their own
	// comment: AddMovementInput(FVector::ForwardVector/RightVector, Value) accumulates
	// into the pawn's pending movement input in world space, so a plausible-looking
	// "fix" to actor-relative movement (GetActorForwardVector()) would change this
	// result and fail here.
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

// Regression coverage for the acceptance criterion that
// L_FlatCamera3DPrototype.umap itself (not just the pawn class in a throwaway map)
// contains a correctly-configured placed pawn instance. CreateNewMap()-based tests
// above prove the class works but can't catch a future edit to the shipped level
// (e.g. an instance-level override resetting the boom pitch) - only opening the real
// map can.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolFlatCamera3DPipelineLevelTest,
	"KrowdKontrol.Unit.FlatCamera3DPipelineLevelHasConfiguredPawn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolFlatCamera3DPipelineLevelTest::RunTest(const FString& Parameters)
{
	FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/Maps/L_FlatCamera3DPrototype"));

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("L_FlatCamera3DPrototype should load into a valid World"), World))
	{
		return false;
	}

	AFlatCamera3DPrototypePawn* PlacedPawn = nullptr;
	for (TActorIterator<AFlatCamera3DPrototypePawn> It(World); It; ++It)
	{
		PlacedPawn = *It;
		break;
	}

	if (!TestNotNull(TEXT("L_FlatCamera3DPrototype should contain a placed AFlatCamera3DPrototypePawn"), PlacedPawn))
	{
		return false;
	}

	TestTrue(TEXT("Placed pawn's CameraBoom pitch should be genuinely top-down (<= -45 degrees)"),
		PlacedPawn->CameraBoom->GetRelativeRotation().Pitch <= -45.0f);
	TestFalse(TEXT("Placed pawn's CameraBoom rotation should be locked, not player-controlled"),
		PlacedPawn->CameraBoom->bUsePawnControlRotation);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
