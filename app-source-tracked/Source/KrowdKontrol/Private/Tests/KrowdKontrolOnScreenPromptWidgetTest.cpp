// Confirms UOnScreenPromptWidget (issue #34) shows a text cue via ShowPrompt(),
// auto-dismisses at the ~2s hard cap, never uses ESlateVisibility::Visible (the
// input-non-blocking guarantee), clamps an oversized/negative requested duration, and
// replaces rather than stacks when re-triggered while already showing. Also confirms
// an unbuilt widget tree degrades to safe defaults rather than crashing, and the
// Initialize()/NativeOnInitialized() guard builds the tree exactly once regardless of
// call order - same reasoning as KrowdKontrolAbilityCooldownTrayWidgetTest.cpp and
// KrowdKontrolPostRunSummaryWidgetTest.cpp.
//
// CreateWidget() calls Initialize() synchronously, which fires NativeOnInitialized() -
// no TakeWidget()/AddToViewport()/Slate realization needed, so this works under the
// -nullrhi flag KrowdKontrol.Unit.* tests run with (see harness/run_ue_automation.sh).
// Needs a real UWorld (CreateWidget's first argument). The Initialize()-guard and
// unbuilt-tree cases use a bare NewObject() instead (no World needed) via this test
// class's friend-class access. Most of this test calls AdvanceDismissTimer() directly,
// since NativeTick's usual driver - live Slate ticking - isn't available under
// -nullrhi; the NativeTick() override itself is still called directly once (via
// friend-class access, bypassing Slate) to cover the real per-frame call site rather
// than relying solely on the AdvanceDismissTimer() proxy.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "OnScreenPromptWidget.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolOnScreenPromptWidgetTest,
	"KrowdKontrol.Unit.OnScreenPromptWidget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolOnScreenPromptWidgetTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	// (a) Widget constructs.
	UOnScreenPromptWidget* Widget =
		CreateWidget<UOnScreenPromptWidget>(World, UOnScreenPromptWidget::StaticClass());
	if (!TestNotNull(TEXT("UOnScreenPromptWidget should construct"), Widget))
	{
		return false;
	}

	// (b) Idle state immediately after construction.
	TestFalse(TEXT("Prompt should not be visible immediately after construction"), Widget->IsPromptVisible());
	TestEqual(TEXT("Remaining seconds should be 0 immediately after construction"), Widget->GetRemainingSeconds(), 0.0f);
	TestTrue(TEXT("Prompt display text should be empty immediately after construction"), Widget->GetPromptDisplayText().IsEmpty());

	// (c) ShowPrompt() with the default duration hits exactly the cap, and the
	// input-safety assertion: chrome must be HitTestInvisible, never Visible.
	const FText SomeText = FText::FromString(TEXT("Match the colour to the enemy."));
	Widget->ShowPrompt(SomeText);
	TestTrue(TEXT("Prompt should be visible after ShowPrompt()"), Widget->IsPromptVisible());
	TestEqual(TEXT("Remaining seconds should equal MaxPromptDurationSeconds by default"),
		Widget->GetRemainingSeconds(), UOnScreenPromptWidget::MaxPromptDurationSeconds);
	TestEqual(TEXT("Prompt display text should equal the shown message"),
		Widget->GetPromptDisplayText().ToString(), SomeText.ToString());
	TestTrue(TEXT("Prompt border visibility should be exactly HitTestInvisible while showing"),
		Widget->PromptBorder->GetVisibility() == ESlateVisibility::HitTestInvisible);

	// (d) Countdown advances.
	Widget->AdvanceDismissTimer(1.0f);
	TestTrue(TEXT("Prompt should still be visible after 1s of a 2s prompt"), Widget->IsPromptVisible());
	TestEqual(TEXT("Remaining seconds should be 1.0f after 1s"), Widget->GetRemainingSeconds(), 1.0f);

	// (e) Dismissed within the ~2s cap - the acceptance criterion.
	Widget->AdvanceDismissTimer(1.0f);
	TestFalse(TEXT("Prompt should no longer be visible after the full 2s"), Widget->IsPromptVisible());
	TestEqual(TEXT("Remaining seconds should be 0 after the full 2s"), Widget->GetRemainingSeconds(), 0.0f);
	TestTrue(TEXT("Prompt display text should be empty again after dismissal"), Widget->GetPromptDisplayText().IsEmpty());
	TestTrue(TEXT("Prompt border visibility should be Collapsed after dismissal"),
		Widget->PromptBorder->GetVisibility() == ESlateVisibility::Collapsed);

	// (f) A large delta clears immediately, remaining never goes negative.
	Widget->ShowPrompt(SomeText);
	Widget->AdvanceDismissTimer(100.0f);
	TestFalse(TEXT("Prompt should dismiss immediately on an oversized delta"), Widget->IsPromptVisible());
	TestEqual(TEXT("Remaining seconds should clamp at 0 on an oversized delta"), Widget->GetRemainingSeconds(), 0.0f);

	// (g) Cap enforcement - no prompt can persist beyond MaxPromptDurationSeconds.
	Widget->ShowPrompt(SomeText, 10.0f);
	TestEqual(TEXT("Remaining seconds should clamp to MaxPromptDurationSeconds, not the requested 10.0f"),
		Widget->GetRemainingSeconds(), UOnScreenPromptWidget::MaxPromptDurationSeconds);

	// (h) Negative duration clamps to 0 / not visible.
	Widget->ShowPrompt(SomeText, -5.0f);
	TestFalse(TEXT("Negative ShowPrompt duration should clamp to not-visible"), Widget->IsPromptVisible());
	TestEqual(TEXT("Negative ShowPrompt duration should clamp remaining to 0"), Widget->GetRemainingSeconds(), 0.0f);

	// (i) No stacking - a fresh ShowPrompt() while one is already showing replaces
	// rather than queues.
	const FText TextA = FText::FromString(TEXT("Prompt A"));
	const FText TextB = FText::FromString(TEXT("Prompt B"));
	Widget->ShowPrompt(TextA);
	Widget->AdvanceDismissTimer(1.0f);
	TestEqual(TEXT("1.0f remaining before the re-trigger"), Widget->GetRemainingSeconds(), 1.0f);
	Widget->ShowPrompt(TextB);
	TestEqual(TEXT("Display text should be the latest prompt's message"),
		Widget->GetPromptDisplayText().ToString(), TextB.ToString());
	TestEqual(TEXT("Remaining seconds should reset to the cap on re-trigger"),
		Widget->GetRemainingSeconds(), UOnScreenPromptWidget::MaxPromptDurationSeconds);

	// (j) NativeTick actually drives the timer - the real per-frame code path a live
	// game session ticks, not just the direct AdvanceDismissTimer() calls used above.
	Widget->ShowPrompt(SomeText);
	Widget->NativeTick(FGeometry(), 1.0f);
	TestEqual(TEXT("NativeTick should advance the timer via AdvanceDismissTimer()"),
		Widget->GetRemainingSeconds(), 1.0f);
	// Idle NativeTick should not perturb an already-zero timer.
	Widget->AdvanceDismissTimer(1.0f);
	TestEqual(TEXT("Timer should be 0 after fully dismissing"), Widget->GetRemainingSeconds(), 0.0f);
	Widget->NativeTick(FGeometry(), 1.0f);
	TestEqual(TEXT("NativeTick while idle should not change remaining seconds away from 0"),
		Widget->GetRemainingSeconds(), 0.0f);

	// (k) Initialize() guard - must not rebuild the tree when NativeOnInitialized()
	// already ran, and the reverse call order too.
	UOnScreenPromptWidget* GuardWidget = NewObject<UOnScreenPromptWidget>();
	if (!TestNotNull(TEXT("UOnScreenPromptWidget should construct for guard test"), GuardWidget))
	{
		return false;
	}
	GuardWidget->NativeOnInitialized();
	UBorder* FirstPromptBorder = GuardWidget->PromptBorder;
	if (!TestNotNull(TEXT("PromptBorder should be set after NativeOnInitialized()"), FirstPromptBorder))
	{
		return false;
	}
	GuardWidget->Initialize();
	TestEqual(TEXT("Initialize() must not rebuild the tree when already built"),
		ToRawPtr(GuardWidget->PromptBorder), FirstPromptBorder);

	UOnScreenPromptWidget* ReverseGuardWidget = NewObject<UOnScreenPromptWidget>();
	if (!TestNotNull(TEXT("UOnScreenPromptWidget should construct for reverse guard test"), ReverseGuardWidget))
	{
		return false;
	}
	ReverseGuardWidget->Initialize();
	UBorder* ReverseFirstPromptBorder = ReverseGuardWidget->PromptBorder;
	if (!TestNotNull(TEXT("PromptBorder should be set after Initialize()"), ReverseFirstPromptBorder))
	{
		return false;
	}
	ReverseGuardWidget->NativeOnInitialized();
	TestEqual(TEXT("NativeOnInitialized() must not rebuild the tree when already built"),
		ToRawPtr(ReverseGuardWidget->PromptBorder), ReverseFirstPromptBorder);

	// (l) Unbuilt-tree safety - a widget whose tree was never built (bare
	// NewObject(), neither NativeOnInitialized() nor Initialize() called) should
	// degrade safely rather than crashing.
	UOnScreenPromptWidget* UnbuiltWidget = NewObject<UOnScreenPromptWidget>();
	if (!TestNotNull(TEXT("UOnScreenPromptWidget should construct for unbuilt-tree test"), UnbuiltWidget))
	{
		return false;
	}
	TestFalse(TEXT("Unbuilt widget should report not visible"), UnbuiltWidget->IsPromptVisible());
	TestEqual(TEXT("Unbuilt widget should report 0 remaining seconds"), UnbuiltWidget->GetRemainingSeconds(), 0.0f);
	TestTrue(TEXT("Unbuilt widget should report empty display text"), UnbuiltWidget->GetPromptDisplayText().IsEmpty());
	UnbuiltWidget->ShowPrompt(SomeText);
	UnbuiltWidget->AdvanceDismissTimer(1.0f);
	TestTrue(TEXT("ShowPrompt()/AdvanceDismissTimer() on an unbuilt tree should not crash"), true);

	// (m) AddToViewport() no-crash smoke check under -nullrhi (expect no-op, not a
	// real viewport attach - no live UGameViewportSubsystem target exists under
	// -nullrhi/CreateNewMap()).
	Widget->AddToViewport();
	TestTrue(TEXT("AddToViewport() should not crash"), true);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
