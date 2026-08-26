// Confirms UCrowdMasteryTotalSubsystem (PRD "Crowd Mastery Persistence" REQ-1, issue
// #327) starts at 0, accumulates deposits across simulated runs rather than
// overwriting, clamps negative deposits to 0, and resets to 0 without breaking
// subsequent deposits.
//
// No UWorld/CreateNewMap() needed - same "no engine-object dependency" rationale
// KrowdKontrolLevelClearTimeSubsystemTest.cpp documents: this subsystem's public API
// never calls GetWorld() or GetGameInstance(), so it's constructed directly via
// NewObject<>(). Unlike that test, no on-disk save-file state exists here (REQ-4
// persistence is out of scope for this issue), so there's nothing to clean up.

#include "Misc/AutomationTest.h"
#include "CrowdMasteryTotalSubsystem.h"
#include "Engine/GameInstance.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolCrowdMasteryTotalSubsystemTest,
	"KrowdKontrol.Unit.CrowdMasteryTotalSubsystem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolCrowdMasteryTotalSubsystemTest::RunTest(const FString& Parameters)
{
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

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
