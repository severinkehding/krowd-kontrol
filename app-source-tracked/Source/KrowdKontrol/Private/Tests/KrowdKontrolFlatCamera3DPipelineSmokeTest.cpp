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
#include "GameFramework/PlayerController.h"
#include "AbilityUnlockComponent.h"
#include "EnemyBaseTestActor.h"
#include "AbilityCastVFXComponent.h"
#include "AbilityData.h"
#include "Components/PointLightComponent.h"
#include "PunishmentManagerComponent.h"
#include "SpeedReductionPunishmentComponent.h"
#include "AbilityLockoutComponent.h"
#include "AbilitySlot.h"
#include "PlayerEnergyComponent.h"
#include "PunishmentTriggeredTestListener.h"

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

	// Captured before any ApplyContactDamage call below - this pawn's real
	// AbilityLockoutComponent (issue #178) wins arbitration (issue #180) over
	// SpeedReductionPunishmentComponent (issue #179) on the shared
	// PunishmentManagerComponent trigger the very next block exercises, so MaxSpeed
	// should remain untouched by that block's ApplyContactDamage call.
	const float OriginalMovementSpeed = MovementComponent->MaxSpeed;

	// Proves this pawn's PunishmentManagerComponent (issue #177) is genuinely bound
	// to this same pawn's own PlayerEnergyComponent via constructor-time AddDynamic,
	// not a copy-paste slip binding to the other prototype pawn's instance - the
	// KrowdKontrol.Unit.PunishmentManagerComponent test alone can't catch this since
	// it wires the component by hand, never through a pawn constructor.
	UPunishmentManagerComponent* PunishmentManagerComponent = Pawn->PunishmentManagerComponent;
	if (TestNotNull(TEXT("Pawn should have a PunishmentManagerComponent"), PunishmentManagerComponent))
	{
		UPunishmentTriggeredTestListener* Listener = NewObject<UPunishmentTriggeredTestListener>();
		PunishmentManagerComponent->OnPunishmentTriggered.AddDynamic(Listener, &UPunishmentTriggeredTestListener::HandlePunishmentTriggered);

		Pawn->PlayerEnergyComponent->ApplyContactDamage(7.0f, nullptr);
		TestEqual(TEXT("Pawn's own PlayerEnergyComponent damage should trigger this pawn's own PunishmentManagerComponent"),
			Listener->CallCount, 1);
	}

	// Proves this pawn's AbilityLockoutComponent (issue #178) — not
	// SpeedReductionPunishmentComponent (issue #179) — is the one that actually activates
	// on this pawn's real contact-damage trigger, now that UPunishmentArbitrationComponent
	// (issue #180) arbitrates between them: ability-lock (priority 2) always wins the
	// shared OnPunishmentTriggered signal over speed-reduction (priority 3) whenever both
	// exist on the same pawn, per issue #180's own priority order. MaxSpeed must stay
	// untouched; Stun (the no-cast-yet fallback) must be locked instead.
	USpeedReductionPunishmentComponent* SpeedReductionComponent = Pawn->SpeedReductionPunishmentComponent;
	if (TestNotNull(TEXT("Pawn should have a SpeedReductionPunishmentComponent"), SpeedReductionComponent))
	{
		TestEqual(TEXT("Ability-lock should have won arbitration, so MaxSpeed should be untouched by this pawn's own contact damage above"),
			MovementComponent->MaxSpeed, OriginalMovementSpeed);
	}

	UAbilityLockoutComponent* LockoutComponent = Pawn->AbilityLockoutComponent;
	if (TestNotNull(TEXT("Pawn should have an AbilityLockoutComponent"), LockoutComponent))
	{
		TestTrue(TEXT("Ability-lock should have activated (Stun fallback) via arbitration from this pawn's own contact damage above"),
			LockoutComponent->IsAbilityLocked(EAbilitySlot::Stun));
	}

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

	// The 5 ability-cast action bindings (issue #138) - same existence-check shape as
	// MoveForward/MoveRight above, extended to the new "CastStun".."CastSnare" actions
	// so a BindAction name typo (mismatched against app/Config/DefaultInput.ini's actual
	// key, which this repo's harness can't otherwise verify - .ini files have no
	// app-source-tracked/ mirror) is at least localized to the .ini file alone rather
	// than compounding with a silently-unregistered C++ binding too.
	const TArray<FName> ExpectedCastActionNames = {
		TEXT("CastStun"), TEXT("CastSleep"), TEXT("CastRoot"), TEXT("CastFear"), TEXT("CastSnare")
	};
	for (const FName& ExpectedActionName : ExpectedCastActionNames)
	{
		bool bFound = false;
		for (int32 Index = 0; Index < InputComponent->GetNumActionBindings(); ++Index)
		{
			if (InputComponent->GetActionBinding(Index).GetActionName() == ExpectedActionName)
			{
				bFound = true;
				break;
			}
		}
		TestTrue(FString::Printf(TEXT("SetupPlayerInputComponent should bind a %s action"), *ExpectedActionName.ToString()), bFound);
	}

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

