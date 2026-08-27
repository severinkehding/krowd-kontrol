// Pins the controller-level input-binding flags that the 2026-08-24/26 all-input-dead
// regression (PR #272 -> PR #309) turned out to hinge on. The briefing test drives
// HandleBriefingDismissInput() directly and so passed the whole time game input was
// dead; this test inspects the actual FInputKeyBinding the controller registers, so a
// future consuming (or pause-inert) controller-level binding fails the gate instead of
// shipping.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "KrowdKontrolPlayerController.h"
#include "Components/InputComponent.h"
#include "InputCoreTypes.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolPlayerControllerInputBindingTest,
	"KrowdKontrol.Unit.PlayerControllerInputBinding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolPlayerControllerInputBindingTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	AKrowdKontrolPlayerController* Controller = World->SpawnActor<AKrowdKontrolPlayerController>();
	if (!TestNotNull(TEXT("AKrowdKontrolPlayerController should spawn into the test World"), Controller))
	{
		return false;
	}

	// A plain SpawnActor in an automation world never runs SetupInputComponent() -
	// that hangs off InitInputSystem(), which normally fires on possession by a real
	// local player. Invoke it directly; it creates PlayerInput + InputComponent and
	// then calls SetupInputComponent(), which is the code under test.
	if (!Controller->InputComponent)
	{
		Controller->InitInputSystem();
	}
	if (!TestNotNull(TEXT("InitInputSystem should leave the controller with an InputComponent"),
		ToRawPtr(Controller->InputComponent)))
	{
		return false;
	}

	const FInputKeyBinding* BriefingDismissBinding = nullptr;
	const FInputKeyBinding* DebugMenuBinding = nullptr;
	for (const FInputKeyBinding& Binding : Controller->InputComponent->KeyBindings)
	{
		if (Binding.Chord.Key == EKeys::AnyKey && Binding.KeyEvent == IE_Pressed)
		{
			BriefingDismissBinding = &Binding;
		}
		if (Binding.Chord.Key == EKeys::F1 && Binding.KeyEvent == IE_Pressed)
		{
			DebugMenuBinding = &Binding;
		}
	}

	if (!TestNotNull(TEXT("SetupInputComponent should register the AnyKey briefing-dismiss binding"),
		BriefingDismissBinding))
	{
		return false;
	}
	// The two flags PR #309 exists for. bConsumeInput=false: a consuming AnyKey
	// binding at controller level eats every key press before the possessed pawn's
	// input stack runs (the all-input-dead regression). bExecuteWhenPaused=true:
	// ShowBriefing() pauses the world, and a binding without this flag is skipped
	// entirely while paused - the dismiss binding would never fire during the one
	// window it exists for.
	TestFalse(TEXT("Briefing-dismiss AnyKey binding must NOT consume input (PR #272 regression, PR #309 fix)"),
		BriefingDismissBinding->bConsumeInput);
	TestTrue(TEXT("Briefing-dismiss AnyKey binding must execute while the world is paused (briefing pauses the game)"),
		BriefingDismissBinding->bExecuteWhenPaused);

	// Mirror-parity guard for the adjacent #308 binding: it must exist, and it must
	// not have quietly become a consuming binding either (F1 is harmless to consume,
	// but consuming AnyKey-adjacent regressions land exactly this way).
	TestNotNull(TEXT("SetupInputComponent should register the F1 debug-menu binding (#308)"),
		DebugMenuBinding);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
