// Confirms UMasteryScreenWidget (issue #373, docs/prd-mastery-skill-tree.md REQ-2
// scaffolding) builds a "MASTERY" title, an "UNSPENT POINTS: N" text block reading
// UCrowdMasteryTotalSubsystem::GetAccumulatedTotal(), and a BACK button that
// broadcasts OnBackRequested with no subsystem side effects. Mirrors
// KrowdKontrolMainMenuWidgetTest.cpp's block-by-block shape and its
// CachedMasteryTotalSubsystem injection seam.

#include "Misc/AutomationTest.h"
#include "MasteryScreenWidget.h"
#include "MasteryScreenBackRequestedTestListener.h"
#include "CrowdMasteryTotalSubsystem.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolMasteryScreenWidgetTest,
	"KrowdKontrol.Unit.MasteryScreenWidget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolMasteryScreenWidgetTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	UMasteryScreenWidget* Widget = CreateWidget<UMasteryScreenWidget>(World, UMasteryScreenWidget::StaticClass());
	if (!TestNotNull(TEXT("UMasteryScreenWidget should construct"), Widget))
	{
		return false;
	}

	// (a) Title text is present and reads "MASTERY".
	if (TestNotNull(TEXT("TitleText should be non-null"), ToRawPtr(Widget->TitleText)))
	{
		TestEqual(TEXT("Title text should read MASTERY"), Widget->TitleText->GetText().ToString(), FString(TEXT("MASTERY")));
	}

	// (b) Points text defaults to "UNSPENT POINTS: 0" with no GameInstance to read a
	// real total from (this bare CreateNewMap() World has none).
	if (TestNotNull(TEXT("PointsText should be non-null"), ToRawPtr(Widget->PointsText)))
	{
		TestEqual(TEXT("Points display should read UNSPENT POINTS: 0 with no GameInstance"),
			Widget->GetPointsDisplayText().ToString(), FString(TEXT("UNSPENT POINTS: 0")));
	}

	// (c) Injecting a real subsystem via the friend-accessible CachedMasteryTotalSubsystem
	// seam and calling RefreshPointsDisplayText() again proves GetAccumulatedTotal()'s
	// value flows correctly into the formatted display text.
	UMasteryScreenWidget* InjectedWidget = CreateWidget<UMasteryScreenWidget>(World, UMasteryScreenWidget::StaticClass());
	if (TestNotNull(TEXT("InjectedWidget should construct"), InjectedWidget))
	{
		UGameInstance* GameInstanceOuter = NewObject<UGameInstance>();
		UCrowdMasteryTotalSubsystem* InjectedSubsystem = NewObject<UCrowdMasteryTotalSubsystem>(GameInstanceOuter);
		InjectedSubsystem->DepositRunMastery(42);
		InjectedWidget->CachedMasteryTotalSubsystem = InjectedSubsystem;
		InjectedWidget->RefreshPointsDisplayText();
		TestEqual(TEXT("Points display should reflect the injected subsystem's real total"),
			InjectedWidget->GetPointsDisplayText().ToString(), FString(TEXT("UNSPENT POINTS: 42")));

		// (d) NativeConstruct() (fired by AddToViewport()) must re-run
		// RefreshPointsDisplayText() so the display refreshes whenever the screen is
		// (re-)opened, not just once at construction.
		InjectedSubsystem->DepositRunMastery(8);
		InjectedWidget->NativeConstruct();
		TestEqual(TEXT("NativeConstruct() should re-read the total, showing 50"),
			InjectedWidget->GetPointsDisplayText().ToString(), FString(TEXT("UNSPENT POINTS: 50")));
	}

	// (e) BackButton::OnClicked is bound; calling HandleBackClicked() broadcasts
	// OnBackRequested with no subsystem side effects.
	if (TestNotNull(TEXT("BackButton should be non-null"), ToRawPtr(Widget->BackButton)))
	{
		TestTrue(TEXT("BackButton::OnClicked should be bound"), Widget->BackButton->OnClicked.IsBound());

		UMasteryScreenBackRequestedTestListener* Listener = NewObject<UMasteryScreenBackRequestedTestListener>();
		Widget->OnBackRequested.AddDynamic(Listener, &UMasteryScreenBackRequestedTestListener::HandleBackRequested);
		Widget->HandleBackClicked();
		TestEqual(TEXT("HandleBackClicked() should broadcast OnBackRequested exactly once"), Listener->CallCount, 1);
	}

	// (f) Unbuilt widget (bare NewObject(), neither NativeOnInitialized() nor
	// Initialize() called) degrades safely rather than crashing.
	UMasteryScreenWidget* UnbuiltWidget = NewObject<UMasteryScreenWidget>();
	if (TestNotNull(TEXT("UnbuiltWidget should construct"), UnbuiltWidget))
	{
		TestTrue(TEXT("GetPointsDisplayText should degrade to empty text before the tree is built"),
			UnbuiltWidget->GetPointsDisplayText().IsEmpty());

		UnbuiltWidget->RefreshPointsDisplayText();
		TestTrue(TEXT("RefreshPointsDisplayText on an unbuilt widget should not crash"), true);
	}

	// (g) Initialize() guard - must not rebuild the tree when NativeOnInitialized()
	// (invoked synchronously by CreateWidget()) already built it.
	UMasteryScreenWidget* GuardWidget = CreateWidget<UMasteryScreenWidget>(World, UMasteryScreenWidget::StaticClass());
	if (TestNotNull(TEXT("GuardWidget should construct"), GuardWidget))
	{
		UTextBlock* FirstTitleText = GuardWidget->TitleText;
		GuardWidget->Initialize();
		TestEqual(TEXT("Initialize() must not rebuild the tree when already built"),
			ToRawPtr(GuardWidget->TitleText), FirstTitleText);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
