// Confirms UTeachingPromptComponent (issue #219, PRD "Level Progression & Teaching
// Arc" REQ-3's core-loop half): four one-shot Level-1-only on-screen prompts
// ("STUN IT — PRESS 1", "IT FOLLOWS YOU — WALK", "DROP IT ON THE GLOWING PEN",
// "ROOM CLEAR — DOOR OPEN"), each firing/dismissing off a real gameplay signal, never
// re-firing once its guard is set.
//
// Per this module's established convention, tests drive the component's private
// friend-testable Check*/Handle* methods directly rather than a real per-frame
// TickComponent() loop - see FKrowdKontrolTeachingPromptComponentTest's friendship
// grant on UTeachingPromptComponent and on AEnemyBase (EnemyBase.h).
//
// Each case uses its own FAutomationEditorCommonUtils::CreateNewMap() World, per this
// module's established per-scenario isolation convention (see
// KrowdKontrolAbilityMatchupNudgeComponentTest.cpp).
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "TeachingPromptComponent.h"
#include "KrowdKontrolPlayerController.h"
#include "OnScreenPromptWidget.h"
#include "FlatCamera3DPrototypePawn.h"
#include "EnemyBaseTestActor.h"
#include "RoomActor.h"
#include "TargetZone.h"
#include "AbilitySlot.h"
#include "AbilityUnlockLevelSubsystem.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Components/InputComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolTeachingPromptComponentTest,
	"KrowdKontrol.Unit.TeachingPromptComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace KrowdKontrolTeachingPromptComponentTest
{
	// Mirrors KrowdKontrolAbilityMatchupNudgeComponentTest.cpp's
	// SpawnControllerWithPromptWidget() exactly - see that file's own comment for why
	// World->AddController() must be called explicitly in a bare CreateNewMap() world.
	AKrowdKontrolPlayerController* SpawnControllerWithPromptWidget(UWorld* World)
	{
		AKrowdKontrolPlayerController* Controller = World->SpawnActor<AKrowdKontrolPlayerController>();
		if (!Controller)
		{
			return nullptr;
		}
		Controller->Player = NewObject<ULocalPlayer>(GEngine);
		Controller->SetAsLocalPlayerController();
		World->AddController(Controller);
		Controller->DispatchBeginPlay();
		return Controller;
	}
}

