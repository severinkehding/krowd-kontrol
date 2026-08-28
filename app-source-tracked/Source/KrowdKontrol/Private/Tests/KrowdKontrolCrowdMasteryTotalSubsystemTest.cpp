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

	// Clean up all real on-disk state this test created, so a repeat run starts
	// from the same clean slate this run began with - same cleanup obligation
	// KrowdKontrolLevelClearTimeSubsystemTest.cpp documents for the same shared
	// slot.
	UGameplayStatics::DeleteGameInSlot(ULevelClearTimeSubsystem::SaveSlotName, 0);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