// Regression coverage for the acceptance criterion that WASD/arrow input actually
// moves the pawn at runtime, not just that the bound delegates accumulate a pending
// movement vector (which the assertions above already cover). Simulates one frame of
// held-forward input the way UPlayerInput::ProcessInputStack would invoke the bound
// delegate, then ticks UFloatingPawnMovement directly the way its own per-frame
// component tick would, and asserts the pawn's world location actually changed - the
// gap a live-MCP E2E validator could not close because Slate-level key injection
// doesn't reach the PIE viewport's gameplay input focus.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolFlatCamera3DPipelineMovementTest,
	"KrowdKontrol.Unit.FlatCamera3DPipelineMovement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolFlatCamera3DPipelineMovementTest::RunTest(const FString& Parameters)
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

	// UFloatingPawnMovement::TickComponent only applies pending input when the pawn has
	// a local controller (Engine/Private/FloatingPawnMovement.cpp) - CreateNewMap()'s
	// editor world has no player-start/GameMode flow to auto-possess AutoPossessPlayer,
	// so possess explicitly to exercise the same runtime gate a live PIE session hits.
	// AController itself is abstract (no concrete state machine), so use the concrete
	// APlayerController subclass rather than the base class.
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	if (!TestNotNull(TEXT("Should be able to spawn a controller to possess the pawn"), Controller))
	{
		return false;
	}
	Controller->Possess(Pawn);
	// This editor world has no GameInstance/ULocalPlayer flow to make IsLocalController()
	// true on its own (Engine/Private/PlayerController.cpp falls back to "does this
	// controller have a ULocalPlayer?" once GetNetDriver() is null) - SetAsLocalPlayerController()
	// is the same public setter APlayerController::SetPlayer() calls for a real local
	// player, so this exercises the identical runtime gate without needing a full
	// GameInstance/viewport setup this unit test has no use for otherwise.
	Controller->SetAsLocalPlayerController();

	UClass* InputComponentClass = UInputSettings::GetDefaultInputComponentClass();
	if (!TestNotNull(TEXT("Project should have a configured default InputComponent class"), InputComponentClass))
	{
		return false;
	}

	UInputComponent* InputComponent = NewObject<UInputComponent>(Pawn, InputComponentClass);
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
	TestFalse(TEXT("Placed pawn's CameraBoom should not collision-test, to avoid zooming through geometry"),
		PlacedPawn->CameraBoom->bDoCollisionTest);
	TestFalse(TEXT("Placed pawn's TopDownCamera rotation should also be locked, not player-controlled"),
		PlacedPawn->TopDownCamera->bUsePawnControlRotation);

	return true;
}

