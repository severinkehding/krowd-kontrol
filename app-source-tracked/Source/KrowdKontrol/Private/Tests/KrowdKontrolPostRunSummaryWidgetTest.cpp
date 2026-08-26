// Confirms UPostRunSummaryWidget (issue #74) renders both the clear-time and Crowd
// Mastery fields immediately on construction (placeholder values, per the issue's
// scope - real tracking is PRD 06 REQ-2/REQ-3, out of scope here), and that
// SetSummaryValues() - the wiring point a future real tracking system will use -
// formats and updates both fields correctly. Clear time is formatted M:SS, matching
// PRD 06 REQ-2's own display example ("Your best: 4:32"). Also confirms negative
// inputs floor at zero, an unbuilt widget tree degrades to empty display text rather
// than crashing, and the Initialize()/NativeOnInitialized() guard (EnsureWidgetTreeBuilt())
// builds the tree exactly once regardless of which of the two fires first - both call
// orders are exercised below.
//
// CreateWidget() calls Initialize() synchronously, which fires
// NativeOnInitialized() - no TakeWidget()/AddToViewport()/Slate realization needed,
// so this works under the -nullrhi flag KrowdKontrol.Unit.* tests run with (see
// harness/run_ue_automation.sh). Needs a real UWorld (CreateWidget's first
// argument), same reasoning as KrowdKontrolRoomEnemyBudgetControllerTest.cpp. The
// Initialize()-guard and unbuilt-tree cases use a bare NewObject() instead (no World
// needed) via this test class's friend-class access to ClearTimeText and to
// NativeOnInitialized()/Initialize() directly, matching KrowdKontrolPlayerEnergyComponentTest.cpp's
// friend-class precedent for exercising lifecycle/guard edge cases without full
// construction machinery.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "PostRunSummaryWidget.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"

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
	TestTrue(TEXT("Best clear time text should be non-empty after construction"),
		!Widget->GetBestClearTimeDisplayText().IsEmpty());
	TestTrue(TEXT("Crowd Mastery text should be non-empty after construction"),
		!Widget->GetCrowdMasteryDisplayText().IsEmpty());

	// (b) SetSummaryValues is the real wiring point HandleLevelClear() uses - confirm
	// all three fields reflect an explicit call with known values, not just whatever
	// the placeholder default happens to be. Best-time value (200.0f) deliberately
	// distinct from the clear-time value (272.0f) so a copy-paste argument-order bug
	// would be caught by the assertions, not silently pass.
	Widget->SetSummaryValues(272.0f, 200.0f, 14);
	TestEqual(TEXT("Clear time should format as M:SS"),
		Widget->GetClearTimeDisplayText().ToString(), FString(TEXT("Clear Time: 4:32")));
	TestEqual(TEXT("Best clear time should format as M:SS"),
		Widget->GetBestClearTimeDisplayText().ToString(), FString(TEXT("Best: 3:20")));
	TestEqual(TEXT("Crowd Mastery should show the given count"),
		Widget->GetCrowdMasteryDisplayText().ToString(), FString(TEXT("Crowd Mastery: 14")));

	// (c) A zero-second, zero-count run should still render (edge case: no negative
	// time/counts, no divide-by-zero in the minutes/seconds split).
	Widget->SetSummaryValues(0.0f, 0.0f, 0);
	TestEqual(TEXT("Zero clear time should format as 0:00"),
		Widget->GetClearTimeDisplayText().ToString(), FString(TEXT("Clear Time: 0:00")));
	TestEqual(TEXT("Zero best clear time should format as 0:00"),
		Widget->GetBestClearTimeDisplayText().ToString(), FString(TEXT("Best: 0:00")));
	TestEqual(TEXT("Zero Crowd Mastery should show 0"),
		Widget->GetCrowdMasteryDisplayText().ToString(), FString(TEXT("Crowd Mastery: 0")));

	// (d) Negative inputs must floor at zero (SetSummaryValues() clamps with
	// FMath::Max(0, ...) rather than asserting - confirm that's what actually happens).
	Widget->SetSummaryValues(-5.0f, -5.0f, -3);
	TestEqual(TEXT("Negative clear time should floor to 0:00"),
		Widget->GetClearTimeDisplayText().ToString(), FString(TEXT("Clear Time: 0:00")));
	TestEqual(TEXT("Negative best clear time should floor to 0:00"),
		Widget->GetBestClearTimeDisplayText().ToString(), FString(TEXT("Best: 0:00")));
	TestEqual(TEXT("Negative Crowd Mastery should floor to 0"),
		Widget->GetCrowdMasteryDisplayText().ToString(), FString(TEXT("Crowd Mastery: 0")));

	// (e) The Initialize() safety net must not rebuild the tree when
	// NativeOnInitialized() already built it - the "already built, skip" branch that
	// only matters for real, player-owned gameplay usage (CreateWidget() above never
	// exercises it, since it's player-less). Uses the friend-class direct-call access
	// this test class has to NativeOnInitialized()/Initialize() to simulate that path
	// without needing a real owning player/controller.
	UPostRunSummaryWidget* GuardWidget = NewObject<UPostRunSummaryWidget>();
	if (!TestNotNull(TEXT("UPostRunSummaryWidget should construct for guard test"), GuardWidget))
	{
		return false;
	}
	GuardWidget->NativeOnInitialized();
	UTextBlock* FirstClearTimeText = GuardWidget->ClearTimeText;
	if (!TestNotNull(TEXT("ClearTimeText should be set after NativeOnInitialized()"), FirstClearTimeText))
	{
		return false;
	}
	GuardWidget->Initialize();
	TestEqual(TEXT("Initialize() must not rebuild the tree when ClearTimeText is already set"),
		ToRawPtr(GuardWidget->ClearTimeText), FirstClearTimeText);

	// (e2) The reverse call order: Initialize() builds the tree first, and a direct
	// NativeOnInitialized() call afterwards must be the no-op instead - proves the
	// guard is symmetric, not just correct for the one order that crashed in PR #89.
	UPostRunSummaryWidget* ReverseGuardWidget = NewObject<UPostRunSummaryWidget>();
	if (!TestNotNull(TEXT("UPostRunSummaryWidget should construct for reverse guard test"), ReverseGuardWidget))
	{
		return false;
	}
	ReverseGuardWidget->Initialize();
	UTextBlock* ReverseFirstClearTimeText = ReverseGuardWidget->ClearTimeText;
	if (!TestNotNull(TEXT("ClearTimeText should be set after Initialize()"), ReverseFirstClearTimeText))
	{
		return false;
	}
	ReverseGuardWidget->NativeOnInitialized();
	TestEqual(TEXT("NativeOnInitialized() must not rebuild the tree when ClearTimeText is already set"),
		ToRawPtr(ReverseGuardWidget->ClearTimeText), ReverseFirstClearTimeText);

	// (f) A widget whose tree was never built (bare NewObject(), neither
	// NativeOnInitialized() nor Initialize() called) should degrade to empty display
	// text rather than crashing on a null ClearTimeText/CrowdMasteryText.
	UPostRunSummaryWidget* UnbuiltWidget = NewObject<UPostRunSummaryWidget>();
	if (!TestNotNull(TEXT("UPostRunSummaryWidget should construct for unbuilt-tree test"), UnbuiltWidget))
	{
		return false;
	}
	TestTrue(TEXT("Clear time text should be empty when the tree was never built"),
		UnbuiltWidget->GetClearTimeDisplayText().IsEmpty());
	TestTrue(TEXT("Best clear time text should be empty when the tree was never built"),
		UnbuiltWidget->GetBestClearTimeDisplayText().IsEmpty());
	TestTrue(TEXT("Crowd Mastery text should be empty when the tree was never built"),
		UnbuiltWidget->GetCrowdMasteryDisplayText().IsEmpty());
	UnbuiltWidget->SetSummaryValues(272.0f, 200.0f, 14);
	TestTrue(TEXT("SetSummaryValues() on an unbuilt tree should not crash and should stay empty"),
		UnbuiltWidget->GetClearTimeDisplayText().IsEmpty());
	TestTrue(TEXT("SetSummaryValues() on an unbuilt tree should leave best clear time empty too"),
		UnbuiltWidget->GetBestClearTimeDisplayText().IsEmpty());

	// (g) Centred anchoring (issue #319 / Post-Run Progression PRD REQ-1's locked
	// 2026-08-26 operator design decision): the content block's UCanvasPanelSlot must
	// be anchored and aligned to screen-centre on both axes, auto-sized so wrapped
	// text grows the box instead of clipping - mirrors
	// KrowdKontrolQuestTrackerWidgetTest.cpp's case (4)/(4b) (issue #310's pattern),
	// adapted from corner to centre anchoring.
	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(Widget->WidgetTree->RootWidget);
	if (TestNotNull(TEXT("Widget root should be a UCanvasPanel"), RootCanvas))
	{
		TestEqual(TEXT("Root canvas should have exactly one child"), RootCanvas->GetChildrenCount(), 1);
		if (RootCanvas->GetChildrenCount() == 1)
		{
			UCanvasPanelSlot* ContentSlot = Cast<UCanvasPanelSlot>(RootCanvas->GetChildAt(0)->Slot);
			if (TestNotNull(TEXT("Content child's slot should be a UCanvasPanelSlot"), ContentSlot))
			{
				TestTrue(TEXT("Content block should be anchored to screen-centre on both axes"),
					ContentSlot->GetAnchors() == FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
				TestTrue(TEXT("Content block should be aligned to its own centre"),
					ContentSlot->GetAlignment() == FVector2D(0.5f, 0.5f));
				TestTrue(TEXT("Content slot should auto-size so wrapped text grows the box, not off-screen (issue #319)"),
					ContentSlot->GetAutoSize());
			}
			USizeBox* WidthCap = Cast<USizeBox>(RootCanvas->GetChildAt(0));
			if (TestNotNull(TEXT("Root canvas child should be the width-capping USizeBox (issue #319)"), WidthCap))
			{
				TestEqual(TEXT("Width cap should hold ContentWidthPx"),
					WidthCap->GetWidthOverride(), UPostRunSummaryWidget::ContentWidthPx);
				TestEqual(TEXT("Width cap's content should be the chrome border, not detached from the live tree"),
					Cast<UWidget>(WidthCap->GetContent()), Cast<UWidget>(Widget->RootBorder));
			}
		}
	}

	TestTrue(TEXT("Clear time text should auto-wrap inside the width cap (issue #319)"),
		Widget->ClearTimeText->GetAutoWrapText());
	TestTrue(TEXT("Best clear time text should auto-wrap inside the width cap (issue #319)"),
		Widget->BestClearTimeText->GetAutoWrapText());
	TestTrue(TEXT("Crowd Mastery text should auto-wrap inside the width cap (issue #319)"),
		Widget->CrowdMasteryText->GetAutoWrapText());

	// (h) Resolution-safety envelope, checked at this project's documented min/max
	// target resolutions - same pair KrowdKontrolQuestTrackerWidgetTest.cpp's case (5)
	// uses (issue #310's pattern, the one this issue's AC names explicitly). Unlike
	// the quest tracker's corner-pinned footprint (which risks running off the RIGHT
	// edge), a centred block's risk is running wider/taller than the viewport itself,
	// so the check is "content width stays comfortably under full screen width" at
	// both extremes rather than a corner-margin fraction.
	const float MaxWidthFraction = 0.5f;
	const FVector2D TargetResolutions[] = { FVector2D(1280.0f, 720.0f), FVector2D(3840.0f, 2160.0f) };
	for (const FVector2D& TargetResolution : TargetResolutions)
	{
		TestTrue(*FString::Printf(TEXT("Content block width should stay within %.0f%% of screen width at %dx%d"),
			MaxWidthFraction * 100.0f, (int32)TargetResolution.X, (int32)TargetResolution.Y),
			UPostRunSummaryWidget::ContentWidthPx <= TargetResolution.X * MaxWidthFraction);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
