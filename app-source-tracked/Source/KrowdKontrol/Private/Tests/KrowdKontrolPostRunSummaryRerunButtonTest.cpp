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
// documented limitation as KrowdKontrolLevelRestartTest.cpp.
//
// (b) No owning player: a bare CreateWidget<T>(World, ...) construction (mirrors
// KrowdKontrolMainMenuWidgetTest.cpp's identical HandleQuitClicked() null-owner case)
// must not crash when HandleRerunClicked() is called directly.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "PostRunSummaryWidget.h"
#include "KrowdKontrolPlayerController.h"
#include "Components/Button.h"
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

		TestFalse(TEXT("bRestartRequested should be false before the button is clicked"),
			Controller->WasRestartRequested());
		Widget->HandleRerunClicked();
		TestTrue(TEXT("Clicking RERUN LEVEL should invoke the shared RequestLevelRestart() path"),
			Controller->WasRestartRequested());
	}

	// (b) No owning player: must not crash.
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
		Widget->HandleRerunClicked(); // must not crash
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
