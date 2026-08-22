// Confirms UBriefingCardWidget (issue #246) populates its three text fields from a
// FLevelBriefingRow via ShowBriefing(), collapses the optional new-ability line when
// empty, auto-dismisses at exactly the 8s cap (BriefingAutoDismissSeconds), and that
// NativeTick actually drives the countdown. Also confirms an unbuilt widget tree
// degrades to safe defaults rather than crashing, and the Initialize()/
// NativeOnInitialized() guard builds the tree exactly once regardless of call order -
// same reasoning as KrowdKontrolOnScreenPromptWidgetTest.cpp/
// KrowdKontrolPostRunSummaryWidgetTest.cpp.
//
// CreateWidget() calls Initialize() synchronously, which fires NativeOnInitialized() -
// no TakeWidget()/AddToViewport()/Slate realization needed, so this works under the
// -nullrhi flag KrowdKontrol.Unit.* tests run with. Needs a real UWorld (CreateWidget's
// first argument). The Initialize()-guard and unbuilt-tree cases use a bare
// NewObject() instead (no World needed).
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "BriefingCardWidget.h"
#include "LevelBriefingData.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolBriefingCardWidgetTest,
	"KrowdKontrol.Unit.BriefingCardWidget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolBriefingCardWidgetTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	// (a) Widget constructs.
	UBriefingCardWidget* Widget =
		CreateWidget<UBriefingCardWidget>(World, UBriefingCardWidget::StaticClass());
	if (!TestNotNull(TEXT("UBriefingCardWidget should construct"), Widget))
	{
		return false;
	}

	// (b) Idle state immediately after construction.
	TestFalse(TEXT("Briefing should not be visible immediately after construction"), Widget->IsBriefingVisible());
	TestTrue(TEXT("Level name display text should be empty immediately after construction"), Widget->GetLevelNameDisplayText().IsEmpty());
	TestTrue(TEXT("Objective display text should be empty immediately after construction"), Widget->GetObjectiveDisplayText().IsEmpty());
	TestTrue(TEXT("New-ability display text should be empty immediately after construction"), Widget->GetNewAbilityDisplayText().IsEmpty());

	// (c) ShowBriefing() with a full row - all three fields populate, timer starts
	// at the full 8s cap.
	FLevelBriefingRow FullRow;
	FullRow.LevelDisplayName = FText::FromString(TEXT("LEVEL 1"));
	FullRow.ObjectiveLines.Add(FText::FromString(TEXT("PACIFY ALL 8 ROBOTS")));
	FullRow.ObjectiveLines.Add(FText::FromString(TEXT("STUN THEM, HERD THEM TO THEIR PENS")));
	FullRow.NewAbilityUnlockLine = FText::FromString(TEXT("NEW: SLEEP - PRESS 2 - STRONG VS SNIPERS"));

	Widget->ShowBriefing(FullRow);
	TestTrue(TEXT("Briefing should be visible after ShowBriefing()"), Widget->IsBriefingVisible());
	TestEqual(TEXT("Level name should match the row"),
		Widget->GetLevelNameDisplayText().ToString(), TEXT("LEVEL 1"));
	TestEqual(TEXT("Objective text should join both objective lines with a newline"),
		Widget->GetObjectiveDisplayText().ToString(), TEXT("PACIFY ALL 8 ROBOTS\nSTUN THEM, HERD THEM TO THEIR PENS"));
	TestEqual(TEXT("New-ability text should match the row"),
		Widget->GetNewAbilityDisplayText().ToString(), TEXT("NEW: SLEEP - PRESS 2 - STRONG VS SNIPERS"));
	TestTrue(TEXT("New-ability text visibility should be HitTestInvisible when populated"),
		Widget->NewAbilityText->GetVisibility() == ESlateVisibility::HitTestInvisible);
	// UGameplayStatics::SetGamePaused() requires a live AGameModeBase
	// (World->GetAuthGameMode()), which CreateNewMap() test Worlds never spawn - see
	// KrowdKontrolAbilityCastComponentTest.cpp's identical note. Documenting the
	// no-op explicitly here rather than leaving ShowBriefing()'s pause side effect
	// completely unchecked.
	TestFalse(TEXT("SetGamePaused() is a documented no-op in CreateNewMap() worlds without a GameMode - see KrowdKontrolAbilityCastComponentTest.cpp"),
		World->IsPaused());

	// (d) ShowBriefing() with an empty NewAbilityUnlockLine - proves the "optional"
	// half of the AC: the new-ability line collapses instead of showing blank.
	FLevelBriefingRow RowWithoutUnlock;
	RowWithoutUnlock.LevelDisplayName = FText::FromString(TEXT("LEVEL 2"));
	RowWithoutUnlock.ObjectiveLines.Add(FText::FromString(TEXT("SURVIVE THE AMBUSH")));

	Widget->ShowBriefing(RowWithoutUnlock);
	TestTrue(TEXT("New-ability display text should be empty when the row's unlock line is empty"),
		Widget->GetNewAbilityDisplayText().IsEmpty());
	TestTrue(TEXT("New-ability text visibility should be Collapsed when the row's unlock line is empty"),
		Widget->NewAbilityText->GetVisibility() == ESlateVisibility::Collapsed);

	// (e) AdvanceDismissTimer countdown across two calls dismisses at exactly 8s,
	// not before/after.
	Widget->ShowBriefing(FullRow);
	TestEqual(TEXT("Remaining seconds should equal BriefingAutoDismissSeconds after ShowBriefing()"),
		Widget->RemainingSeconds, UBriefingCardWidget::BriefingAutoDismissSeconds);
	Widget->AdvanceDismissTimer(4.0f);
	TestTrue(TEXT("Briefing should still be visible after 4s of an 8s briefing"), Widget->IsBriefingVisible());
	TestEqual(TEXT("Remaining seconds should be 4.0f after 4s"), Widget->RemainingSeconds, 4.0f);
	Widget->AdvanceDismissTimer(4.0f);
	TestFalse(TEXT("Briefing should no longer be visible after the full 8s"), Widget->IsBriefingVisible());
	TestEqual(TEXT("Remaining seconds should be 0 after the full 8s"), Widget->RemainingSeconds, 0.0f);
	TestTrue(TEXT("Level name display text should be empty again after dismissal"), Widget->GetLevelNameDisplayText().IsEmpty());
	TestTrue(TEXT("Root border visibility should be Collapsed after dismissal"),
		Widget->RootBorder->GetVisibility() == ESlateVisibility::Collapsed);
	// Same documented no-op as above, for DismissBriefing()'s unpause call.
	TestFalse(TEXT("SetGamePaused(false) is likewise a documented no-op here - see KrowdKontrolAbilityCastComponentTest.cpp"),
		World->IsPaused());

	// (f) NativeTick actually drives the timer - the real per-frame code path a live
	// game session ticks, not just the direct AdvanceDismissTimer() calls used above.
	Widget->ShowBriefing(FullRow);
	Widget->NativeTick(FGeometry(), 1.0f);
	TestEqual(TEXT("NativeTick should advance the timer via AdvanceDismissTimer()"),
		Widget->RemainingSeconds, UBriefingCardWidget::BriefingAutoDismissSeconds - 1.0f);
	// Idle NativeTick should not perturb an already-zero timer.
	Widget->AdvanceDismissTimer(UBriefingCardWidget::BriefingAutoDismissSeconds);
	TestEqual(TEXT("Timer should be 0 after fully dismissing"), Widget->RemainingSeconds, 0.0f);
	Widget->NativeTick(FGeometry(), 1.0f);
	TestEqual(TEXT("NativeTick while idle should not change remaining seconds away from 0"),
		Widget->RemainingSeconds, 0.0f);

	// (g) Initialize() guard - must not rebuild the tree when NativeOnInitialized()
	// already ran, and the reverse call order too.
	UBriefingCardWidget* GuardWidget = NewObject<UBriefingCardWidget>();
	if (!TestNotNull(TEXT("UBriefingCardWidget should construct for guard test"), GuardWidget))
	{
		return false;
	}
	GuardWidget->NativeOnInitialized();
	UBorder* FirstRootBorder = GuardWidget->RootBorder;
	if (!TestNotNull(TEXT("RootBorder should be set after NativeOnInitialized()"), FirstRootBorder))
	{
		return false;
	}
	GuardWidget->Initialize();
	TestEqual(TEXT("Initialize() must not rebuild the tree when already built"),
		ToRawPtr(GuardWidget->RootBorder), FirstRootBorder);

	UBriefingCardWidget* ReverseGuardWidget = NewObject<UBriefingCardWidget>();
	if (!TestNotNull(TEXT("UBriefingCardWidget should construct for reverse guard test"), ReverseGuardWidget))
	{
		return false;
	}
	ReverseGuardWidget->Initialize();
	UBorder* ReverseFirstRootBorder = ReverseGuardWidget->RootBorder;
	if (!TestNotNull(TEXT("RootBorder should be set after Initialize()"), ReverseFirstRootBorder))
	{
		return false;
	}
	ReverseGuardWidget->NativeOnInitialized();
	TestEqual(TEXT("NativeOnInitialized() must not rebuild the tree when already built"),
		ToRawPtr(ReverseGuardWidget->RootBorder), ReverseFirstRootBorder);

	// (h) Unbuilt-tree safety - a widget whose tree was never built (bare
	// NewObject(), neither NativeOnInitialized() nor Initialize() called) should
	// degrade safely rather than crashing.
	UBriefingCardWidget* UnbuiltWidget = NewObject<UBriefingCardWidget>();
	if (!TestNotNull(TEXT("UBriefingCardWidget should construct for unbuilt-tree test"), UnbuiltWidget))
	{
		return false;
	}
	TestFalse(TEXT("Unbuilt widget should report not visible"), UnbuiltWidget->IsBriefingVisible());
	TestTrue(TEXT("Unbuilt widget should report empty level name text"), UnbuiltWidget->GetLevelNameDisplayText().IsEmpty());
	TestTrue(TEXT("Unbuilt widget should report empty objective text"), UnbuiltWidget->GetObjectiveDisplayText().IsEmpty());
	TestTrue(TEXT("Unbuilt widget should report empty new-ability text"), UnbuiltWidget->GetNewAbilityDisplayText().IsEmpty());
	UnbuiltWidget->ShowBriefing(FullRow);
	UnbuiltWidget->AdvanceDismissTimer(1.0f);
	UnbuiltWidget->DismissBriefing();
	TestTrue(TEXT("ShowBriefing()/AdvanceDismissTimer()/DismissBriefing() on an unbuilt tree should not crash"), true);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
