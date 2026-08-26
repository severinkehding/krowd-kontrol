// Confirms issue #320: the post-run summary screen's RERUN LEVEL button is wired to
// the shared reload path and degrades safely with no owning player.
//
// (a) Real click wiring: with a real AKrowdKontrolPlayerController owning the widget,
// RerunButton exists, its OnClicked delegate is bound, and clicking it (via
// HandleRerunClicked(), friend access) invokes the same
// AKrowdKontrolPlayerController::RequestLevelRestart() path HandleLevelFailed()
// (defeat, issue #172/#223) and HandleNextLevelClicked()'s final-level branch (#321)
// already use - proven via WasRestartRequested(), the same real accessor those tests
// use. The real UGameplayStatics::OpenLevel() call inside RequestLevelRestart() stays
// unreachable here since CreateNewMap() test Worlds are never game worlds - same
// documented limitation as KrowdKontrolLevelRestartTest.cpp. Also exercises the real
// HandleLevelClear() path (not just HandleRerunClicked() directly) with this same real
// owning player/LocalPlayer, so the SetInputMode()/focus-on-RerunButton wiring added by
// this issue actually executes under test instead of being silently skipped (as it is
// in KrowdKontrolPostRunSummaryWidgetWiringTest.cpp, whose widget has no owning player).
//
// (b) No owning player: a bare CreateWidget<T>(World, ...) construction (mirrors
// KrowdKontrolMainMenuWidgetTest.cpp's identical HandleQuitClicked() null-owner case)
// must not crash when HandleRerunClicked() is called directly, and the
// bHasWarnedMissingOwningControllerOnRerun one-shot guard must actually be one-shot -
// calling it twice must only log once (mirrors
// KrowdKontrolAbilityUnlockLevelSubsystemTest.cpp's AddExpectedError(..., 1, false)
// idiom for proving the same guard shape).
//
// (c) Layout order: RerunButton must render below the info block (CrowdMasteryText,
// the last info-block element) and above NextLevelButton in the vertical layout, per
// this issue's AC.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "PostRunSummaryWidget.h"
#include "KrowdKontrolPlayerController.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolPostRunSummaryRerunButtonTest,
	"KrowdKontrol.Unit.PostRunSummaryRerunButton",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolPostRunSummaryRerunButtonTest::RunTest(const FString& Parameters)
{
	// (a) Real click wiring: clicking RERUN LEVEL invokes the shared restart path.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		World->InitializeActorsForPlay(FURL());

		AKrowdKontrolPlayerController* Controller = World->SpawnActor<AKrowdKontrolPlayerController>();
		if (!TestNotNull(TEXT("Controller should spawn"), Controller))
		{
			return false;
		}
		Controller->Player = NewObject<ULocalPlayer>(GEngine);
		Controller->SetAsLocalPlayerController();
		Controller->DispatchBeginPlay(); // auto-creates Controller->PostRunSummaryWidgetInstance via CreateHUDWidgets()

		if (!TestNotNull(TEXT("PostRunSummaryWidgetInstance should be created by CreateHUDWidgets()"),
			ToRawPtr(Controller->PostRunSummaryWidgetInstance)))
		{
			return false;
		}

		UPostRunSummaryWidget* Widget = Controller->PostRunSummaryWidgetInstance;
		if (!TestNotNull(TEXT("RerunButton should be constructed"), ToRawPtr(Widget->RerunButton)))
		{
			return false;
		}
		TestTrue(TEXT("RerunButton->OnClicked should be bound to HandleRerunClicked"),
			Widget->RerunButton->OnClicked.IsBound());
		TestEqual(TEXT("RerunButtonLabel should read RERUN LEVEL"),
			Widget->RerunButtonLabel->GetText().ToString(), FString(TEXT("RERUN LEVEL")));

		// Exercise the real HandleLevelClear() path (not just HandleRerunClicked()
		// directly) with a real owning player/LocalPlayer, so the new
		// SetInputMode()/focus-on-RerunButton wiring added in this issue actually
		// executes under test, not just skipped because GetOwningPlayer() is null (as
		// it is in KrowdKontrolPostRunSummaryWidgetWiringTest.cpp). NOTE: this only
		// proves the call doesn't crash, not that focus actually lands on RerunButton -
		// no existing test in this codebase exercises FSlateApplication's focus APIs,
		// and headless -nullrhi Automation's Slate focus reporting has no established
		// precedent here to build on (unlike KrowdKontrolLevelRestartTest.cpp's
		// documented OpenLevel()/PIE-hang gap). Acknowledged gap, not silently missing.
		Widget->HandleLevelClear();
		TestNotNull(TEXT("RerunButton should still be valid after HandleLevelClear() runs with a real owning player"),
			ToRawPtr(Widget->RerunButton));

		TestFalse(TEXT("bRestartRequested should be false before the button is clicked"),
			Controller->WasRestartRequested());
		Widget->HandleRerunClicked();
		TestTrue(TEXT("Clicking RERUN LEVEL should invoke the shared RequestLevelRestart() path"),
			Controller->WasRestartRequested());
	}

	// (b) No owning player: must not crash, and the warning must fire exactly once even
	// across repeated clicks (bHasWarnedMissingOwningControllerOnRerun).
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

		TestNull(TEXT("GetOwningPlayer should be null for a bare CreateWidget(World, ...) construction"),
			Widget->GetOwningPlayer());

		AddExpectedError(TEXT("owning player is not an AKrowdKontrolPlayerController"),
			EAutomationExpectedErrorFlags::Contains, 1, false);
		Widget->HandleRerunClicked(); // must not crash
		Widget->HandleRerunClicked(); // must not log again - the AddExpectedError count of 1 above proves it
	}

	// (c) Layout order: RerunButton must render above NextLevelButton (this issue's AC).
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

		UPanelWidget* Layout = Widget->RootBorder ? Cast<UPanelWidget>(Widget->RootBorder->GetContent()) : nullptr;
		if (!TestNotNull(TEXT("RootBorder's content should be the layout panel widget"), Layout))
		{
			return false;
		}

		const int32 CrowdMasteryIndex = Layout->GetChildIndex(Widget->CrowdMasteryText);
		const int32 RerunIndex = Layout->GetChildIndex(Widget->RerunButton);
		const int32 NextLevelIndex = Layout->GetChildIndex(Widget->NextLevelButton);
		TestTrue(TEXT("RerunButton should be positioned below the info block and above NextLevelButton in the layout"),
			CrowdMasteryIndex >= 0 && RerunIndex >= 0 && NextLevelIndex >= 0
				&& CrowdMasteryIndex < RerunIndex && RerunIndex < NextLevelIndex);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
