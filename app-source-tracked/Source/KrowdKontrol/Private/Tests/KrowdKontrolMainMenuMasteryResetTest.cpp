// Confirms issue #329 (docs/prd-crowd-mastery-persistence.md REQ-3): UMainMenuWidget's
// RESET control for the accumulated Crowd Mastery total. Covers the full inline
// confirm/cancel state machine (RESET -> CONFIRM RESET/CANCEL -> back to RESET) and its
// wiring to UCrowdMasteryTotalSubsystem::ResetAccumulatedTotal(), without needing a real
// GetGameInstance() (this suite's CreateNewMap() Editor Worlds have none - same
// documented limitation as KrowdKontrolPostRunSummaryWidgetTest.cpp's Resolve...()
// coverage). Case (e) injects a real subsystem instance directly into the
// friend-accessible cache seam to prove the actual reset wiring works end to end.

#include "Misc/AutomationTest.h"
#include "MainMenuWidget.h"
#include "CrowdMasteryTotalSubsystem.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Components/Button.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolMainMenuMasteryResetTest,
	"KrowdKontrol.Unit.MainMenuMasteryReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolMainMenuMasteryResetTest::RunTest(const FString& Parameters)
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

	// (a) Initial state: RESET visible, CONFIRM/CANCEL collapsed, not armed.
	if (TestNotNull(TEXT("MasteryResetButton should be non-null"), ToRawPtr(Widget->MasteryResetButton))
		&& TestNotNull(TEXT("MasteryResetConfirmButton should be non-null"), ToRawPtr(Widget->MasteryResetConfirmButton))
		&& TestNotNull(TEXT("MasteryResetCancelButton should be non-null"), ToRawPtr(Widget->MasteryResetCancelButton)))
	{
		TestFalse(TEXT("bMasteryResetConfirmPending should start false"), Widget->bMasteryResetConfirmPending);
		TestEqual(TEXT("MasteryResetButton should start Visible"),
			Widget->MasteryResetButton->GetVisibility(), ESlateVisibility::Visible);
		TestEqual(TEXT("MasteryResetConfirmButton should start Collapsed"),
			Widget->MasteryResetConfirmButton->GetVisibility(), ESlateVisibility::Collapsed);
		TestEqual(TEXT("MasteryResetCancelButton should start Collapsed"),
			Widget->MasteryResetCancelButton->GetVisibility(), ESlateVisibility::Collapsed);
	}

	// (b) Clicking RESET arms the confirm step.
	Widget->HandleMasteryResetClicked();
	TestTrue(TEXT("bMasteryResetConfirmPending should be true after RESET is clicked"), Widget->bMasteryResetConfirmPending);
	TestEqual(TEXT("MasteryResetButton should be Collapsed once armed"),
		Widget->MasteryResetButton->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("MasteryResetConfirmButton should be Visible once armed"),
		Widget->MasteryResetConfirmButton->GetVisibility(), ESlateVisibility::Visible);
	TestEqual(TEXT("MasteryResetCancelButton should be Visible once armed"),
		Widget->MasteryResetCancelButton->GetVisibility(), ESlateVisibility::Visible);

	// (c) CANCEL disarms without ever touching the subsystem-resolution seam - assert
	// this BEFORE case (d) registers its AddExpectedError, so a false pass here can't be
	// masked by that registration (AddExpectedError counts apply across the whole
	// RunTest(), not per-block).
	Widget->HandleMasteryResetCancelClicked();
	TestFalse(TEXT("bMasteryResetConfirmPending should be false after CANCEL"), Widget->bMasteryResetConfirmPending);
	TestEqual(TEXT("MasteryResetButton should be Visible again after CANCEL"),
		Widget->MasteryResetButton->GetVisibility(), ESlateVisibility::Visible);
	TestEqual(TEXT("MasteryResetConfirmButton should be Collapsed again after CANCEL"),
		Widget->MasteryResetConfirmButton->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("MasteryResetCancelButton should be Collapsed again after CANCEL"),
		Widget->MasteryResetCancelButton->GetVisibility(), ESlateVisibility::Collapsed);
	TestNull(TEXT("CANCEL should never resolve the mastery-total subsystem"), ToRawPtr(Widget->CachedMasteryTotalSubsystem));
	TestFalse(TEXT("CANCEL should never trigger the missing-subsystem warning"), Widget->bHasWarnedMissingMasteryTotalSubsystem);

	// (d) CONFIRM with no real GameInstance (this suite's CreateNewMap() Worlds have
	// none) degrades safely and warns exactly once, even across two confirm attempts.
	AddExpectedError(TEXT("no UCrowdMasteryTotalSubsystem available"), EAutomationExpectedErrorFlags::Contains, 1);
	Widget->HandleMasteryResetClicked();
	Widget->HandleMasteryResetConfirmClicked();
	TestFalse(TEXT("bMasteryResetConfirmPending should be false after CONFIRM"), Widget->bMasteryResetConfirmPending);
	TestEqual(TEXT("MasteryResetButton should be Visible again after CONFIRM"),
		Widget->MasteryResetButton->GetVisibility(), ESlateVisibility::Visible);
	Widget->HandleMasteryResetClicked();
	Widget->HandleMasteryResetConfirmClicked();
	TestFalse(TEXT("bMasteryResetConfirmPending should be false after a second CONFIRM"), Widget->bMasteryResetConfirmPending);
	TestEqual(TEXT("MasteryResetButton should be Visible again after a second CONFIRM"),
		Widget->MasteryResetButton->GetVisibility(), ESlateVisibility::Visible);

	// (e) CONFIRM actually resets a real, injected subsystem - the key behavioral proof,
	// since the real GetGameInstance()-based resolution path is untestable in
	// CreateNewMap() worlds (same documented limitation as issue #327's coverage).
	UMainMenuWidget* SecondWidget = CreateWidget<UMainMenuWidget>(World, UMainMenuWidget::StaticClass());
	if (TestNotNull(TEXT("SecondWidget should construct"), SecondWidget))
	{
		UGameInstance* GameInstanceOuter = NewObject<UGameInstance>();
		UCrowdMasteryTotalSubsystem* InjectedSubsystem = NewObject<UCrowdMasteryTotalSubsystem>(GameInstanceOuter);
		InjectedSubsystem->DepositRunMastery(7);
		SecondWidget->CachedMasteryTotalSubsystem = InjectedSubsystem;

		SecondWidget->HandleMasteryResetClicked();
		SecondWidget->HandleMasteryResetConfirmClicked();
		TestEqual(TEXT("Confirming reset should zero the injected subsystem's total"),
			InjectedSubsystem->GetAccumulatedTotal(), 0);
		TestFalse(TEXT("bMasteryResetConfirmPending should be false after CONFIRM on SecondWidget"),
			SecondWidget->bMasteryResetConfirmPending);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
