// Confirms issue #246: ULevelBriefingSubsystem is the OnLevelBegin consumer that
// shows a data-table-driven pre-level briefing card. Covers (a) table lookup by
// bare map name and PIE-prefix stripping; (b) controller forwarding when the
// widget already exists; (c) the pending-buffer flush when OnLevelBegin fires
// before AKrowdKontrolPlayerController::BeginPlay() has created its HUD widgets
// (same race issue #235 fixed for OnScreenPromptWidgetInstance); (d) dismiss on
// player input; (e) dismiss on the 8s auto-timeout; (f) the real
// ULevelLifecycleSubsystem::OnLevelBegin broadcast wiring, including the safe
// missing-row no-op for a non-L_LevelNN map name; (g) the missing-table no-op.
//
// Each case uses its own FAutomationEditorCommonUtils::CreateNewMap() World, per
// this module's established per-scenario isolation convention (see
// KrowdKontrolAbilityUnlockLevelSubsystemTest.cpp).
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "LevelBriefingSubsystem.h"
#include "LevelBriefingData.h"
#include "LevelLifecycleSubsystem.h"
#include "KrowdKontrolPlayerController.h"
#include "BriefingCardWidget.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolLevelBriefingSubsystemTest,
	"KrowdKontrol.Unit.LevelBriefingSubsystem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace KrowdKontrolLevelBriefingSubsystemTest
{
	// Builds an in-code table with a single "L_Level01" row - the established
	// code-only pattern for testing DataTable-driven logic without a real asset
	// (see the investigation's "NOT Building" section).
	UDataTable* BuildBriefingTable()
	{
		UDataTable* Table = NewObject<UDataTable>();
		Table->RowStruct = FLevelBriefingRow::StaticStruct();

		FLevelBriefingRow Row;
		Row.LevelDisplayName = FText::FromString(TEXT("LEVEL 1"));
		Row.ObjectiveLines.Add(FText::FromString(TEXT("PACIFY ALL 8 ROBOTS")));
		Row.ObjectiveLines.Add(FText::FromString(TEXT("STUN THEM, HERD THEM TO THEIR PENS")));
		Row.NewAbilityUnlockLine = FText::FromString(TEXT("NEW: SLEEP - PRESS 2 - STRONG VS SNIPERS"));
		Table->AddRow(FName(TEXT("L_Level01")), Row);
		return Table;
	}

	// Spawns a possessed, world-registered, local-player-backed controller - mirrors
	// KrowdKontrolAbilityUnlockPromptComponentTest.cpp's SpawnControllerWithPromptWidget()
	// verbatim. The local-player setup (Player/SetAsLocalPlayerController()) is what
	// CreateWidget<T>(Controller, Class) requires inside CreateHUDWidgets() -
	// without it, CreateWidget() logs "Only Local Player Controllers can be assigned
	// to widgets" and returns null, which this test needs to be non-null via
	// DispatchBeginPlay() below.
	AKrowdKontrolPlayerController* SpawnPossessedController(UWorld* World, APawn* Pawn)
	{
		AKrowdKontrolPlayerController* Controller = World->SpawnActor<AKrowdKontrolPlayerController>();
		if (!Controller)
		{
			return nullptr;
		}
		Controller->Player = NewObject<ULocalPlayer>(GEngine);
		Controller->SetAsLocalPlayerController();
		Controller->Possess(Pawn);
		// CreateNewMap() worlds skip PostInitializeComponents, so
		// World->GetFirstPlayerController() reads empty without this explicit
		// registration step.
		World->AddController(Controller);
		return Controller;
	}
}

