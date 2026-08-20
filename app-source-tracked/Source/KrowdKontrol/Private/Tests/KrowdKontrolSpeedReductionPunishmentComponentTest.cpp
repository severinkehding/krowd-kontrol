#include "Misc/AutomationTest.h"
#include "SpeedReductionPunishmentComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolSpeedReductionPunishmentComponentTest,
	"KrowdKontrol.Unit.SpeedReductionPunishmentComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolSpeedReductionPunishmentComponentTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	AActor* Owner = World->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("AActor owner should spawn into the test World"), Owner))
	{
		return false;
	}

	UFloatingPawnMovement* Movement = NewObject<UFloatingPawnMovement>(Owner);
	Movement->RegisterComponent();
	Movement->MaxSpeed = 1200.0f;

	USpeedReductionPunishmentComponent* SpeedReduction = NewObject<USpeedReductionPunishmentComponent>(Owner);
	SpeedReduction->RegisterComponent();
	SpeedReduction->MovementComponent = Movement;
	SpeedReduction->SpeedMultiplierWhileActive = 0.5f;
	SpeedReduction->SpeedReductionDurationSeconds = 3.0f;

	// (a) Activation applies the configured factor.
	SpeedReduction->HandlePunishmentTriggered();
	TestEqual(TEXT("MaxSpeed should be reduced to 50% of its original value on activation"),
		Movement->MaxSpeed, 600.0f);

	// (c) Re-triggering while already active must not compound the reduction -
	// checked before (b) restores, since restoring first would make this
	// indistinguishable from a fresh activation. No real-time wait: the
	// codebase's established idiom (KrowdKontrolAbilityVFXColourTest.cpp) is to
	// call the timer callback directly rather than advance a live FTimerManager;
	// here we instead call the public trigger handler again, which is the real
	// production re-trigger path.
	SpeedReduction->HandlePunishmentTriggered();
	TestEqual(TEXT("Re-triggering while already active should not compound the reduction"),
		Movement->MaxSpeed, 600.0f);

	// (b) Expiry restores the real original value. RestoreOriginalSpeed is
	// private and timer-driven in production; called directly here via the
	// friend-grant idiom (mirrors ClearCastFlash() in
	// KrowdKontrolAbilityVFXColourTest.cpp) since there's no precedent in this
	// test suite for advancing a live FTimerManager.
	SpeedReduction->RestoreOriginalSpeed();
	TestEqual(TEXT("MaxSpeed should restore to its original value after expiry"),
		Movement->MaxSpeed, 1200.0f);

	// Sanity: OriginalMaxSpeed captured 1200, not the already-reduced 600 from
	// the (c) re-trigger above - proves the IsTimerActive guard actually
	// prevented re-capture, not just re-application of the multiplier.
	TestEqual(TEXT("OriginalMaxSpeed should have captured the real pre-punishment value, not a reduced one"),
		SpeedReduction->OriginalMaxSpeed, 1200.0f);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
