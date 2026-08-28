// Confirms UCrowdMasteryTotalSubsystem (PRD "Crowd Mastery Persistence" REQ-1, issue
// #327) starts at 0, accumulates deposits across simulated runs rather than
// overwriting, clamps negative deposits to 0, and resets to 0 without breaking
// subsequent deposits.
//
// No UWorld/CreateNewMap() needed - same "no engine-object dependency" rationale
// KrowdKontrolLevelClearTimeSubsystemTest.cpp documents: this subsystem's public API
// never calls GetWorld() or GetGameInstance(), so it's constructed directly via
// NewObject<>(). This test also exercises the save/reload round trip (PRD "Crowd
// Mastery Persistence" REQ-4, issue #330) and cleans up the shared on-disk save slot
// both before and after, matching KrowdKontrolLevelClearTimeSubsystemTest.cpp's own
// precedent for that same shared slot.

#include "Misc/AutomationTest.h"
#include "CrowdMasteryTotalSubsystem.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "LevelClearTimeSaveGame.h"
#include "LevelClearTimeSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolCrowdMasteryTotalSubsystemTest,
	"KrowdKontrol.Unit.CrowdMasteryTotalSubsystem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolCrowdMasteryTotalSubsystemTest::RunTest(const FString& Parameters)
{
	// Clean slate: this test now write-throughs to the same real on-disk slot
	// KrowdKontrolLevelClearTimeSubsystemTest.cpp also uses - delete any leftover save
	// data from a prior interrupted run before asserting starting state.
	UGameplayStatics::DeleteGameInSlot(ULevelClearTimeSubsystem::SaveSlotName, 0);

	// Pre-populate the shared save object with unrelated stats before this subsystem
	// ever touches the slot, mirroring KrowdKontrolLevelClearTimeSubsystemTest.cpp's
	// own "sibling field survives unrelated writes" check added for issue #174 -
	// proves PersistAccumulatedTotal()'s load-then-mutate-then-save discipline
	// doesn't silently wipe BestClearTimesByLevel/BestCrowdMasteryByLevel.
	const FName SiblingFieldLevelID(TEXT("KrowdKontrol.Unit.CrowdMasteryTotalSubsystem.SeedLevel"));
	{
		ULevelClearTimeSaveGame* SeedSaveGame = CastChecked<ULevelClearTimeSaveGame>(
			UGameplayStatics::CreateSaveGameObject(ULevelClearTimeSaveGame::StaticClass()));
		SeedSaveGame->BestClearTimesByLevel.Add(SiblingFieldLevelID, 42.0f);
		SeedSaveGame->BestCrowdMasteryByLevel.Add(SiblingFieldLevelID, 7);
		UGameplayStatics::SaveGameToSlot(SeedSaveGame, ULevelClearTimeSubsystem::SaveSlotName, 0);
	}

	UGameInstance* GameInstanceOuter = NewObject<UGameInstance>();
	UCrowdMasteryTotalSubsystem* Subsystem = NewObject<UCrowdMasteryTotalSubsystem>(GameInstanceOuter);
	if (!TestNotNull(TEXT("UCrowdMasteryTotalSubsystem should construct"), Subsystem))
	{
		return false;
	}

	// Initial state: 0 before any deposit.
	TestEqual(TEXT("A freshly-constructed subsystem should start at 0"),
		Subsystem->GetAccumulatedTotal(), 0);

	// Deposit-on-clear from a single run.
	Subsystem->DepositRunMastery(5);
	TestEqual(TEXT("A single deposit should be reflected in the total"),
		Subsystem->GetAccumulatedTotal(), 5);

	// Accumulation across two simulated runs - proves accumulation, not overwrite.
	Subsystem->DepositRunMastery(3);
	TestEqual(TEXT("A second deposit should accumulate onto the first, not overwrite it"),
		Subsystem->GetAccumulatedTotal(), 8);

	// Negative deposit is clamped to 0, never subtracted from the running total.
	Subsystem->DepositRunMastery(-10);
	TestEqual(TEXT("A negative deposit should be clamped to 0 and leave the total unchanged"),
		Subsystem->GetAccumulatedTotal(), 8);

	// Reset-to-zero.
	Subsystem->ResetAccumulatedTotal();
	TestEqual(TEXT("ResetAccumulatedTotal should zero the total"),
		Subsystem->GetAccumulatedTotal(), 0);

	// Post-reset deposit still works - proves reset doesn't leave the subsystem broken.
	Subsystem->DepositRunMastery(2);
	TestEqual(TEXT("A deposit after reset should be recorded normally"),
		Subsystem->GetAccumulatedTotal(), 2);

	// Save/reload cycle (PRD "Crowd Mastery Persistence" REQ-4, issue #330): a
	// fresh, independently-constructed subsystem instance (simulating closing and
	// reopening the game - a fresh UGameInstance/subsystem, sharing only the
	// on-disk save slot) reads back the same accumulated total via
	// LoadPersistedTotal(), the method the real Initialize() override calls at
	// real GameInstance startup - called directly here since this test
	// constructs the subsystem via NewObject<>() rather than through a live
	// FSubsystemCollectionBase.
	UGameInstance* SecondGameInstanceOuter = NewObject<UGameInstance>();
	UCrowdMasteryTotalSubsystem* SecondSessionSubsystem = NewObject<UCrowdMasteryTotalSubsystem>(SecondGameInstanceOuter);
	if (!TestNotNull(TEXT("Second UCrowdMasteryTotalSubsystem should construct"), SecondSessionSubsystem))
	{
		UGameplayStatics::DeleteGameInSlot(ULevelClearTimeSubsystem::SaveSlotName, 0);
		return false;
	}
	TestEqual(TEXT("A freshly-constructed subsystem should start at 0 before LoadPersistedTotal is called"),
		SecondSessionSubsystem->GetAccumulatedTotal(), 0);
	SecondSessionSubsystem->LoadPersistedTotal();
	TestEqual(TEXT("LoadPersistedTotal should read back the total the first session's last write-through persisted"),
		SecondSessionSubsystem->GetAccumulatedTotal(), 2);

	// Confirm the seeded sibling fields survived every PersistAccumulatedTotal()
	// write-through this test triggered above - the same property
	// KrowdKontrolLevelClearTimeSubsystemTest.cpp:186-193 checks for issue #174.
	if (USaveGame* Loaded = UGameplayStatics::LoadGameFromSlot(ULevelClearTimeSubsystem::SaveSlotName, 0))
	{
		if (ULevelClearTimeSaveGame* Typed = Cast<ULevelClearTimeSaveGame>(Loaded))
		{
			const float* SeedClearTime = Typed->BestClearTimesByLevel.Find(SiblingFieldLevelID);
			TestTrue(TEXT("Seeded BestClearTimesByLevel entry should still exist after Crowd Mastery total writes"), SeedClearTime != nullptr);
			if (SeedClearTime)
			{
				TestEqual(TEXT("Seeded BestClearTimesByLevel value should be unchanged by Crowd Mastery total writes"), *SeedClearTime, 42.0f);
			}
			const int32* SeedBest = Typed->BestCrowdMasteryByLevel.Find(SiblingFieldLevelID);
			TestTrue(TEXT("Seeded BestCrowdMasteryByLevel entry should still exist after Crowd Mastery total writes"), SeedBest != nullptr);
			if (SeedBest)
			{
				TestEqual(TEXT("Seeded BestCrowdMasteryByLevel value should be unchanged by Crowd Mastery total writes"), *SeedBest, 7);
			}
		}
	}

	// LoadOrCreateSaveGame()'s wrong-type fallback branch (test-coverage review LOW
	// finding): saving a plain USaveGame (not a ULevelClearTimeSaveGame) to the slot
	// should make LoadOrCreateSaveGame() fall through to CreateSaveGameObject() rather
	// than crash on the failed Cast, leaving LoadPersistedTotal() at a safe 0. The
	// sibling "LoadGameFromSlot returns nullptr despite DoesSaveGameExist reporting
	// true" branch has no cheap way to force from a test (would need a corrupted file
	// or a fake ISaveGameSystem), so it's left for the follow-up issue the review
	// suggested rather than faked here.
	UGameplayStatics::DeleteGameInSlot(ULevelClearTimeSubsystem::SaveSlotName, 0);
	{
		USaveGame* WrongTypeSaveGame = UGameplayStatics::CreateSaveGameObject(USaveGame::StaticClass());
		UGameplayStatics::SaveGameToSlot(WrongTypeSaveGame, ULevelClearTimeSubsystem::SaveSlotName, 0);
	}
	UGameInstance* WrongTypeGameInstanceOuter = NewObject<UGameInstance>();
	UCrowdMasteryTotalSubsystem* WrongTypeSubsystem = NewObject<UCrowdMasteryTotalSubsystem>(WrongTypeGameInstanceOuter);
	if (TestNotNull(TEXT("UCrowdMasteryTotalSubsystem should construct for the wrong-type fallback check"), WrongTypeSubsystem))
	{
		WrongTypeSubsystem->LoadPersistedTotal();
		TestEqual(TEXT("LoadPersistedTotal should fall back to 0 when the save slot holds a non-ULevelClearTimeSaveGame object, not crash"),
			WrongTypeSubsystem->GetAccumulatedTotal(), 0);
	}

	// Clean up all real on-disk state this test created, so a repeat run starts
	// from the same clean slate this run began with - same cleanup obligation
	// KrowdKontrolLevelClearTimeSubsystemTest.cpp documents for the same shared
	// slot.
	UGameplayStatics::DeleteGameInSlot(ULevelClearTimeSubsystem::SaveSlotName, 0);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
