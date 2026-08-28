// Confirms issue #325: UMainMenuWidget builds a data-driven level-select list from
// ULevelSequenceSubsystem::GetShippedLevelMapNames() - one UMainMenuLevelButtonWidget
// per LevelSequenceTable row, each correctly targeting its own map, and that clicking
// one routes through OnLevelSelected(FName) to UMainMenuWidget::HandleLevelSelected()
// with the right target, without ever needing the real UGameplayStatics::OpenLevel()
// call to succeed (this suite's CreateNewMap() Worlds are Editor Worlds, not game
// worlds, so HandleLevelSelected()'s IsGameWorld() guard correctly skips real travel -
// see LastSelectedLevelMapName's observability-seam role, mirroring
// ULevelSequenceSubsystem::LastAdvanceAttemptedMapName's identical rationale).
//
// Also covers the real current production default: an unset LevelSequenceTable (no
// content DataTable asset exists yet) must degrade to zero buttons, no crash.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "MainMenuWidget.h"
#include "MainMenuLevelButtonWidget.h"
#include "LevelSequenceSubsystem.h"
#include "LevelSequenceData.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolMainMenuLevelSelectTest,
	"KrowdKontrol.Unit.MainMenuLevelSelect",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace KrowdKontrolMainMenuLevelSelectTest
{
	// Multi-row version of KrowdKontrolLevelSequenceSubsystemTest.cpp's
	// BuildSequenceTable() helper - three rows chained in authoring order, matching
	// the shape a real LevelSequenceTable content asset would have.
	UDataTable* BuildThreeLevelSequenceTable()
	{
		UDataTable* Table = NewObject<UDataTable>();
		Table->RowStruct = FLevelSequenceRow::StaticStruct();

		FLevelSequenceRow Row1;
		Row1.NextLevelMapName = FName(TEXT("L_Level02"));
		Table->AddRow(FName(TEXT("L_Level01")), Row1);

		FLevelSequenceRow Row2;
		Row2.NextLevelMapName = FName(TEXT("L_Level03"));
		Table->AddRow(FName(TEXT("L_Level02")), Row2);

		FLevelSequenceRow Row3;
		Row3.NextLevelMapName = NAME_None;
		Table->AddRow(FName(TEXT("L_Level03")), Row3);

		return Table;
	}
}

