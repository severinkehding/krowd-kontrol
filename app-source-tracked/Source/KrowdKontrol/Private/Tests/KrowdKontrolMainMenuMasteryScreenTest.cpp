// Confirms issue #373 (docs/prd-mastery-skill-tree.md REQ-2 scaffolding):
// UMainMenuWidget's MASTERY button navigates to a lazily-created UMasteryScreenWidget,
// hiding the menu's own RootBorder while it's shown, and that UMasteryScreenWidget's
// OnBackRequested restores the menu with no duplicate bind across repeat clicks.
// Mirrors KrowdKontrolMainMenuMasteryResetTest.cpp's header/boilerplate shape.

#include "Misc/AutomationTest.h"
#include "MainMenuWidget.h"
#include "MasteryScreenWidget.h"
#include "MasteryScreenBackRequestedTestListener.h"
#include "CrowdMasteryTotalSubsystem.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Components/Button.h"
#include "Components/Border.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolMainMenuMasteryScreenTest,
	"KrowdKontrol.Unit.MainMenuMasteryScreen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolMainMenuMasteryScreenTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	UMainMenuWidget* Widget = CreateWidget<UMainMenuWidget>(World, UMainMenuWidget::StaticClass());
	if (!TestNotNull(TEXT("UMainMenuWidget should construct"), Widget))
	{
		return false;
	}

	// (a) MasteryButton is non-null and wired to a real handler.
	if (!TestNotNull(TEXT("MasteryButton should be non-null"), ToRawPtr(Widget->MasteryButton)))
	{
		return false;
	}
	TestTrue(TEXT("MasteryButton::OnClicked should be bound"), Widget->MasteryButton->OnClicked.IsBound());
	TestNull(TEXT("MasteryScreenWidgetInstance should be null before the first click"), ToRawPtr(Widget->MasteryScreenWidgetInstance));

	// (b) Clicking MASTERY creates the screen and hides the menu's own root.
	Widget->HandleMasteryButtonClicked();
	if (!TestNotNull(TEXT("MasteryScreenWidgetInstance should be non-null after HandleMasteryButtonClicked()"),
		ToRawPtr(Widget->MasteryScreenWidgetInstance)))
	{
		return false;
	}
	if (TestNotNull(TEXT("RootBorder should be non-null"), ToRawPtr(Widget->RootBorder)))
	{
		TestEqual(TEXT("RootBorder should be Collapsed once the mastery screen is shown"),
			Widget->RootBorder->GetVisibility(), ESlateVisibility::Collapsed);
	}

	// (c) The newly-created screen's points text reflects an injected
	// UCrowdMasteryTotalSubsystem total, via the same friend-accessible cache seam
	// KrowdKontrolMainMenuMasteryResetTest.cpp uses for UMainMenuWidget itself.
	UMasteryScreenWidget* ScreenInstance = Widget->MasteryScreenWidgetInstance;
	UGameInstance* GameInstanceOuter = NewObject<UGameInstance>();
	UCrowdMasteryTotalSubsystem* InjectedSubsystem = NewObject<UCrowdMasteryTotalSubsystem>(GameInstanceOuter);
	InjectedSubsystem->DepositRunMastery(15);
	ScreenInstance->CachedMasteryTotalSubsystem = InjectedSubsystem;
	ScreenInstance->RefreshPointsDisplayText();
	TestEqual(TEXT("Mastery screen points display should reflect the injected subsystem's real total"),
		ScreenInstance->GetPointsDisplayText().ToString(), FString(TEXT("UNSPENT POINTS: 15")));

	// (d) Broadcasting OnBackRequested (simulating a real BACK click) restores
	// RootBorder to Visible, with no subsystem state touched.
	ScreenInstance->OnBackRequested.Broadcast();
	if (TestNotNull(TEXT("RootBorder should still be non-null"), ToRawPtr(Widget->RootBorder)))
	{
		TestEqual(TEXT("RootBorder should be Visible again after BACK"),
			Widget->RootBorder->GetVisibility(), ESlateVisibility::Visible);
	}
	TestEqual(TEXT("BACK should never touch the mastery total"), InjectedSubsystem->GetAccumulatedTotal(), 15);

	// (e) A second MASTERY click reuses the same screen instance (no duplicate
	// CreateWidget call) - a regression here would return a different pointer, since
	// HandleMasteryButtonClicked()'s CreateWidget+AddDynamic pair only runs inside the
	// `if (!MasteryScreenWidgetInstance)` first-creation branch. A test-side listener
	// bound alongside the real HandleMasteryScreenBackRequested binding then confirms
	// a single OnBackRequested.Broadcast() reaches every subscriber exactly once.
	Widget->HandleMasteryButtonClicked();
	TestEqual(TEXT("Second click should reuse the same MasteryScreenWidgetInstance"),
		ToRawPtr(Widget->MasteryScreenWidgetInstance), ToRawPtr(ScreenInstance));

	UMasteryScreenBackRequestedTestListener* Listener = NewObject<UMasteryScreenBackRequestedTestListener>();
	ScreenInstance->OnBackRequested.AddDynamic(Listener, &UMasteryScreenBackRequestedTestListener::HandleBackRequested);
	ScreenInstance->OnBackRequested.Broadcast();
	TestEqual(TEXT("A single Broadcast() should reach the test listener exactly once"), Listener->CallCount, 1);
	if (TestNotNull(TEXT("RootBorder should still be non-null after the second restore"), ToRawPtr(Widget->RootBorder)))
	{
		TestEqual(TEXT("RootBorder should be Visible again after the second BACK"),
			Widget->RootBorder->GetVisibility(), ESlateVisibility::Visible);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