bool FKrowdKontrolTeachingPromptComponentTest::RunTest(const FString& Parameters)
{
	using namespace KrowdKontrolTeachingPromptComponentTest;

	// (a) Stun prompt fires on the first hot enemy, dismisses on a Stun cast.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}

		AKrowdKontrolPlayerController* Controller = SpawnControllerWithPromptWidget(World);
		if (!TestNotNull(TEXT("Controller should spawn"), Controller) ||
			!TestNotNull(TEXT("Controller should own a live OnScreenPromptWidgetInstance"), ToRawPtr(Controller->OnScreenPromptWidgetInstance)))
		{
			return false;
		}

		APawn* Owner = World->SpawnActor<APawn>();
		UTeachingPromptComponent* Component = NewObject<UTeachingPromptComponent>(Owner);
		Component->RegisterComponent();
		Component->BeginPlay();
		TestTrue(TEXT("Component should resolve as Level 1 for CreateNewMap()'s synthetic map name"), Component->bIsLevel1);

		AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), Enemy))
		{
			return false;
		}
		Enemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert (Hot)

		Component->CheckStunPromptFireCondition();
		TestTrue(TEXT("Stun prompt should fire on the first hot enemy"),
			Controller->OnScreenPromptWidgetInstance->IsPromptVisible());
		TestEqual(TEXT("Stun prompt text should be the STUN cue"),
			Controller->OnScreenPromptWidgetInstance->GetPromptDisplayText().ToString(),
			TEXT("STUN IT — PRESS 1"));

		Component->HandleAbilityCastApplied(EAbilitySlot::Stun, Enemy);
		TestTrue(TEXT("Stun prompt dismiss guard should be set after a Stun cast"), Component->bHasDismissedStunPrompt);
	}

	// (b) Control prompt fires on the first successful cast of any ability, text is right.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}

		AKrowdKontrolPlayerController* Controller = SpawnControllerWithPromptWidget(World);
		if (!TestNotNull(TEXT("Controller should spawn"), Controller) ||
			!TestNotNull(TEXT("Controller should own a live OnScreenPromptWidgetInstance"), ToRawPtr(Controller->OnScreenPromptWidgetInstance)))
		{
			return false;
		}

		APawn* Owner = World->SpawnActor<APawn>();
		UTeachingPromptComponent* Component = NewObject<UTeachingPromptComponent>(Owner);
		Component->RegisterComponent();
		Component->BeginPlay();

		AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), Enemy))
		{
			return false;
		}

		Component->HandleAbilityCastApplied(EAbilitySlot::Sleep, Enemy);
		TestTrue(TEXT("Control prompt fire guard should be set on the first successful cast, unfiltered by ability"),
			Component->bHasFiredControlPrompt);
		TestEqual(TEXT("Control prompt text should be the FOLLOW cue"),
			Controller->OnScreenPromptWidgetInstance->GetPromptDisplayText().ToString(),
			TEXT("IT FOLLOWS YOU — WALK"));
		TestEqual(TEXT("FirstControlledEnemy should now point at the cast's target"),
			Component->FirstControlledEnemy.Get(), static_cast<AEnemyBase*>(Enemy));
	}

	// (c) Control prompt dismisses on player movement while the tracked enemy is
	// Controlled; never dismisses if the tracked enemy isn't Controlled, even with movement.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}

		AFlatCamera3DPrototypePawn* Pawn = World->SpawnActor<AFlatCamera3DPrototypePawn>();
		if (!TestNotNull(TEXT("AFlatCamera3DPrototypePawn should spawn into the test World"), Pawn) ||
			!TestNotNull(TEXT("The real pawn's TeachingPromptComponent should be constructed"), ToRawPtr(Pawn->TeachingPromptComponent)))
		{
			return false;
		}
		UTeachingPromptComponent* Component = Pawn->TeachingPromptComponent;
		Component->BeginPlay();

		// UFloatingPawnMovement::TickComponent only applies pending input when the pawn
		// has a local controller - mirrors FKrowdKontrolFlatCamera3DPipelineMovementTest's
		// possess/SetAsLocalPlayerController setup exactly.
		APlayerController* Controller = World->SpawnActor<APlayerController>();
		if (!TestNotNull(TEXT("Should be able to spawn a controller to possess the pawn"), Controller))
		{
			return false;
		}
		Controller->Possess(Pawn);
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

		AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), Enemy))
		{
			return false;
		}
		Enemy->TickCheckDetection(Enemy->GetActorLocation()); // Idle -> Alert
		Enemy->ReceiveControl(EAbilitySlot::Stun);             // Alert -> Controlled
		Component->HandleAbilityCastApplied(EAbilitySlot::Stun, Enemy);

		MoveForwardBinding->AxisDelegate.Execute(1.0f);
		Pawn->MovementComponent->TickComponent(0.1f, LEVELTICK_All, nullptr);

		Component->CheckControlPromptDismissCondition();
		TestTrue(TEXT("Control prompt dismiss guard should be set once the tracked Controlled enemy's owner moves"),
			Component->bHasDismissedControlPrompt);

		// Negative: a tracked enemy that is no longer Controlled (banked) must never
		// dismiss the prompt, even with the same movement already applied above.
		AEnemyBaseTestActor* OtherEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("Second AEnemyBaseTestActor should spawn into the test World"), OtherEnemy))
		{
			return false;
		}
		OtherEnemy->TickCheckDetection(OtherEnemy->GetActorLocation());
		OtherEnemy->ReceiveControl(EAbilitySlot::Stun);
		OtherEnemy->TransitionToBanked();
		Component->bHasDismissedControlPrompt = false;
		Component->FirstControlledEnemy = OtherEnemy;
		Component->CheckControlPromptDismissCondition();
		TestFalse(TEXT("Control prompt must never dismiss for a tracked enemy that is no longer Controlled"),
			Component->bHasDismissedControlPrompt);
	}

	// (d) Drop prompt fires when the tracked controlled enemy is near a real
	// ATargetZone, dismisses on that specific enemy's OnEnemyBanked (not any enemy's).
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}

		AKrowdKontrolPlayerController* Controller = SpawnControllerWithPromptWidget(World);
		if (!TestNotNull(TEXT("Controller should spawn"), Controller) ||
			!TestNotNull(TEXT("Controller should own a live OnScreenPromptWidgetInstance"), ToRawPtr(Controller->OnScreenPromptWidgetInstance)))
		{
			return false;
		}

		APawn* Owner = World->SpawnActor<APawn>();
		UTeachingPromptComponent* Component = NewObject<UTeachingPromptComponent>(Owner);
		Component->RegisterComponent();
		Component->BeginPlay();

		const FVector ZoneLocation(500.0f, 0.0f, 0.0f);
		ATargetZone* Zone = World->SpawnActor<ATargetZone>(ATargetZone::StaticClass(), FTransform(ZoneLocation));
		if (!TestNotNull(TEXT("ATargetZone should spawn into the test World"), Zone))
		{
			return false;
		}

		AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>(AEnemyBaseTestActor::StaticClass(), FTransform(ZoneLocation));
		if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), Enemy))
		{
			return false;
		}
		Enemy->TickCheckDetection(Enemy->GetActorLocation());
		Enemy->ReceiveControl(EAbilitySlot::Stun);
		Component->HandleAbilityCastApplied(EAbilitySlot::Stun, Enemy);

		Component->CheckDropPromptFireCondition();
		TestTrue(TEXT("Drop prompt should fire once the tracked controlled enemy is near a real ATargetZone"),
			Component->bHasFiredDropPrompt);
		TestEqual(TEXT("Drop prompt text should be the DROP cue"),
			Controller->OnScreenPromptWidgetInstance->GetPromptDisplayText().ToString(),
			TEXT("DROP IT ON THE GLOWING PEN"));

		// Negative: a different, non-tracked enemy banking must not dismiss it.
		AEnemyBaseTestActor* OtherEnemy = World->SpawnActor<AEnemyBaseTestActor>(AEnemyBaseTestActor::StaticClass(), FTransform(ZoneLocation));
		if (!TestNotNull(TEXT("Second AEnemyBaseTestActor should spawn into the test World"), OtherEnemy))
		{
			return false;
		}
		OtherEnemy->TickCheckDetection(OtherEnemy->GetActorLocation());
		OtherEnemy->ReceiveControl(EAbilitySlot::Stun);
		OtherEnemy->TransitionToBanked();
		TestFalse(TEXT("A different, non-tracked enemy banking must not dismiss the drop prompt"),
			Component->bHasDismissedDropPrompt);

		Enemy->TransitionToBanked();
		TestTrue(TEXT("Drop prompt dismiss guard should be set once the tracked controlled enemy itself banks"),
			Component->bHasDismissedDropPrompt);
	}

	// (e) Room-clear prompt fires once, on the first non-empty room's clear, ignoring
	// vacuous empty rooms.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		// SpawnControllerWithPromptWidget's manual DispatchBeginPlay() must run before
		// the world has begun play - otherwise SpawnActor() below auto-dispatches
		// BeginPlay itself (before Player/SetAsLocalPlayerController() are set), the
		// later manual DispatchBeginPlay() call becomes a no-op (an actor's BeginPlay
		// never runs twice), and OnScreenPromptWidgetInstance is never created.
		AKrowdKontrolPlayerController* Controller = SpawnControllerWithPromptWidget(World);
		if (!TestNotNull(TEXT("Controller should spawn"), Controller) ||
			!TestNotNull(TEXT("Controller should own a live OnScreenPromptWidgetInstance"), ToRawPtr(Controller->OnScreenPromptWidgetInstance)))
		{
			return false;
		}

		// Real dynamic-delegate broadcasts between spawned actors need this - mirrors
		// KrowdKontrolRoomActorDoorGatingTest.cpp's file-comment rationale exactly.
		World->InitializeActorsForPlay(FURL());
		World->SetBegunPlay(true);

		APawn* Owner = World->SpawnActor<APawn>();
		UTeachingPromptComponent* Component = NewObject<UTeachingPromptComponent>(Owner);
		Component->RegisterComponent();

		// Spatially separated (not both left at the default origin transform) so
		// ARoomActor::FindNearestRoom's nearest-by-distance comparison unambiguously
		// resolves Enemy's owning room - AEnemyBase::IsPlayerInOwningRoom()'s
		// Idle->Alert gate (issue #244) blocks the transition entirely if the two
		// rooms tie on distance and FindNearestRoom happens to resolve to the wrong
		// one (see this repo's own project_roomactor_zones_colocated_at_origin note).
		const FVector EmptyRoomLocation(5000.0f, 0.0f, 0.0f);
		ARoomActor* EmptyRoom = World->SpawnActor<ARoomActor>(ARoomActor::StaticClass(), FTransform(EmptyRoomLocation));
		ARoomActor* PopulatedRoom = World->SpawnActor<ARoomActor>();
		if (!TestNotNull(TEXT("Empty ARoomActor should spawn into the test World"), EmptyRoom) ||
			!TestNotNull(TEXT("Populated ARoomActor should spawn into the test World"), PopulatedRoom))
		{
			return false;
		}

		AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), Enemy))
		{
			return false;
		}
		PopulatedRoom->AddOwnedEnemy(Enemy);

		// Component's BeginPlay() binds to every ARoomActor already in the world -
		// both rooms already exist by this point, matching the constructor's real
		// "already-placed rooms" assumption.
		Component->BeginPlay();

		TestFalse(TEXT("Room-clear prompt must not fire merely from the empty room's construction-time broadcast"),
			Controller->OnScreenPromptWidgetInstance->IsPromptVisible());

		Enemy->TickCheckDetection(Enemy->GetActorLocation());
		Enemy->ReceiveControl(EAbilitySlot::Stun);
		Enemy->TransitionToBanked();

		TestTrue(TEXT("Room-clear prompt should fire once the first non-empty room clears"),
			Controller->OnScreenPromptWidgetInstance->IsPromptVisible());
		TestEqual(TEXT("Room-clear prompt text should be the DOOR OPEN cue"),
			Controller->OnScreenPromptWidgetInstance->GetPromptDisplayText().ToString(),
			TEXT("ROOM CLEAR — DOOR OPEN"));
		TestTrue(TEXT("Room-clear prompt fire guard should be set"), Component->bHasFiredRoomClearPrompt);
	}

	// (f) Level-1 scoping: CreateNewMap()'s synthetic map name defaults to level 1 per
	// ParseLevelIndexFromMapName's documented contract, so every case above implicitly
	// runs "as Level 1" - this pins that assumption explicitly.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		TestEqual(TEXT("CreateNewMap()'s synthetic map name should default to level 1"),
			UAbilityUnlockLevelSubsystem::ParseLevelIndexFromMapName(FName(*World->GetMapName())), 1);

		APawn* Owner = World->SpawnActor<APawn>();
		UTeachingPromptComponent* Component = NewObject<UTeachingPromptComponent>(Owner);
		Component->RegisterComponent();
		Component->BeginPlay();
		TestTrue(TEXT("Component should resolve bIsLevel1 true for CreateNewMap()'s synthetic map name"), Component->bIsLevel1);
	}

	// (g) No re-fire after dismissal: representative of the "fires exactly once"
	// contract shared by all four prompts, pinned here for the stun prompt.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}

		AKrowdKontrolPlayerController* Controller = SpawnControllerWithPromptWidget(World);
		if (!TestNotNull(TEXT("Controller should spawn"), Controller) ||
			!TestNotNull(TEXT("Controller should own a live OnScreenPromptWidgetInstance"), ToRawPtr(Controller->OnScreenPromptWidgetInstance)))
		{
			return false;
		}

		APawn* Owner = World->SpawnActor<APawn>();
		UTeachingPromptComponent* Component = NewObject<UTeachingPromptComponent>(Owner);
		Component->RegisterComponent();
		Component->BeginPlay();

		AEnemyBaseTestActor* EnemyOne = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("First AEnemyBaseTestActor should spawn into the test World"), EnemyOne))
		{
			return false;
		}
		EnemyOne->TickCheckDetection(FVector::ZeroVector);
		Component->CheckStunPromptFireCondition();
		Component->HandleAbilityCastApplied(EAbilitySlot::Stun, EnemyOne);
		TestTrue(TEXT("Stun prompt should have fired and dismissed once"),
			Component->bHasFiredStunPrompt && Component->bHasDismissedStunPrompt);

		Controller->OnScreenPromptWidgetInstance->AdvanceDismissTimer(UOnScreenPromptWidget::MaxPromptDurationSeconds);
		TestFalse(TEXT("Prompt should have expired via AdvanceDismissTimer"),
			Controller->OnScreenPromptWidgetInstance->IsPromptVisible());

		AEnemyBaseTestActor* EnemyTwo = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("Second AEnemyBaseTestActor should spawn into the test World"), EnemyTwo))
		{
			return false;
		}
		EnemyTwo->TickCheckDetection(FVector::ZeroVector);
		Component->CheckStunPromptFireCondition();
		TestTrue(TEXT("Stun prompt fire guard should remain set, no re-fire"), Component->bHasFiredStunPrompt);
		TestFalse(TEXT("Stun prompt must never show again once already fired for this component instance"),
			Controller->OnScreenPromptWidgetInstance->IsPromptVisible());
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
