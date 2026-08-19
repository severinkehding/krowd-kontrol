// Confirms UAbilityMatchupNudgeComponent (issue #40, PRD 09 REQ-5 / PRD 02 REQ-4):
// consuming UAbilityMatchupSignalComponent::OnAbilityMatchupSignal, 3 consecutive
// non-colour-matched successful casts trigger exactly one
// UOnScreenPromptWidget::ShowPrompt() call - not before, not more than once per pawn
// instance - and a matched cast anywhere in the streak resets the counter instead of
// combining toward the threshold.
//
// Each case uses its own FAutomationEditorCommonUtils::CreateNewMap() World, per this
// module's established per-scenario isolation convention (see
// KrowdKontrolFirstStunBeaconComponentTest.cpp).
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "AbilityMatchupNudgeComponent.h"
#include "KrowdKontrolPlayerController.h"
#include "OnScreenPromptWidget.h"
#include "FlatCamera3DPrototypePawn.h"
#include "AbilityMatchupSignalComponent.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolAbilityMatchupNudgeComponentTest,
	"KrowdKontrol.Unit.AbilityMatchupNudgeComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace KrowdKontrolAbilityMatchupNudgeComponentTest
{
	// Spawns a controller-backed AKrowdKontrolPlayerController with a real, live
	// OnScreenPromptWidgetInstance - mirrors KrowdKontrolHUDWiringTest.cpp:43-66
	// exactly (CreateWidget<T>(APlayerController*, ...) hard-requires a real
	// ULocalPlayer already attached, and DispatchBeginPlay(), not BeginPlay()
	// directly, or UE 5.8 asserts).
	//
	// FAutomationEditorCommonUtils::CreateNewMap()'s World never calls
	// World->InitializeActorsForPlay() (no PIE session backs this harness), so
	// World->AreActorsInitialized() is false and AActor::PostActorConstruction()
	// never routes to PostInitializeComponents() for actors spawned into it -
	// confirmed empirically (UWorld::GetNumControllers()/GetNumPlayerControllers()
	// both read 0 immediately after a plain SpawnActor<AKrowdKontrolPlayerController>()
	// here, even after DispatchBeginPlay()). AController::PostInitializeComponents()
	// is what calls UWorld::AddController() in real gameplay (PIE/packaged, where
	// GameMode/InitializeActorsForPlay does run this), so
	// UAbilityMatchupNudgeComponent::ResolvePromptWidget()'s
	// World->GetFirstPlayerController() lookup is correct production code but needs
	// this registration step done explicitly here to match what the engine does
	// automatically outside this harness. UWorld::AddController() is public API,
	// safe to call directly.
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

bool FKrowdKontrolAbilityMatchupNudgeComponentTest::RunTest(const FString& Parameters)
{
	using namespace KrowdKontrolAbilityMatchupNudgeComponentTest;

	// (a) Below threshold: 2 (i.e. NonMatchedCastThreshold - 1) non-matched calls must
	// not show the prompt yet.
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
		if (!TestNotNull(TEXT("APawn should spawn into the test World"), Owner))
		{
			return false;
		}
		UAbilityMatchupNudgeComponent* NudgeComponent = NewObject<UAbilityMatchupNudgeComponent>(Owner);
		NudgeComponent->RegisterComponent();

		NudgeComponent->HandleAbilityMatchupSignal(EAbilitySlot::Sleep, nullptr, false);
		NudgeComponent->HandleAbilityMatchupSignal(EAbilitySlot::Sleep, nullptr, false);
		TestFalse(TEXT("Prompt should not be visible before the threshold is reached"),
			Controller->OnScreenPromptWidgetInstance->IsPromptVisible());
	}

	// (b) Threshold reached: a 3rd consecutive non-matched call must show the prompt
	// with the expected text.
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
		UAbilityMatchupNudgeComponent* NudgeComponent = NewObject<UAbilityMatchupNudgeComponent>(Owner);
		NudgeComponent->RegisterComponent();

		NudgeComponent->HandleAbilityMatchupSignal(EAbilitySlot::Sleep, nullptr, false);
		NudgeComponent->HandleAbilityMatchupSignal(EAbilitySlot::Sleep, nullptr, false);
		NudgeComponent->HandleAbilityMatchupSignal(EAbilitySlot::Sleep, nullptr, false);
		TestTrue(TEXT("Prompt should be visible once the threshold is reached"),
			Controller->OnScreenPromptWidgetInstance->IsPromptVisible());
		TestEqual(TEXT("Prompt text should be the nudge message"),
			Controller->OnScreenPromptWidgetInstance->GetPromptDisplayText().ToString(),
			TEXT("Try matching the ability colour to the enemy."));

		// (c) One-shot, no repeat: once the prompt has expired, further non-matched
		// casts must never show it again.
		Controller->OnScreenPromptWidgetInstance->AdvanceDismissTimer(UOnScreenPromptWidget::MaxPromptDurationSeconds);
		TestFalse(TEXT("Prompt should have expired via AdvanceDismissTimer"),
			Controller->OnScreenPromptWidgetInstance->IsPromptVisible());

		NudgeComponent->HandleAbilityMatchupSignal(EAbilitySlot::Sleep, nullptr, false);
		NudgeComponent->HandleAbilityMatchupSignal(EAbilitySlot::Sleep, nullptr, false);
		NudgeComponent->HandleAbilityMatchupSignal(EAbilitySlot::Sleep, nullptr, false);
		TestFalse(TEXT("Prompt must not re-trigger after already firing once for this component instance"),
			Controller->OnScreenPromptWidgetInstance->IsPromptVisible());
	}

	// (d) A matched cast resets the counter: two non-matched calls (below threshold),
	// one matched call (resets), two more non-matched calls (still below threshold
	// post-reset), then a third non-matched call (now at threshold post-reset).
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
		UAbilityMatchupNudgeComponent* NudgeComponent = NewObject<UAbilityMatchupNudgeComponent>(Owner);
		NudgeComponent->RegisterComponent();

		NudgeComponent->HandleAbilityMatchupSignal(EAbilitySlot::Sleep, nullptr, false);
		NudgeComponent->HandleAbilityMatchupSignal(EAbilitySlot::Sleep, nullptr, false);
		NudgeComponent->HandleAbilityMatchupSignal(EAbilitySlot::Sleep, nullptr, true);
		TestFalse(TEXT("A matched cast should not itself trigger the prompt"),
			Controller->OnScreenPromptWidgetInstance->IsPromptVisible());

		NudgeComponent->HandleAbilityMatchupSignal(EAbilitySlot::Sleep, nullptr, false);
		NudgeComponent->HandleAbilityMatchupSignal(EAbilitySlot::Sleep, nullptr, false);
		TestFalse(TEXT("Two non-matched casts after a reset should still be below threshold"),
			Controller->OnScreenPromptWidgetInstance->IsPromptVisible());

		NudgeComponent->HandleAbilityMatchupSignal(EAbilitySlot::Sleep, nullptr, false);
		TestTrue(TEXT("A third non-matched cast after a reset should reach the threshold"),
			Controller->OnScreenPromptWidgetInstance->IsPromptVisible());
	}

	// (e) Missing-widget defensive case: no AKrowdKontrolPlayerController spawned at
	// all, so GetWorld()->GetFirstPlayerController() returns null. Must not crash and
	// must warn exactly once.
	{
		AddExpectedError(TEXT("no OnScreenPromptWidget available"), EAutomationExpectedErrorFlags::Contains, 1, false);

		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}

		APawn* Owner = World->SpawnActor<APawn>();
		UAbilityMatchupNudgeComponent* NudgeComponent = NewObject<UAbilityMatchupNudgeComponent>(Owner);
		NudgeComponent->RegisterComponent();

		NudgeComponent->HandleAbilityMatchupSignal(EAbilitySlot::Sleep, nullptr, false);
		NudgeComponent->HandleAbilityMatchupSignal(EAbilitySlot::Sleep, nullptr, false);
		NudgeComponent->HandleAbilityMatchupSignal(EAbilitySlot::Sleep, nullptr, false);
		TestTrue(TEXT("HandleAbilityMatchupSignal must not crash when no OnScreenPromptWidget can be resolved"), true);
	}

	// (f) Real-pawn wiring: AFlatCamera3DPrototypePawn's constructor binds
	// AbilityMatchupSignalComponent->OnAbilityMatchupSignal to its own
	// AbilityMatchupNudgeComponent - a copy-paste slip there would compile cleanly and
	// every other case above would still pass, since none of them go through the real
	// pawn.
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

		AFlatCamera3DPrototypePawn* WiringPawn = World->SpawnActor<AFlatCamera3DPrototypePawn>();
		if (!TestNotNull(TEXT("AFlatCamera3DPrototypePawn should spawn into the test World"), WiringPawn))
		{
			return false;
		}
		if (!TestNotNull(TEXT("The real pawn's AbilityMatchupNudgeComponent should be constructed"),
			ToRawPtr(WiringPawn->AbilityMatchupNudgeComponent)))
		{
			return false;
		}

		WiringPawn->AbilityMatchupSignalComponent->OnAbilityMatchupSignal.Broadcast(EAbilitySlot::Sleep, nullptr, false);
		WiringPawn->AbilityMatchupSignalComponent->OnAbilityMatchupSignal.Broadcast(EAbilitySlot::Sleep, nullptr, false);
		WiringPawn->AbilityMatchupSignalComponent->OnAbilityMatchupSignal.Broadcast(EAbilitySlot::Sleep, nullptr, false);
		TestTrue(TEXT("The pawn's real constructor-time AddDynamic binding must reach AbilityMatchupNudgeComponent"),
			Controller->OnScreenPromptWidgetInstance->IsPromptVisible());
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
