// Confirms issue #216: ULevelSequenceSubsystem is the OnLevelClear consumer that
// resolves the run's position in a configured level sequence. Covers (a) a non-final
// level's clear resolving the next map from LevelSequenceTable without touching
// ULevelLifecycleSubsystem::FinalMapName; (b) a level explicitly marked as the
// sequence's end (NextLevelMapName == NAME_None) setting FinalMapName so the
// existing OnRunComplete path fires, per KrowdKontrolLevelLifecycleSubsystemTest.cpp
// case (f)'s same mechanism; (c) a map with no LevelSequenceTable row at all being
// a safe, warn-once no-op that leaves FinalMapName untouched (distinct from case
// (b) - an unconfigured map must never be mistaken for the sequence's final level);
// (d) an entirely unset LevelSequenceTable - the system's actual current production
// default until the real DataTable asset is authored - taking the same warn-once
// no-op path via a distinct guard clause from case (c)'s "table set, no matching row".
//
// Issue #321 split this subsystem's responsibilities: HandleLevelClear() (exercised
// below, via the real OnLevelClear broadcast) now only resolves/bookkeeps - it no
// longer calls UGameplayStatics::OpenLevel() itself. The actual map travel moved to
// a new public AdvanceToNextLevel(), now only ever invoked by the post-run summary
// screen's NEXT LEVEL button (KrowdKontrolPostRunSummaryNextLevelButtonTest.cpp),
// not automatically on clear. Case (a) below asserts LastAdvanceAttemptedMapName
// stays NAME_None after HandleLevelClear() runs, proving that split holds - a
// regression that reintroduced auto-advance would fail it, since that seam is only
// ever set from inside AdvanceToNextLevel() itself.
//
// Each case uses its own FAutomationEditorCommonUtils::CreateNewMap() World, per
// this module's established per-scenario isolation convention (see
// KrowdKontrolLevelBriefingSubsystemTest.cpp). ULevelSequenceSubsystem is exercised
// via its real OnLevelClear subscription (auto-wired at World creation through its
// own Initialize()) in every case below, not a direct HandleLevelClear() call - this
// module's subsystems auto-instantiate and self-subscribe as soon as
// World->GetSubsystem<T>() (or any GetSubsystem<T>() call) first runs.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "LevelSequenceSubsystem.h"
#include "LevelSequenceData.h"
#include "LevelLifecycleSubsystem.h"
#include "LevelLifecycleTestListener.h"
#include "EnemyBaseTestActor.h"
#include "AbilitySlot.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolLevelSequenceSubsystemTest,
	"KrowdKontrol.Unit.LevelSequenceSubsystem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace KrowdKontrolLevelSequenceSubsystemTest
{
	// Builds an in-code table with a single row keyed to CurrentMapName - the
	// established code-only pattern for testing DataTable-driven logic without a
	// real asset (see KrowdKontrolLevelBriefingSubsystemTest.cpp's
	// BuildBriefingTable()).
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

bool FKrowdKontrolLevelSequenceSubsystemTest::RunTest(const FString& Parameters)
{
	using namespace KrowdKontrolLevelSequenceSubsystemTest;
	const FVector ZeroDistanceLocation(0.0f, 0.0f, 0.0f);

	// (a) Non-final clear: the current map's row points at a real next map name -
	// ComputeNextLevelMapName() resolves it, and FinalMapName is left untouched
	// (this clear must not be mistaken for the sequence's end).
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}

		// A CreateNewMap() World is an Editor-type World, never a game world - this is
		// the precondition HandleLevelClear()'s IsGameWorld() guard relies on to skip
		// the real OpenLevel() call here. Asserted up front so this test documents *why*
		// no assertion on the actual reload follows (mirrors KrowdKontrolLevelRestartTest.cpp).
		TestFalse(TEXT("CreateNewMap() World should not be a game world"), World->IsGameWorld());

		ULevelSequenceSubsystem* SequenceSubsystem = World->GetSubsystem<ULevelSequenceSubsystem>();
		ULevelLifecycleSubsystem* LifecycleSubsystem = World->GetSubsystem<ULevelLifecycleSubsystem>();
		if (!TestNotNull(TEXT("UWorld should auto-instantiate ULevelSequenceSubsystem"), SequenceSubsystem) ||
			!TestNotNull(TEXT("UWorld should auto-instantiate ULevelLifecycleSubsystem"), LifecycleSubsystem))
		{
			return false;
		}

		const FName CurrentMapName = FName(*World->GetMapName());
		SequenceSubsystem->LevelSequenceTable = BuildSequenceTable(CurrentMapName, FName(TEXT("L_Level02")));

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

		TestEqual(TEXT("ComputeNextLevelMapName should resolve the configured next level"),
			SequenceSubsystem->ComputeNextLevelMapName(), FName(TEXT("L_Level02")));
		TestEqual(TEXT("FinalMapName must stay untouched for a non-final clear"),
			LifecycleSubsystem->FinalMapName, FName(NAME_None));

		// Issue #321's critical fix: HandleLevelClear() (just exercised via the real
		// OnLevelClear broadcast above) must never itself call AdvanceToNextLevel() -
		// only the post-run summary screen's NEXT LEVEL button does that now. Asserting
		// LastAdvanceAttemptedMapName stayed NAME_None here means a future regression
		// that reintroduces auto-advance from HandleLevelClear() (via AdvanceToNextLevel(),
		// the only path that sets this seam) would fail this test, closing the gap this
		// fix would otherwise have zero automated regression protection for.
		TestEqual(TEXT("HandleLevelClear must not itself trigger AdvanceToNextLevel() - only the NEXT LEVEL button click handler does"),
			SequenceSubsystem->LastAdvanceAttemptedMapName, FName(NAME_None));
	}

	// (b) Final clear: the current map's row explicitly ends the sequence
	// (NextLevelMapName == NAME_None) - HandleLevelClear() must set FinalMapName so
	// ULevelLifecycleSubsystem's own existing OnRunComplete path fires, without this
	// class broadcasting OnRunComplete itself.
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
		SequenceSubsystem->LevelSequenceTable = BuildSequenceTable(CurrentMapName, NAME_None);

		ULevelLifecycleTestListener* Listener = NewObject<ULevelLifecycleTestListener>();
		LifecycleSubsystem->OnLevelClear.AddDynamic(Listener, &ULevelLifecycleTestListener::HandleLevelClear);
		LifecycleSubsystem->OnRunComplete.AddDynamic(Listener, &ULevelLifecycleTestListener::HandleRunComplete);

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

		TestEqual(TEXT("ComputeNextLevelMapName should return NAME_None for the sequence's final level"),
			SequenceSubsystem->ComputeNextLevelMapName(), FName(NAME_None));
		TestEqual(TEXT("OnLevelClear should still fire for the final level"),
			Listener->LevelClearCallCount, 1);
		TestEqual(TEXT("OnRunComplete should fire because HandleLevelClear set FinalMapName to this world's map"),
			Listener->RunCompleteCallCount, 1);

		// The whole final-level mechanism depends on HandleLevelClear() setting
		// FinalMapName *before* RefreshLevelClearState()'s own FinalMapName check,
		// inside the same OnLevelClear.Broadcast() call - assert the actual call
		// order, not just that both counts landed on 1, so a future refactor that
		// moves HandleLevelClear() off this synchronous broadcast fails this test
		// (mirrors KrowdKontrolLevelLifecycleSubsystemTest.cpp case (f)).
		const TArray<FString> ExpectedOrder = { TEXT("LevelClear"), TEXT("RunComplete") };
		TestEqual(TEXT("OnRunComplete must fire immediately after OnLevelClear, not before or interleaved"),
			Listener->CallOrder, ExpectedOrder);
	}

	// (c) Unconfigured map: no LevelSequenceTable row exists for the current map at
	// all (distinct from case (b) - a map simply outside the sequence must not be
	// mistaken for its final level). Safe, warn-once no-op; FinalMapName stays
	// untouched and OnRunComplete does not fire.
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

		// Row keyed to a map name that will never match this test World's own
		// synthetic CreateNewMap() name.
		SequenceSubsystem->LevelSequenceTable = BuildSequenceTable(FName(TEXT("L_Level01")), FName(TEXT("L_Level02")));

		ULevelLifecycleTestListener* Listener = NewObject<ULevelLifecycleTestListener>();
		LifecycleSubsystem->OnRunComplete.AddDynamic(Listener, &ULevelLifecycleTestListener::HandleRunComplete);

		AddExpectedError(TEXT("no LevelSequenceTable row"), EAutomationExpectedErrorFlags::Contains, 1, false);

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

		TestEqual(TEXT("ComputeNextLevelMapName should return NAME_None with no matching row"),
			SequenceSubsystem->ComputeNextLevelMapName(), FName(NAME_None));
		TestEqual(TEXT("FinalMapName must stay untouched for an unconfigured map"),
			LifecycleSubsystem->FinalMapName, FName(NAME_None));
		TestEqual(TEXT("OnRunComplete must not fire for an unconfigured map"),
			Listener->RunCompleteCallCount, 0);

		// A second RefreshLevelClearState() call must not re-warn - OnLevelClear only
		// fires once per world anyway (bHasFiredLevelClear), but this locks in that
		// guarantee from this test's own perspective too. The AddExpectedError count
		// of 1 above asserts no second warning was logged.
		LifecycleSubsystem->RefreshLevelClearState();
	}

	// (d) Unset LevelSequenceTable: the real current production default, since the
	// LevelSequenceTable DataTable asset doesn't exist yet - every real level clear
	// in the live game currently takes this path. FindCurrentMapRow() must take the
	// null-table guard (distinct code path from case (c)'s "table set, no matching
	// row"), producing the same safe warn-once no-op.
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
		// SequenceSubsystem->LevelSequenceTable intentionally left at its default (nullptr).

		ULevelLifecycleTestListener* Listener = NewObject<ULevelLifecycleTestListener>();
		LifecycleSubsystem->OnRunComplete.AddDynamic(Listener, &ULevelLifecycleTestListener::HandleRunComplete);

		AddExpectedError(TEXT("no LevelSequenceTable row"), EAutomationExpectedErrorFlags::Contains, 1, false);

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

		TestEqual(TEXT("ComputeNextLevelMapName should return NAME_None with an unset LevelSequenceTable"),
			SequenceSubsystem->ComputeNextLevelMapName(), FName(NAME_None));
		TestEqual(TEXT("FinalMapName must stay untouched with an unset LevelSequenceTable"),
			LifecycleSubsystem->FinalMapName, FName(NAME_None));
		TestEqual(TEXT("OnRunComplete must not fire with an unset LevelSequenceTable"),
			Listener->RunCompleteCallCount, 0);
	}

	// (e) GetShippedLevelMapNames(): issue #325's shared-authority enumeration. A
	// multi-row table returns every row name in table order; an unset table (today's
	// real production default, per case (d) above) returns an empty array rather than
	// crashing or warning - this is a pure query, not the warn-once
	// FindCurrentMapRow()/HandleLevelClear() path, so no AddExpectedError() is needed
	// here.
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

		TestEqual(TEXT("GetShippedLevelMapNames should be empty with an unset LevelSequenceTable"),
			SequenceSubsystem->GetShippedLevelMapNames().Num(), 0);

		UDataTable* Table = NewObject<UDataTable>();
		Table->RowStruct = FLevelSequenceRow::StaticStruct();
		FLevelSequenceRow Row1; Row1.NextLevelMapName = FName(TEXT("L_Level02"));
		Table->AddRow(FName(TEXT("L_Level01")), Row1);
		FLevelSequenceRow Row2; Row2.NextLevelMapName = FName(TEXT("L_Level03"));
		Table->AddRow(FName(TEXT("L_Level02")), Row2);
		FLevelSequenceRow Row3; Row3.NextLevelMapName = NAME_None;
		Table->AddRow(FName(TEXT("L_Level03")), Row3);
		SequenceSubsystem->LevelSequenceTable = Table;

		const TArray<FName> ShippedLevels = SequenceSubsystem->GetShippedLevelMapNames();
		TestEqual(TEXT("GetShippedLevelMapNames should return all 3 configured rows"),
			ShippedLevels.Num(), 3);
		TestTrue(TEXT("GetShippedLevelMapNames should include L_Level01"), ShippedLevels.Contains(FName(TEXT("L_Level01"))));
		TestTrue(TEXT("GetShippedLevelMapNames should include L_Level02"), ShippedLevels.Contains(FName(TEXT("L_Level02"))));
		TestTrue(TEXT("GetShippedLevelMapNames should include L_Level03"), ShippedLevels.Contains(FName(TEXT("L_Level03"))));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
