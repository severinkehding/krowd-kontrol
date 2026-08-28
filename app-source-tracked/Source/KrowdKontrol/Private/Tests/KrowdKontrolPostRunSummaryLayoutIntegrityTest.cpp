// Confirms issue #322: the post-run summary screen's information block (clear time /
// best time / Crowd Mastery) and its two buttons (RERUN LEVEL, NEXT LEVEL) never
// overlap and never clip off-screen, at both the 1280x720 minimum and 3840x2160
// maximum target resolutions - as one dedicated, standing contract independent of the
// narrower tests issues #319/#320/#321 already shipped for their own individual
// changes (see app-changelog/issue-319.md: "no button-overlap verification
// (explicitly out of scope, deferred to a separate layout-integrity issue)").
//
// This project's KrowdKontrol.Unit.* tests run headless (-nullrhi, CreateNewMap()
// Editor World, no live Slate/viewport realization - AddToViewport() is a documented
// no-op here, see KrowdKontrolEnergyMeterWidgetTest.cpp case (13)), so "bounding box"
// is proven analytically rather than via live geometry queries:
//
// (a) Non-overlap (structural): ClearTimeText, BestClearTimeText, CrowdMasteryText,
// RerunButton, and NextLevelButton are all direct children of the single
// UVerticalBox ("Layout") wrapped by RootBorder (see BuildWidgetTree()). A
// UVerticalBox stacks children into non-overlapping rows by construction, so proving
// all five share that one parent, with the info block's three fields ordered before
// both buttons (and RerunButton before NextLevelButton), is the overlap guarantee -
// this extends KrowdKontrolPostRunSummaryRerunButtonTest.cpp case (c)'s
// GetChildIndex() ordering check (CrowdMasteryText/RerunButton only) to all five
// elements as its own dedicated contract.
//
// (b) On-screen containment (analytical): the whole content block sits in a USizeBox
// with its width fixed and its height capped at ContentWidthPx x ContentHeightPx
// respectively, centred via a UCanvasPanelSlot with
// anchors/alignment (0.5, 0.5) and AutoSize(true) (issue #319). A rectangle of size
// (W, H) centred in a screen of size (Sw, Sh) has TopLeft = ((Sw-W)/2, (Sh-H)/2) and
// BottomRight = ((Sw+W)/2, (Sh+H)/2); both stay within [0, Sw] x [0, Sh] iff W <= Sw
// and H <= Sh. So "never clips off any edge" reduces to ContentWidthPx <= ScreenWidth
// and ContentHeightPx <= ScreenHeight, checked at both 1280x720 and 3840x2160. This is
// a strictly stronger, explicitly-named version of
// KrowdKontrolPostRunSummaryWidgetTest.cpp case (h)'s existing 50%-fraction margin
// check (which already implies this holds, but never asserts "fits inside 100% of the
// screen" as its own named contract).
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "PostRunSummaryWidget.h"
#include "Tests/AutomationEditorCommon.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolPostRunSummaryLayoutIntegrityTest,
	"KrowdKontrol.Unit.PostRunSummaryLayoutIntegrity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolPostRunSummaryLayoutIntegrityTest::RunTest(const FString& Parameters)
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

	// (a) Non-overlap: all five elements share the one VerticalBox parent, in an order
	// that keeps the info block above both buttons and RerunButton above NextLevelButton.
	UPanelWidget* Layout = Widget->RootBorder ? Cast<UPanelWidget>(Widget->RootBorder->GetContent()) : nullptr;
	if (!TestNotNull(TEXT("RootBorder's content should be the layout panel widget"), Layout))
	{
		return false;
	}

	const int32 ClearTimeIndex = Layout->GetChildIndex(Widget->ClearTimeText);
	const int32 BestClearTimeIndex = Layout->GetChildIndex(Widget->BestClearTimeText);
	const int32 CrowdMasteryIndex = Layout->GetChildIndex(Widget->CrowdMasteryText);
	const int32 RerunIndex = Layout->GetChildIndex(Widget->RerunButton);
	const int32 NextLevelIndex = Layout->GetChildIndex(Widget->NextLevelButton);

	TestTrue(TEXT("All five elements should share the single layout VerticalBox parent"),
		ClearTimeIndex >= 0 && BestClearTimeIndex >= 0 && CrowdMasteryIndex >= 0 && RerunIndex >= 0 && NextLevelIndex >= 0);

	TestTrue(TEXT("Info block (clear time, best time, Crowd Mastery) should be positioned above both buttons"),
		ClearTimeIndex < RerunIndex && BestClearTimeIndex < RerunIndex && CrowdMasteryIndex < RerunIndex
			&& ClearTimeIndex < NextLevelIndex && BestClearTimeIndex < NextLevelIndex && CrowdMasteryIndex < NextLevelIndex);

	TestTrue(TEXT("RerunButton should be positioned above NextLevelButton"),
		RerunIndex < NextLevelIndex);

	// (b) On-screen containment: the centred content block must fit fully within the
	// screen at both target resolutions - no clipping off any edge.
	const FVector2D TargetResolutions[] = { FVector2D(1280.0f, 720.0f), FVector2D(3840.0f, 2160.0f) };
	for (const FVector2D& TargetResolution : TargetResolutions)
	{
		TestTrue(*FString::Printf(TEXT("Content block width should fit within the full screen width at %dx%d"),
			(int32)TargetResolution.X, (int32)TargetResolution.Y),
			UPostRunSummaryWidget::ContentWidthPx <= TargetResolution.X);
		TestTrue(*FString::Printf(TEXT("Content block height should fit within the full screen height at %dx%d"),
			(int32)TargetResolution.X, (int32)TargetResolution.Y),
			UPostRunSummaryWidget::ContentHeightPx <= TargetResolution.Y);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