// Confirms each of the 5 private Cast*Ability wrappers (issue #138) forwards to its
// own EAbilitySlot and no other - the wrappers are structurally identical except for
// one enum value each, exactly the shape most prone to a silent copy-paste swap (e.g.
// Fear<->Root) that would compile cleanly and pass every other test in this PR. Each
// ability gets its own case with a fresh CreateNewMap() World, mirroring
// KrowdKontrolAbilityCastComponentTest.cpp's same per-case World isolation (that
// component's own FindNearestValidTarget() scans every AEnemyBase in the World, so
// reusing one World across cases would let an earlier case's already-Controlled enemy
// leak into a later case's target search).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolFlatCamera3DAbilityCastWiringTest,
	"KrowdKontrol.Unit.FlatCamera3DPrototypePawnAbilityCastWiring",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolFlatCamera3DAbilityCastWiringTest::RunTest(const FString& Parameters)
{
	// Level index each ability unlocks at, per AbilityUnlockComponent.cpp's
	// GetLevelToAbilityMap() (Stun is already unlocked at construction, level 1 is
	// never a real NotifyLevelReached call for it).
	struct FWiringCase
	{
		void (AFlatCamera3DPrototypePawn::*Wrapper)();
		EAbilitySlot ExpectedAbility;
		int32 UnlockLevel;
		const TCHAR* WrapperName;
	};
	const FWiringCase Cases[] = {
		{ &AFlatCamera3DPrototypePawn::CastStunAbility, EAbilitySlot::Stun, 1, TEXT("CastStunAbility") },
		{ &AFlatCamera3DPrototypePawn::CastSleepAbility, EAbilitySlot::Sleep, 2, TEXT("CastSleepAbility") },
		{ &AFlatCamera3DPrototypePawn::CastRootAbility, EAbilitySlot::Root, 3, TEXT("CastRootAbility") },
		{ &AFlatCamera3DPrototypePawn::CastFearAbility, EAbilitySlot::Fear, 4, TEXT("CastFearAbility") },
		{ &AFlatCamera3DPrototypePawn::CastSnareAbility, EAbilitySlot::Snare, 5, TEXT("CastSnareAbility") },
	};

	for (const FWiringCase& Case : Cases)
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
		// This editor test World never runs BeginPlay(), so AbilityCastVFXComponent's
		// CastFlashLightComponent must be driven deterministically here, same as
		// KrowdKontrolAbilityVFXColourTest.cpp does for its own standalone components.
		Pawn->AbilityCastVFXComponent->InitializeCastVFX();

		AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), Enemy))
		{
			return false;
		}
		Enemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert, within default CastRangeUnits

		Pawn->AbilityUnlockComponent->NotifyLevelReached(Case.UnlockLevel);

		(Pawn->*Case.Wrapper)();
		TestEqual(FString::Printf(TEXT("%s should apply its own EAbilitySlot specifically, not a neighboring slot"), Case.WrapperName),
			static_cast<uint8>(Enemy->GetControllingAbility()), static_cast<uint8>(Case.ExpectedAbility));
		TestEqual(FString::Printf(TEXT("%s should move the target enemy to Controlled"), Case.WrapperName),
			static_cast<uint8>(Enemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));

		// Confirm the pawn's own constructor-bound AbilityCastVFXComponent (not a
		// hand-wired stand-in, per KrowdKontrolAbilityVFXColourTest.cpp) actually
		// reacted to this real cast (issue #67).
		if (TestNotNull(FString::Printf(TEXT("%s: Pawn->AbilityCastVFXComponent->CastFlashLightComponent should exist"), Case.WrapperName),
			ToRawPtr(Pawn->AbilityCastVFXComponent->CastFlashLightComponent)))
		{
			TestTrue(FString::Printf(TEXT("%s should drive the pawn's real AbilityCastVFXComponent to the matching locked colour"), Case.WrapperName),
				Pawn->AbilityCastVFXComponent->CastFlashLightComponent->GetLightColor().Equals(
					AbilityData::Get(Case.ExpectedAbility).Colour, 0.01f));
		}
	}

	return true;
}

