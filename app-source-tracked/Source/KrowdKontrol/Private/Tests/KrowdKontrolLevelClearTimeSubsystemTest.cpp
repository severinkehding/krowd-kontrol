// Confirms ULevelClearTimeSubsystem (issue #3, PRD 06 REQ-2) tracks and persists a
// per-level personal-best clear time: a first clear with no prior record becomes the
// best, a slower clear never overwrites it, a faster clear does, records stay keyed
// per level rather than global, and the best time survives a fresh subsystem
// instance (simulating closing and reopening the game).
//
// No UWorld/CreateNewMap() needed - matches KrowdKontrolGizmoNarrativeSubsystemTest.cpp's
// "no engine-object dependency" precedent: this subsystem's public API never calls
// GetWorld() or GetGameInstance() (GetGameInstance() is null in this project's
// CreateNewMap()-based Automation test worlds anyway, per PlaceholderTerminalActor.h's
// documented rationale), so it's constructed directly via NewObject<>().
//
// UGameplayStatics::SaveGameToSlot/LoadGameFromSlot write real files under this
// project's Saved/SaveGames/ directory - unlike every other NewObject<>()-constructed
// test fixture in this module, that state survives across separate Automation runs.
// This test deletes its save slot both before (guaranteeing the "no prior record"
// starting state acceptance criteria (a) depends on) and after (leaving no stray
// on-disk state for a repeat run).
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "LevelClearTimeSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolLevelClearTimeSubsystemTest,
	"KrowdKontrol.Unit.LevelClearTimeSubsystem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolLevelClearTimeSubsystemTest::RunTest(const FString& Parameters)
{
	const FName TestLevelID = TEXT("KrowdKontrol.Unit.LevelClearTimeSubsystem.TestLevel");

	// Clean slate: delete any leftover save data from a prior run of this same test
	// before asserting "no prior record" behaviour (see this file's top comment for why
	// this step is necessary, unlike every other test in this module).
	UGameplayStatics::DeleteGameInSlot(ULevelClearTimeSubsystem::SaveSlotName, 0);

	UGameInstance* GameInstanceOuter = NewObject<UGameInstance>();
	ULevelClearTimeSubsystem* Subsystem = NewObject<ULevelClearTimeSubsystem>(GameInstanceOuter);
	if (!TestNotNull(TEXT("ULevelClearTimeSubsystem should construct"), Subsystem))
	{
		UGameplayStatics::DeleteGameInSlot(ULevelClearTimeSubsystem::SaveSlotName, 0);
		return false;
	}

	float OutBest = 0.0f;
	TestFalse(TEXT("No best time should exist before any clear is recorded"),
		Subsystem->GetBestClearTimeSeconds(TestLevelID, OutBest));

	// (a) First clear with no prior record becomes the best.
	TestTrue(TEXT("First clear on a level with no record should become the best"),
		Subsystem->RecordClearTime(TestLevelID, 120.0f));
	TestTrue(TEXT("A best time should now exist"), Subsystem->GetBestClearTimeSeconds(TestLevelID, OutBest));
	TestEqual(TEXT("Best time should equal the first clear's time"), OutBest, 120.0f);

	// (b) A slower clear does not overwrite the best.
	TestFalse(TEXT("A slower clear should not become the new best"),
		Subsystem->RecordClearTime(TestLevelID, 150.0f));
	Subsystem->GetBestClearTimeSeconds(TestLevelID, OutBest);
	TestEqual(TEXT("Best time should remain the faster, first clear"), OutBest, 120.0f);

	// (c) A faster clear overwrites the best.
	TestTrue(TEXT("A faster clear should become the new best"),
		Subsystem->RecordClearTime(TestLevelID, 90.0f));
	Subsystem->GetBestClearTimeSeconds(TestLevelID, OutBest);
	TestEqual(TEXT("Best time should now be the faster clear"), OutBest, 90.0f);

	// Keyed per level, not global: a distinct level has no best time of its own.
	const FName OtherLevelID = TEXT("KrowdKontrol.Unit.LevelClearTimeSubsystem.OtherTestLevel");
	TestFalse(TEXT("A different level should have no best time of its own"),
		Subsystem->GetBestClearTimeSeconds(OtherLevelID, OutBest));

	// Persists across sessions: a second, independently-constructed subsystem instance
	// (simulating closing and reopening the game - a fresh UGameInstance/subsystem,
	// sharing only the on-disk save slot) reads back the same best time.
	UGameInstance* SecondGameInstanceOuter = NewObject<UGameInstance>();
	ULevelClearTimeSubsystem* SecondSessionSubsystem = NewObject<ULevelClearTimeSubsystem>(SecondGameInstanceOuter);
	float SecondSessionBest = 0.0f;
	TestTrue(TEXT("A fresh subsystem instance should read back the persisted best time"),
		SecondSessionSubsystem->GetBestClearTimeSeconds(TestLevelID, SecondSessionBest));
	TestEqual(TEXT("Persisted best time should match what the first session saved"), SecondSessionBest, 90.0f);

	// StopLevelTimerAndRecordClear with no matching StartLevelTimer logs a warning and
	// no-ops rather than crashing or recording a bogus time.
	const FName UnstartedLevelID = TEXT("KrowdKontrol.Unit.LevelClearTimeSubsystem.NeverStarted");
	AddExpectedError(TEXT("no active timer"), EAutomationExpectedErrorFlags::Contains, 1);
	const float ElapsedForUnstarted = Subsystem->StopLevelTimerAndRecordClear(UnstartedLevelID);
	TestEqual(TEXT("Stopping a never-started timer should return 0 elapsed seconds"), ElapsedForUnstarted, 0.0f);
	TestFalse(TEXT("Stopping a never-started timer should not create a record"),
		Subsystem->GetBestClearTimeSeconds(UnstartedLevelID, OutBest));

	// A real Start/Stop cycle measures a non-negative elapsed time and records it as
	// that level's best (no prior record exists for this key).
	const FName TimedLevelID = TEXT("KrowdKontrol.Unit.LevelClearTimeSubsystem.TimedLevel");
	Subsystem->StartLevelTimer(TimedLevelID);
	const float ElapsedForTimed = Subsystem->StopLevelTimerAndRecordClear(TimedLevelID);
	TestTrue(TEXT("A real Start/Stop cycle should measure a non-negative elapsed time"), ElapsedForTimed >= 0.0f);
	float TimedBest = 0.0f;
	TestTrue(TEXT("The timed clear should be recorded as that level's best"),
		Subsystem->GetBestClearTimeSeconds(TimedLevelID, TimedBest));
	TestEqual(TEXT("The recorded best should equal the measured elapsed time"), TimedBest, ElapsedForTimed);

	// Clean up all real on-disk state this test created, so a repeat run starts from
	// the same clean slate this run began with.
	UGameplayStatics::DeleteGameInSlot(ULevelClearTimeSubsystem::SaveSlotName, 0);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