bool FKrowdKontrolLevelBriefingSubsystemTest::RunTest(const FString& Parameters)
{
	using namespace KrowdKontrolLevelBriefingSubsystemTest;

	// (a) Table lookup by bare map name, plus PIE-prefix stripping - a PIE-mangled
	// map name must still resolve to the same row as its bare form.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}

		ULevelBriefingSubsystem* Subsystem = World->GetSubsystem<ULevelBriefingSubsystem>();
		if (!TestNotNull(TEXT("UWorld should auto-instantiate ULevelBriefingSubsystem"), Subsystem))
		{
			return false;
		}
		Subsystem->LevelBriefingTable = BuildBriefingTable();

		APawn* Pawn = World->SpawnActor<APawn>();
		AKrowdKontrolPlayerController* Controller = SpawnPossessedController(World, Pawn);
		if (!TestNotNull(TEXT("Controller should spawn"), Controller))
		{
			return false;
		}
		Controller->DispatchBeginPlay();

		Subsystem->HandleLevelBegin(FName(TEXT("UEDPIE_0_L_Level01")));

		if (!TestNotNull(TEXT("BriefingCardWidgetInstance should exist"), ToRawPtr(Controller->BriefingCardWidgetInstance)))
		{
			return false;
		}
		TestTrue(TEXT("A PIE-mangled map name should still resolve to the L_Level01 row"),
			Controller->BriefingCardWidgetInstance->IsBriefingVisible());
		TestEqual(TEXT("Level name should match the resolved row"),
			Controller->BriefingCardWidgetInstance->GetLevelNameDisplayText().ToString(), TEXT("LEVEL 1"));
	}

	// (b) Controller forwarding when the widget already exists - the common case.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}

		ULevelBriefingSubsystem* Subsystem = World->GetSubsystem<ULevelBriefingSubsystem>();
		if (!TestNotNull(TEXT("ULevelBriefingSubsystem should exist"), Subsystem))
		{
			return false;
		}
		Subsystem->LevelBriefingTable = BuildBriefingTable();

		APawn* Pawn = World->SpawnActor<APawn>();
		AKrowdKontrolPlayerController* Controller = SpawnPossessedController(World, Pawn);
		if (!TestNotNull(TEXT("Controller should spawn"), Controller))
		{
			return false;
		}
		Controller->DispatchBeginPlay();

		Subsystem->HandleLevelBegin(FName(TEXT("L_Level01")));

		if (!TestNotNull(TEXT("BriefingCardWidgetInstance should exist"), ToRawPtr(Controller->BriefingCardWidgetInstance)))
		{
			return false;
		}
		TestTrue(TEXT("Briefing should be visible after HandleLevelBegin"),
			Controller->BriefingCardWidgetInstance->IsBriefingVisible());
		TestEqual(TEXT("Level name should be populated"),
			Controller->BriefingCardWidgetInstance->GetLevelNameDisplayText().ToString(), TEXT("LEVEL 1"));
		TestEqual(TEXT("Objective text should join both objective lines"),
			Controller->BriefingCardWidgetInstance->GetObjectiveDisplayText().ToString(),
			TEXT("PACIFY ALL 8 ROBOTS\nSTUN THEM, HERD THEM TO THEIR PENS"));
		TestEqual(TEXT("New-ability line should be populated"),
			Controller->BriefingCardWidgetInstance->GetNewAbilityDisplayText().ToString(),
			TEXT("NEW: SLEEP - PRESS 2 - STRONG VS SNIPERS"));

		// (d) Dismiss on player input.
		Controller->BriefingCardWidgetInstance->DismissBriefing();
		TestFalse(TEXT("Briefing should no longer be visible after DismissBriefing()"),
			Controller->BriefingCardWidgetInstance->IsBriefingVisible());

		// (e) Dismiss on the 8s auto-timeout.
		Subsystem->HandleLevelBegin(FName(TEXT("L_Level01")));
		TestTrue(TEXT("Briefing should be visible again after a fresh HandleLevelBegin"),
			Controller->BriefingCardWidgetInstance->IsBriefingVisible());
		Controller->BriefingCardWidgetInstance->AdvanceDismissTimer(UBriefingCardWidget::BriefingAutoDismissSeconds);
		TestFalse(TEXT("Briefing should auto-dismiss once the full 8s has elapsed"),
			Controller->BriefingCardWidgetInstance->IsBriefingVisible());
	}

	// (c) Pending-buffer flush: OnLevelBegin (via a direct HandleLevelBegin call)
	// fires before AKrowdKontrolPlayerController::BeginPlay() has created its HUD
	// widgets - the briefing must not be dropped, it must show once
	// CreateHUDWidgets() runs.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}

		ULevelBriefingSubsystem* Subsystem = World->GetSubsystem<ULevelBriefingSubsystem>();
		if (!TestNotNull(TEXT("ULevelBriefingSubsystem should exist"), Subsystem))
		{
			return false;
		}
		Subsystem->LevelBriefingTable = BuildBriefingTable();

		APawn* Pawn = World->SpawnActor<APawn>();
		AKrowdKontrolPlayerController* Controller = SpawnPossessedController(World, Pawn);
		if (!TestNotNull(TEXT("Controller should spawn"), Controller))
		{
			return false;
		}
		// Deliberately NOT calling DispatchBeginPlay() yet - BriefingCardWidgetInstance
		// doesn't exist when HandleLevelBegin fires below.

		Subsystem->HandleLevelBegin(FName(TEXT("L_Level01")));
		TestNull(TEXT("BriefingCardWidgetInstance should not exist yet"), ToRawPtr(Controller->BriefingCardWidgetInstance));

		Controller->DispatchBeginPlay();
		if (!TestNotNull(TEXT("BriefingCardWidgetInstance should exist after DispatchBeginPlay()"), ToRawPtr(Controller->BriefingCardWidgetInstance)))
		{
			return false;
		}
		TestTrue(TEXT("The buffered briefing should be flushed and shown"),
			Controller->BriefingCardWidgetInstance->IsBriefingVisible());
		TestEqual(TEXT("The flushed briefing's level name should match the buffered row"),
			Controller->BriefingCardWidgetInstance->GetLevelNameDisplayText().ToString(), TEXT("LEVEL 1"));
	}

	// (f) Real broadcast wiring: ULevelLifecycleSubsystem::OnLevelBegin's actual
	// broadcast (not a direct HandleLevelBegin call) reaches ULevelBriefingSubsystem
	// via the Initialize()-time subscription. CreateNewMap()'s synthetic map name
	// won't match "L_Level01", so this also exercises the missing-row safe no-op.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}

		ULevelBriefingSubsystem* Subsystem = World->GetSubsystem<ULevelBriefingSubsystem>();
		ULevelLifecycleSubsystem* LifecycleSubsystem = World->GetSubsystem<ULevelLifecycleSubsystem>();
		if (!TestNotNull(TEXT("ULevelBriefingSubsystem should exist"), Subsystem) ||
			!TestNotNull(TEXT("ULevelLifecycleSubsystem should exist"), LifecycleSubsystem))
		{
			return false;
		}
		Subsystem->LevelBriefingTable = BuildBriefingTable();

		APawn* Pawn = World->SpawnActor<APawn>();
		AKrowdKontrolPlayerController* Controller = SpawnPossessedController(World, Pawn);
		if (!TestNotNull(TEXT("Controller should spawn"), Controller))
		{
			return false;
		}
		Controller->DispatchBeginPlay();

		AddExpectedError(TEXT("no LevelBriefingTable row found"), EAutomationExpectedErrorFlags::Contains, 1, false);

		LifecycleSubsystem->OnWorldBeginPlay(*World);

		TestFalse(TEXT("No briefing should show for a map name with no matching row (safe no-op, not a crash)"),
			Controller->BriefingCardWidgetInstance && Controller->BriefingCardWidgetInstance->IsBriefingVisible());
	}

	// (g) Missing-table no-op: HandleLevelBegin must not crash, and the
	// missing-table warning must fire exactly once even across repeated calls.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}

		ULevelBriefingSubsystem* Subsystem = World->GetSubsystem<ULevelBriefingSubsystem>();
		if (!TestNotNull(TEXT("ULevelBriefingSubsystem should exist"), Subsystem))
		{
			return false;
		}
		Subsystem->LevelBriefingTable = nullptr;

		AddExpectedError(TEXT("LevelBriefingTable is unset"), EAutomationExpectedErrorFlags::Contains, 1, false);

		Subsystem->HandleLevelBegin(FName(TEXT("L_Level01")));
		Subsystem->HandleLevelBegin(FName(TEXT("L_Level02")));
		// The AddExpectedError count of 1 above asserts the second call did not log again.
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