// Regression coverage for issue #188 (PRD "Level Playability & Presentation"
// REQ-4): the camera-framing properties (CameraArmLength/CameraBoomPitch/
// CameraFieldOfView) must (a) default within their documented ClampMin/ClampMax
// ranges, and (b) genuinely drive CameraBoom/TopDownCamera when changed - not sit
// decorative while the constructor's original hardcoded values silently persist
// on the components. Calls ApplyCameraFraming() directly rather than simulating a
// full Details-panel PostEditChangeProperty event: that function is the same one
// both the constructor and PostEditChangeProperty call, so this exercises real
// production wiring without needing to hand-construct an FPropertyChangedEvent.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolFlatCamera3DCameraFramingTest,
	"KrowdKontrol.Unit.FlatCamera3DPipelineCameraFraming",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolFlatCamera3DCameraFramingTest::RunTest(const FString& Parameters)
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

	// (a) Defaults land within the documented ranges (mirrors each property's own
	// ClampMin/ClampMax meta in FlatCamera3DPrototypePawn.h).
	TestTrue(TEXT("CameraArmLength default should be within the documented [300, 600] range"),
		Pawn->CameraArmLength >= 300.0f && Pawn->CameraArmLength <= 600.0f);
	TestTrue(TEXT("CameraBoomPitch default should be within the documented [-75, -45] range"),
		Pawn->CameraBoomPitch >= -75.0f && Pawn->CameraBoomPitch <= -45.0f);
	TestTrue(TEXT("CameraFieldOfView default should be within the documented [60, 90] range"),
		Pawn->CameraFieldOfView >= 60.0f && Pawn->CameraFieldOfView <= 90.0f);

	// Defaults should also already be closer/less extreme than the pre-#188 hardcoded
	// values (800cm / -80 degrees), per REQ-4's "closer than today's 800cm/-80" ask.
	TestTrue(TEXT("CameraArmLength default should be closer than the old 800cm hardcoded value"),
		Pawn->CameraArmLength < 800.0f);
	TestTrue(TEXT("CameraBoomPitch default should be less extreme than the old -80 degree hardcoded value"),
		Pawn->CameraBoomPitch > -80.0f);

	// (a continued) Defaults are also already correctly applied onto the live
	// components at spawn time, via the constructor's ApplyCameraFraming() call.
	TestEqual(TEXT("CameraBoom->TargetArmLength should equal CameraArmLength at spawn"),
		Pawn->CameraBoom->TargetArmLength, Pawn->CameraArmLength);
	TestEqual(TEXT("CameraBoom pitch should equal CameraBoomPitch at spawn"),
		static_cast<float>(Pawn->CameraBoom->GetRelativeRotation().Pitch), Pawn->CameraBoomPitch);
	TestEqual(TEXT("TopDownCamera FieldOfView should equal CameraFieldOfView at spawn"),
		Pawn->TopDownCamera->FieldOfView, Pawn->CameraFieldOfView);

	// (b) Changing a property and re-applying genuinely drives CameraBoom/
	// TopDownCamera - proves these are live wiring, not decorative UPROPERTYs.
	const float NewArmLength = 320.0f;
	const float NewPitch = -72.0f;
	const float NewFOV = 65.0f;
	Pawn->CameraArmLength = NewArmLength;
	Pawn->CameraBoomPitch = NewPitch;
	Pawn->CameraFieldOfView = NewFOV;
	Pawn->ApplyCameraFraming();

	TestEqual(TEXT("Changing CameraArmLength and re-applying should update CameraBoom->TargetArmLength"),
		Pawn->CameraBoom->TargetArmLength, NewArmLength);
	TestEqual(TEXT("Changing CameraBoomPitch and re-applying should update CameraBoom's relative pitch"),
		static_cast<float>(Pawn->CameraBoom->GetRelativeRotation().Pitch), NewPitch);
	TestEqual(TEXT("Changing CameraFieldOfView and re-applying should update TopDownCamera->FieldOfView"),
		Pawn->TopDownCamera->FieldOfView, NewFOV);

	// (c) PostEditChangeProperty itself (not just the shared ApplyCameraFraming()
	// helper) must route each camera property to a live reapply - this is the actual
	// mechanism a placed-instance Details-panel edit uses in the editor.
	const float DetailsPanelArmLength = 500.0f;
	Pawn->CameraArmLength = DetailsPanelArmLength;
	FProperty* ArmLengthProperty = FindFProperty<FProperty>(
		AFlatCamera3DPrototypePawn::StaticClass(),
		GET_MEMBER_NAME_CHECKED(AFlatCamera3DPrototypePawn, CameraArmLength));
	if (TestNotNull(TEXT("CameraArmLength should be a reflected FProperty"), ArmLengthProperty))
	{
		FPropertyChangedEvent ArmLengthChangedEvent(ArmLengthProperty);
		Pawn->PostEditChangeProperty(ArmLengthChangedEvent);
		TestEqual(TEXT("PostEditChangeProperty(CameraArmLength) should reapply onto CameraBoom->TargetArmLength"),
			Pawn->CameraBoom->TargetArmLength, DetailsPanelArmLength);
	}

	// Negative case: an unrelated property change should not be silently swallowed by
	// an over-broad filter that reapplies on every PostEditChangeProperty call.
	Pawn->CameraFieldOfView = 88.0f; // mutated but not yet reapplied
	FProperty* AutoPossessProperty = FindFProperty<FProperty>(
		APawn::StaticClass(), GET_MEMBER_NAME_CHECKED(APawn, AutoPossessPlayer));
	if (TestNotNull(TEXT("AutoPossessPlayer should be a reflected FProperty"), AutoPossessProperty))
	{
		FPropertyChangedEvent UnrelatedChangedEvent(AutoPossessProperty);
		Pawn->PostEditChangeProperty(UnrelatedChangedEvent);
		TestNotEqual(TEXT("PostEditChangeProperty for an unrelated property should not reapply CameraFieldOfView"),
			Pawn->TopDownCamera->FieldOfView, 88.0f);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
