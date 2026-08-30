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
#include "MasteryScreenWidget.h"
#include "MasterySkillBubbleWidget.h"
#include "CrowdMasteryTotalSubsystem.h"
#include "MasteryTreeData.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/DataTable.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/HorizontalBox.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolMainMenuMasteryResetTest,
	"KrowdKontrol.Unit.MainMenuMasteryReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace KrowdKontrolMainMenuMasteryResetTest
{
	// Builds a 4-entry Bubbles array costing 1/2/3/4 points, named "<Prefix>_Bubble0..3" -
	// mirrors KrowdKontrolCrowdMasteryTotalSubsystemSpendTest.cpp's BuildFourBubbles() shape.
	TArray<FMasterySkillBubble> BuildFourBubbles(const FString& Prefix)
	{
		TArray<FMasterySkillBubble> Bubbles;
		for (int32 Index = 0; Index < 4; ++Index)
		{
			FMasterySkillBubble Bubble;
			Bubble.BubbleId = FName(*FString::Printf(TEXT("%s_Bubble%d"), *Prefix, Index));
			Bubble.DisplayName = FText::FromString(FString::Printf(TEXT("%s Skill %d"), *Prefix, Index));
			Bubble.PointCost = Index + 1;
			Bubble.EffectHookId = FName(*FString::Printf(TEXT("%s_Effect%d"), *Prefix, Index));
			Bubbles.Add(Bubble);
		}
		return Bubbles;
	}
}

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
		TestTrue(TEXT("MasteryResetButton::OnClicked should be bound"), Widget->MasteryResetButton->OnClicked.IsBound());
		TestTrue(TEXT("MasteryResetConfirmButton::OnClicked should be bound"), Widget->MasteryResetConfirmButton->OnClicked.IsBound());
		TestTrue(TEXT("MasteryResetCancelButton::OnClicked should be bound"), Widget->MasteryResetCancelButton->OnClicked.IsBound());
	}

	// (a2) MasteryResetBox should be positioned directly after MasteryDisplayAnchor in
	// Layout - the PR's own checked-off acceptance criterion, verified the same way
	// KrowdKontrolPostRunSummaryRerunButtonTest.cpp verifies RerunButton's position.
	UPanelWidget* Layout = Widget->RootBorder ? Cast<UPanelWidget>(Widget->RootBorder->GetContent()) : nullptr;
	if (TestNotNull(TEXT("RootBorder's content should be the layout panel widget"), Layout))
	{
		const int32 AnchorIndex = Layout->GetChildIndex(ToRawPtr(Widget->MasteryDisplayAnchor));
		const int32 ResetBoxIndex = Layout->GetChildIndex(ToRawPtr(Widget->MasteryResetBox));
		TestTrue(TEXT("MasteryResetBox should be positioned directly after MasteryDisplayAnchor"),
			AnchorIndex >= 0 && ResetBoxIndex == AnchorIndex + 1);
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
	TestEqual(TEXT("LastMasteryRespecCallOrder should stay empty when the subsystem fails to resolve"),
		Widget->LastMasteryRespecCallOrder.Num(), 0);

	// (e) CONFIRM actually resets a real, injected subsystem - the key behavioral proof,
	// since the real GetGameInstance()-based resolution path is untestable in
	// CreateNewMap() worlds (same documented limitation as issue #327's coverage).
	UMainMenuWidget* SecondWidget = CreateWidget<UMainMenuWidget>(World, UMainMenuWidget::StaticClass());
	if (TestNotNull(TEXT("SecondWidget should construct"), SecondWidget))
	{
		UGameInstance* GameInstanceOuter = NewObject<UGameInstance>();
		UCrowdMasteryTotalSubsystem* InjectedSubsystem = NewObject<UCrowdMasteryTotalSubsystem>(GameInstanceOuter);
		InjectedSubsystem->DepositRunMastery(7);
		TestEqual(TEXT("Sanity: injected subsystem should hold 7 before reset"), InjectedSubsystem->GetAccumulatedTotal(), 7);
		SecondWidget->CachedMasteryTotalSubsystem = InjectedSubsystem;

		// Display must reflect the reset immediately - the menu is already on screen
		// when CONFIRM fires, so no on-show refresh will run for it (PR #349 pass-2
		// escalation: the issue's 4th AC, buildable only once #328/PR #350 landed the
		// display). Seed the display with the pre-reset total first so the assertion
		// proves an actual refresh, not a never-populated default.
		SecondWidget->RefreshMasteryDisplayText();
		TestEqual(TEXT("Sanity: display should show the pre-reset total before CONFIRM"),
			SecondWidget->MasteryDisplayText->GetText().ToString(), FString(TEXT("CROWD MASTERY: 7")));

		SecondWidget->HandleMasteryResetClicked();
		SecondWidget->HandleMasteryResetConfirmClicked();
		TestEqual(TEXT("Confirming reset should zero the injected subsystem's total"),
			InjectedSubsystem->GetAccumulatedTotal(), 0);
		TestFalse(TEXT("bMasteryResetConfirmPending should be false after CONFIRM on SecondWidget"),
			SecondWidget->bMasteryResetConfirmPending);
		TestEqual(TEXT("Confirming reset should refresh the on-screen mastery display to 0 (issue #329 AC-4)"),
			SecondWidget->MasteryDisplayText->GetText().ToString(), FString(TEXT("CROWD MASTERY: 0")));
	}

	// (e2) CONFIRM performs a full respec (issue #380, docs/prd-mastery-skill-tree.md
	// REQ-5): real spent points + a real unlocked bubble are refunded/cleared, in the
	// pinned Refund-before-Reset order, and an already-open tree screen refreshes its
	// points display and the bubble's visual state immediately - no manual BACK/re-open.
	{
		using namespace KrowdKontrolMainMenuMasteryResetTest;

		UGameInstance* RespecGameInstanceOuter = NewObject<UGameInstance>();
		UCrowdMasteryTotalSubsystem* RespecSubsystem = NewObject<UCrowdMasteryTotalSubsystem>(RespecGameInstanceOuter);

		UDataTable* RespecTable = NewObject<UDataTable>();
		RespecTable->RowStruct = FMasteryTreeNode::StaticStruct();
		FMasteryTreeNode RespecRootRow;
		RespecRootRow.ParentNodeId = NAME_None;
		RespecRootRow.Phase = EMasteryTreePhase::Phase1;
		RespecRootRow.Bubbles = BuildFourBubbles(TEXT("Respec"));
		RespecTable->AddRow(FName(TEXT("Node_Respec")), RespecRootRow);
		RespecSubsystem->MasteryTreeTable = RespecTable;

		RespecSubsystem->DepositRunMastery(5);
		const FName RespecBubble0(TEXT("Respec_Bubble0"));
		TestTrue(TEXT("Sanity: spending on the fixture bubble should succeed"),
			RespecSubsystem->TrySpendOnBubble(RespecBubble0));
		TestEqual(TEXT("Sanity: SpentPoints should be 1 before respec"), RespecSubsystem->GetSpentPoints(), 1);
		TestEqual(TEXT("Sanity: UnlockedBubbles should hold 1 bubble before respec"), RespecSubsystem->GetUnlockedBubbles().Num(), 1);

		UMainMenuWidget* RespecWidget = CreateWidget<UMainMenuWidget>(World, UMainMenuWidget::StaticClass());
		if (TestNotNull(TEXT("RespecWidget should construct"), RespecWidget))
		{
			RespecWidget->CachedMasteryTotalSubsystem = RespecSubsystem;

			// Open the tree screen and inject the same subsystem instance into it too
			// (KrowdKontrolMainMenuMasteryScreenTest.cpp's pattern), then re-run
			// PopulateTreeContent() so real bubble widgets exist to assert on - the
			// screen's own construction-time build ran before the subsystem was
			// injected and found none, so it built nothing the first time.
			RespecWidget->HandleMasteryButtonClicked();
			if (TestNotNull(TEXT("RespecWidget->MasteryScreenWidgetInstance should be non-null"),
				ToRawPtr(RespecWidget->MasteryScreenWidgetInstance)))
			{
				UMasteryScreenWidget* RespecScreen = RespecWidget->MasteryScreenWidgetInstance;
				RespecScreen->CachedMasteryTotalSubsystem = RespecSubsystem;
				RespecScreen->PopulateTreeContent();
				RespecScreen->RefreshPointsDisplayText();

				TObjectPtr<UMasterySkillBubbleWidget>* FoundBubble = RespecScreen->BubbleWidgetsByBubbleId.Find(RespecBubble0);
				if (TestNotNull(TEXT("Sanity: fixture bubble widget should exist on the tree screen"), FoundBubble ? ToRawPtr(*FoundBubble) : nullptr))
				{
					TestEqual(TEXT("Sanity: fixture bubble should show Unlocked before respec"),
						static_cast<uint8>((*FoundBubble)->GetVisualState()), static_cast<uint8>(EMasterySkillBubbleVisualState::Unlocked));
				}
				TestEqual(TEXT("Sanity: tree screen should show 4 unspent points before respec"),
					RespecScreen->GetPointsDisplayText().ToString(), FString(TEXT("UNSPENT POINTS: 4")));

				RespecWidget->HandleMasteryResetClicked();
				RespecWidget->HandleMasteryResetConfirmClicked();

				TestEqual(TEXT("Full respec should zero SpentPoints"), RespecSubsystem->GetSpentPoints(), 0);
				TestEqual(TEXT("Full respec should clear UnlockedBubbles"), RespecSubsystem->GetUnlockedBubbles().Num(), 0);
				TestEqual(TEXT("Full respec should still zero the earned total (#329 semantics, unchanged)"),
					RespecSubsystem->GetAccumulatedTotal(), 0);

				const TArray<FString> ExpectedOrder = { TEXT("Refund"), TEXT("Reset") };
				if (TestEqual(TEXT("LastMasteryRespecCallOrder should have exactly 2 entries"),
					RespecWidget->LastMasteryRespecCallOrder.Num(), ExpectedOrder.Num()))
				{
					for (int32 Index = 0; Index < ExpectedOrder.Num(); ++Index)
					{
						TestEqual(*FString::Printf(TEXT("LastMasteryRespecCallOrder[%d] should pin Refund-before-Reset"), Index),
							RespecWidget->LastMasteryRespecCallOrder[Index], ExpectedOrder[Index]);
					}
				}

				TestEqual(TEXT("Tree screen points display should refresh to 0 immediately, no manual re-open"),
					RespecScreen->GetPointsDisplayText().ToString(), FString(TEXT("UNSPENT POINTS: 0")));

				FoundBubble = RespecScreen->BubbleWidgetsByBubbleId.Find(RespecBubble0);
				if (TestNotNull(TEXT("Fixture bubble widget should still exist on the tree screen after respec"), FoundBubble ? ToRawPtr(*FoundBubble) : nullptr))
				{
					// Respec_Bubble0's node has no ParentNodeId, so its prerequisite is
					// always met - after a full respec with 0 available points it reads
					// Unaffordable (re-locked in the "must re-earn/re-spend" sense), not
					// the Locked state (which only applies to bubbles behind an unmet
					// prerequisite).
					TestEqual(TEXT("Fixture bubble should show Unaffordable after respec (re-locked via 0 available points, screen refreshed)"),
						static_cast<uint8>((*FoundBubble)->GetVisualState()), static_cast<uint8>(EMasterySkillBubbleVisualState::Unaffordable));
				}
			}
		}
	}

	// (e3) CONFIRM with real spent points/unlocks but the tree screen never opened
	// (MasteryScreenWidgetInstance == nullptr) - the `if (MasteryScreenWidgetInstance)`
	// guard in HandleMasteryResetConfirmClicked() must not crash.
	{
		UGameInstance* NoScreenGameInstanceOuter = NewObject<UGameInstance>();
		UCrowdMasteryTotalSubsystem* NoScreenSubsystem = NewObject<UCrowdMasteryTotalSubsystem>(NoScreenGameInstanceOuter);
		NoScreenSubsystem->DepositRunMastery(3);

		UMainMenuWidget* NoScreenWidget = CreateWidget<UMainMenuWidget>(World, UMainMenuWidget::StaticClass());
		if (TestNotNull(TEXT("NoScreenWidget should construct"), NoScreenWidget))
		{
			NoScreenWidget->CachedMasteryTotalSubsystem = NoScreenSubsystem;
			TestNull(TEXT("Sanity: MasteryScreenWidgetInstance should be null - never opened"),
				ToRawPtr(NoScreenWidget->MasteryScreenWidgetInstance));

			NoScreenWidget->HandleMasteryResetClicked();
			NoScreenWidget->HandleMasteryResetConfirmClicked();
			TestTrue(TEXT("CONFIRM with no tree screen open should not crash"), true);
			TestEqual(TEXT("CONFIRM should still zero the total with no tree screen open"),
				NoScreenSubsystem->GetAccumulatedTotal(), 0);
		}
	}

	// (f) RefreshMasteryResetVisibility()'s null-button guard on an unbuilt widget
	// (bare NewObject(), neither NativeOnInitialized() nor Initialize() called) - mirrors
	// KrowdKontrolMainMenuWidgetTest.cpp block (f)'s identical "unbuilt widget degrades
	// safely" convention for SetMasteryDisplayContent().
	UMainMenuWidget* UnbuiltWidget = NewObject<UMainMenuWidget>();
	if (TestNotNull(TEXT("UnbuiltWidget should construct"), UnbuiltWidget))
	{
		UnbuiltWidget->RefreshMasteryResetVisibility();
		TestTrue(TEXT("RefreshMasteryResetVisibility on an unbuilt widget should not crash"), true);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