bool FKrowdKontrolMainMenuLevelSelectTest::RunTest(const FString& Parameters)
{
	using namespace KrowdKontrolMainMenuLevelSelectTest;

	// (a) Populated table: 3 rows -> 3 buttons, correct per-button map targeting,
	// click-through to the shared handler.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}

		ULevelSequenceSubsystem* SequenceSubsystem = World->GetSubsystem<ULevelSequenceSubsystem>();
		if (!TestNotNull(TEXT("UWorld should auto-instantiate ULevelSequenceSubsystem"), SequenceSubsystem))
		{
			return false;
		}

		// Must be set BEFORE CreateWidget<UMainMenuWidget>() - PopulateLevelSelectButtons()
		// reads the subsystem synchronously during NativeOnInitialized(), which fires
		// from inside CreateWidget() itself.
		SequenceSubsystem->LevelSequenceTable = BuildThreeLevelSequenceTable();

		UMainMenuWidget* Widget = CreateWidget<UMainMenuWidget>(World, UMainMenuWidget::StaticClass());
		if (!TestNotNull(TEXT("UMainMenuWidget should construct"), Widget))
		{
			return false;
		}

		if (TestEqual(TEXT("LevelSelectButtons should have one entry per LevelSequenceTable row"),
			Widget->LevelSelectButtons.Num(), 3))
		{
			const TArray<FName> ExpectedOrder = {
				FName(TEXT("L_Level01")),
				FName(TEXT("L_Level02")),
				FName(TEXT("L_Level03")) };

			for (int32 Index = 0; Index < ExpectedOrder.Num(); ++Index)
			{
				UMainMenuLevelButtonWidget* ButtonWidget = Widget->LevelSelectButtons[Index];
				if (TestNotNull(FString::Printf(TEXT("LevelSelectButtons[%d] should be non-null"), Index), ButtonWidget))
				{
					TestEqual(FString::Printf(TEXT("LevelSelectButtons[%d] should target the expected map"), Index),
						ButtonWidget->GetLevelMapName(), ExpectedOrder[Index]);
					TestTrue(FString::Printf(TEXT("LevelSelectButtons[%d]::OnLevelSelected should be bound"), Index),
						ButtonWidget->OnLevelSelected.IsBound());
					TestTrue(FString::Printf(TEXT("LevelSelectButtons[%d]::LevelButton::OnClicked should be bound"), Index),
						ButtonWidget->LevelButton->OnClicked.IsBound());
					TestEqual(FString::Printf(TEXT("LevelSelectButtons[%d] label should show the target map's name"), Index),
						ButtonWidget->LevelButtonLabel->GetText().ToString(), ExpectedOrder[Index].ToString());
				}
			}

			// Pick the middle button and drive its private click handler directly, proving
			// the click -> broadcast -> shared-handler -> correct-target chain end to end
			// without needing the real OpenLevel() call to succeed.
			UMainMenuLevelButtonWidget* MiddleButton = Widget->LevelSelectButtons[1];
			MiddleButton->HandleClicked();
			TestEqual(TEXT("HandleLevelSelected should record the middle button's target map"),
				Widget->LastSelectedLevelMapName, FName(TEXT("L_Level02")));

			// HandleLevelSelected(NAME_None) is the documented content-authoring-mistake
			// guard (an empty/None DataTable row name) - must ignore the call and leave the
			// previously-recorded target map untouched, not overwrite it with NAME_None.
			Widget->HandleLevelSelected(NAME_None);
			TestEqual(TEXT("HandleLevelSelected(NAME_None) should not change LastSelectedLevelMapName"),
				Widget->LastSelectedLevelMapName, FName(TEXT("L_Level02")));

			// EnsureWidgetTreeBuilt()'s "if (!TitleText)" build-once guard must also cover
			// PopulateLevelSelectButtons() - a second Initialize() must not duplicate
			// buttons or double-bind OnLevelSelected, mirroring KrowdKontrolMainMenuWidgetTest.cpp
			// block (e)'s identical TitleText-identity guard check, extended here to the
			// level-select list.
			Widget->Initialize();
			TestEqual(TEXT("LevelSelectButtons count should be unchanged after a second Initialize()"),
				Widget->LevelSelectButtons.Num(), 3);
		}
	}

	// (b) Unset table (today's real production default): zero buttons, no crash.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}

		ULevelSequenceSubsystem* SequenceSubsystem = World->GetSubsystem<ULevelSequenceSubsystem>();
		if (!TestNotNull(TEXT("UWorld should auto-instantiate ULevelSequenceSubsystem"), SequenceSubsystem))
		{
			return false;
		}
		// SequenceSubsystem->LevelSequenceTable intentionally left at its default (nullptr).

		UMainMenuWidget* Widget = CreateWidget<UMainMenuWidget>(World, UMainMenuWidget::StaticClass());
		if (TestNotNull(TEXT("UMainMenuWidget should construct with an unset LevelSequenceTable"), Widget))
		{
			TestEqual(TEXT("LevelSelectButtons should be empty with an unset LevelSequenceTable"),
				Widget->LevelSelectButtons.Num(), 0);
		}
	}

	// (c) No World at all (bare NewObject(), mirrors KrowdKontrolMainMenuWidgetTest.cpp
	// block (g)'s identical no-World construction) - PopulateLevelSelectButtons()'s
	// !SequenceSubsystem branch must degrade to zero buttons, no crash, exactly one
	// logged warning.
	{
		AddExpectedError(TEXT("no ULevelSequenceSubsystem available"), EAutomationExpectedErrorFlags::Contains, 1);
		UMainMenuWidget* NoWorldWidget = NewObject<UMainMenuWidget>();
		if (TestNotNull(TEXT("NoWorldWidget should construct"), NoWorldWidget))
		{
			NoWorldWidget->NativeOnInitialized();
			TestEqual(TEXT("LevelSelectButtons should be empty with no World/subsystem available"),
				NoWorldWidget->LevelSelectButtons.Num(), 0);
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
