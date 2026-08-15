// Confirms UPostRunSummaryWidget (issue #74) renders both the clear-time and Crowd
// Mastery fields immediately on construction (placeholder values, per the issue's
// scope - real tracking is PRD 06 REQ-2/REQ-3, out of scope here), and that
// SetSummaryValues() - the wiring point a future real tracking system will use -
// formats and updates both fields correctly. Clear time is formatted M:SS, matching
// PRD 06 REQ-2's own display example ("Your best: 4:32").
//
// CreateWidget() calls Initialize() synchronously, which fires
// NativeOnInitialized() - no TakeWidget()/AddToViewport()/Slate realization needed,
// so this works under the -nullrhi flag KrowdKontrol.Unit.* tests run with (see
// harness/run_ue_automation.sh). Needs a real UWorld (CreateWidget's first
// argument), same reasoning as KrowdKontrolRoomEnemyBudgetControllerTest.cpp.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "PostRunSummaryWidget.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolPostRunSummaryWidgetTest,
	"KrowdKontrol.Unit.PostRunSummaryWidget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolPostRunSummaryWidgetTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	UPostRunSummaryWidget* Widget = CreateWidget<UPostRunSummaryWidget>(World, UPostRunSummaryWidget::StaticClass());
	if (!TestNotNull(TEXT("UPostRunSummaryWidget should construct"), Widget))
	{
		return false;
	}

	// (a) placeholder values are visible immediately after construction - no external
	// caller needed, the screen is self-demonstrating.
	TestTrue(TEXT("Clear time text should be non-empty after construction"),
		!Widget->GetClearTimeDisplayText().IsEmpty());
	TestTrue(TEXT("Crowd Mastery text should be non-empty after construction"),
		!Widget->GetCrowdMasteryDisplayText().IsEmpty());

	// (b) SetSummaryValues is the real wiring point a future tracking system will
	// use - confirm both fields reflect an explicit call with known values, not just
	// whatever the placeholder default happens to be.
	Widget->SetSummaryValues(272.0f, 14);
	TestEqual(TEXT("Clear time should format as M:SS"),
		Widget->GetClearTimeDisplayText().ToString(), FString(TEXT("Clear Time: 4:32")));
	TestEqual(TEXT("Crowd Mastery should show the given count"),
		Widget->GetCrowdMasteryDisplayText().ToString(), FString(TEXT("Crowd Mastery: 14")));

	// (c) A zero-second, zero-count run should still render (edge case: no negative
	// time/counts, no divide-by-zero in the minutes/seconds split).
	Widget->SetSummaryValues(0.0f, 0);
	TestEqual(TEXT("Zero clear time should format as 0:00"),
		Widget->GetClearTimeDisplayText().ToString(), FString(TEXT("Clear Time: 0:00")));
	TestEqual(TEXT("Zero Crowd Mastery should show 0"),
		Widget->GetCrowdMasteryDisplayText().ToString(), FString(TEXT("Crowd Mastery: 0")));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
