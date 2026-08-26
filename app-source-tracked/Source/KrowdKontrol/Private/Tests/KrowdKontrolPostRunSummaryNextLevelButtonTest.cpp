// Confirms issue #321: the post-run summary screen's NEXT LEVEL button resolves its
// label/behavior from ULevelSequenceSubsystem's data (ComputeNextLevelMapName()), not
// a hardcoded conditional, and that clicking it drives the correct caller-triggered
// path for each case:
//
// (a) Non-final level: ResolvedNextLevelMapName resolves to the configured next map,
// the button label reads "NEXT LEVEL", and HandleNextLevelClicked() does not crash
// (the real UGameplayStatics::OpenLevel() call inside AdvanceToNextLevel() is
// unreachable here since CreateNewMap() test Worlds are never game worlds - same
// documented limitation as every other reload test in this module, e.g.
// KrowdKontrolLevelSequenceSubsystemTest.cpp).
//
// (b) Final shipped level (NextLevelMapName == NAME_None): the button relabels to
// "FINISH RUN (More Levels Coming)" and HandleNextLevelClicked() reruns the current
// level via the shared AKrowdKontrolPlayerController::RequestLevelRestart() reload
// path (issue #223) instead of a parallel implementation - proven by asserting
// WasRestartRequested() flips true, the same real accessor
// KrowdKontrolLevelRestartTest.cpp/KrowdKontrolBossCheckpointRestartTest.cpp assert.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "PostRunSummaryWidget.h"
#include "KrowdKontrolPlayerController.h"
#include "LevelSequenceSubsystem.h"
#include "LevelSequenceData.h"
#include "LevelLifecycleSubsystem.h"
#include "EnemyBaseTestActor.h"
#include "AbilitySlot.h"
#include "Engine/DataTable.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolPostRunSummaryNextLevelButtonTest,
	"KrowdKontrol.Unit.PostRunSummaryNextLevelButton",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace KrowdKontrolPostRunSummaryNextLevelButtonTest
{
	// Same in-code synthetic-table pattern as
	// KrowdKontrolLevelSequenceSubsystemTest.cpp's BuildSequenceTable() - duplicated
	// locally per this test family's existing per-file-helper convention.
	UDataTable* BuildSequenceTable(FName CurrentMapName, FName NextLevelMapName)
	{
		UDataTable* Table = NewObject<UDataTable>();
		Table->RowStruct = FLevelSequenceRow::StaticStruct();

		FLevelSequenceRow Row;
		Row.NextLevelMapName = NextLevelMapName;
		Table->AddRow(CurrentMapName, Row);
		return Table;
	}
}

bool FKrowdKontrolPostRunSummaryNextLevelButtonTest::RunTest(const FString& Parameters)
{
	using namespace KrowdKontrolPostRunSummaryNextLevelButtonTest;
	const FVector ZeroDistanceLocation(0.0f, 0.0f, 0.0f);

	// (a) Non-final level: clicking resolves and (in a real game world) would advance
	// via AdvanceToNextLevel().
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}

		ULevelSequenceSubsystem* SequenceSubsystem = World->GetSubsystem<ULevelSequenceSubsystem>();
		ULevelLifecycleSubsystem* LifecycleSubsystem = World->GetSubsystem<ULevelLifecycleSubsystem>();
		if (!TestNotNull(TEXT("UWorld should auto-instantiate ULevelSequenceSubsystem"), SequenceSubsystem) ||
			!TestNotNull(TEXT("UWorld should auto-instantiate ULevelLifecycleSubsystem"), LifecycleSubsystem))
		{
			return false;
		}

		const FName CurrentMapName = FName(*World->GetMapName());
		SequenceSubsystem->LevelSequenceTable = BuildSequenceTable(CurrentMapName, FName(TEXT("L_Level02")));

		UPostRunSummaryWidget* Widget = CreateWidget<UPostRunSummaryWidget>(World, UPostRunSummaryWidget::StaticClass());
		if (!TestNotNull(TEXT("UPostRunSummaryWidget should construct"), Widget))
		{
			return false;
		}

		LifecycleSubsystem->OnWorldBeginPlay(*World);

		AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), Enemy))
		{
			return false;
		}
		Enemy->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
		Enemy->TickCheckDetection(ZeroDistanceLocation); // Alert -> Attack
		Enemy->ReceiveControl(EAbilitySlot::Stun);        // -> Controlled
		Enemy->TransitionToBanked();                      // -> Banked

		LifecycleSubsystem->RefreshLevelClearState();

		TestEqual(TEXT("ResolvedNextLevelMapName should match the configured next level"),
			Widget->ResolvedNextLevelMapName, FName(TEXT("L_Level02")));
		TestEqual(TEXT("Non-final level should show the NEXT LEVEL label"),
			Widget->GetNextLevelButtonDisplayText().ToString(), FString(TEXT("NEXT LEVEL")));

		// The test World is never a game world, so AdvanceToNextLevel()'s IsGameWorld()
		// guard makes the real OpenLevel() unreachable here - only confirming this
		// doesn't crash, same documented limitation as every other reload test.
		Widget->HandleNextLevelClicked();
	}

	// (b) Final shipped level: the button relabels and reruns via the shared restart path.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		World->InitializeActorsForPlay(FURL());

		ULevelSequenceSubsystem* SequenceSubsystem = World->GetSubsystem<ULevelSequenceSubsystem>();
		ULevelLifecycleSubsystem* LifecycleSubsystem = World->GetSubsystem<ULevelLifecycleSubsystem>();
		if (!TestNotNull(TEXT("UWorld should auto-instantiate ULevelSequenceSubsystem"), SequenceSubsystem) ||
			!TestNotNull(TEXT("UWorld should auto-instantiate ULevelLifecycleSubsystem"), LifecycleSubsystem))
		{
			return false;
		}

		const FName CurrentMapName = FName(*World->GetMapName());
		SequenceSubsystem->LevelSequenceTable = BuildSequenceTable(CurrentMapName, NAME_None);

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

		LifecycleSubsystem->OnWorldBeginPlay(*World);

		AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), Enemy))
		{
			return false;
		}
		Enemy->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
		Enemy->TickCheckDetection(ZeroDistanceLocation); // Alert -> Attack
		Enemy->ReceiveControl(EAbilitySlot::Stun);        // -> Controlled
		Enemy->TransitionToBanked();                      // -> Banked

		LifecycleSubsystem->RefreshLevelClearState();

		TestEqual(TEXT("ResolvedNextLevelMapName should be NAME_None for the sequence's final level"),
			Controller->PostRunSummaryWidgetInstance->ResolvedNextLevelMapName, FName(NAME_None));
		TestEqual(TEXT("Final level should show the FINISH RUN placeholder label"),
			Controller->PostRunSummaryWidgetInstance->GetNextLevelButtonDisplayText().ToString(),
			FString(TEXT("FINISH RUN (More Levels Coming)")));

		TestFalse(TEXT("bRestartRequested should be false before the button is clicked"),
			Controller->WasRestartRequested());
		Controller->PostRunSummaryWidgetInstance->HandleNextLevelClicked();
		TestTrue(TEXT("Clicking the button on the final level should invoke the shared RequestLevelRestart() path"),
			Controller->WasRestartRequested());
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
