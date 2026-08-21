#include "Misc/AutomationTest.h"
#include "SpeedReductionPunishmentComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"

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

	// (d) EndPlay() must clear a pending restore timer, mirroring
	// WaveSpawnerComponent's case (7). This harness never drives the World through
	// World->BeginPlay(), so DestroyComponent() alone never reaches EndPlay()
	// (UActorComponent only calls it when bHasBegunPlay is true), and calling
	// EndPlay() directly on a component that never began play hits UE 5.8's
	// check(bHasBegunPlay) engine assert (ActorComponent.cpp) - see
	// KrowdKontrolWaveSpawnerComponentTest.cpp's case (7) comment for the incident
	// this caused previously. AActor::DispatchBeginPlay() is the legal route.
	{
		UWorld* EndPlayWorld = FAutomationEditorCommonUtils::CreateNewMap();
		AActor* EndPlayOwner = EndPlayWorld->SpawnActor<AActor>();

		UFloatingPawnMovement* EndPlayMovement = NewObject<UFloatingPawnMovement>(EndPlayOwner);
		EndPlayMovement->RegisterComponent();
		EndPlayMovement->MaxSpeed = 1200.0f;

		USpeedReductionPunishmentComponent* EndPlaySpeedReduction = NewObject<USpeedReductionPunishmentComponent>(EndPlayOwner);
		EndPlaySpeedReduction->RegisterComponent();
		EndPlaySpeedReduction->MovementComponent = EndPlayMovement;
		EndPlayOwner->DispatchBeginPlay();

		EndPlaySpeedReduction->HandlePunishmentTriggered();
		TestTrue(TEXT("Restore timer should be pending right after activation"),
			EndPlaySpeedReduction->IsSpeedReductionTimerActive());

		EndPlaySpeedReduction->EndPlay(EEndPlayReason::Destroyed);

		TestFalse(TEXT("EndPlay() should clear the pending restore timer"),
			EndPlaySpeedReduction->IsSpeedReductionTimerActive());
	}

	// (e) MovementComponent left unwired must no-op, not crash - the pre-wiring
	// guard both prototype pawns' constructors make unreachable in production but
	// that any future adopter of this component depends on. Uses its own fresh
	// CreateNewMap() rather than reusing the original World above - (d) already
	// replaced the editor's current map with its own, and reusing a World from
	// before the most recent CreateNewMap() call hits an engine-side
	// "Assertion failed: CurrentLevel" (LevelActor.cpp) in SpawnActor.
	{
		UWorld* UnwiredWorld = FAutomationEditorCommonUtils::CreateNewMap();
		AActor* UnwiredOwner = UnwiredWorld->SpawnActor<AActor>();
		USpeedReductionPunishmentComponent* Unwired = NewObject<USpeedReductionPunishmentComponent>(UnwiredOwner);
		Unwired->RegisterComponent();
		Unwired->HandlePunishmentTriggered(); // should no-op, not crash
	}

	// (f) kk.Punishment.SpeedReductionEnabled=0 prevents HandlePunishmentTriggered
	// from reducing MaxSpeed; restoring the CVar to 1 (its default) immediately
	// allows normal activation again. Same process-wide-CVar restore rationale as
	// KrowdKontrolAbilityLockoutComponentTest.cpp's equivalent case.
	{
		IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("kk.Punishment.SpeedReductionEnabled"));
		if (!TestNotNull(TEXT("kk.Punishment.SpeedReductionEnabled CVar should be registered"), CVar))
		{
			return false;
		}

		UWorld* CVarWorld = FAutomationEditorCommonUtils::CreateNewMap();
		AActor* CVarOwner = CVarWorld->SpawnActor<AActor>();

		UFloatingPawnMovement* CVarMovement = NewObject<UFloatingPawnMovement>(CVarOwner);
		CVarMovement->RegisterComponent();
		CVarMovement->MaxSpeed = 1200.0f;

		USpeedReductionPunishmentComponent* CVarSpeedReduction = NewObject<USpeedReductionPunishmentComponent>(CVarOwner);
		CVarSpeedReduction->RegisterComponent();
		CVarSpeedReduction->MovementComponent = CVarMovement;
		CVarSpeedReduction->SpeedMultiplierWhileActive = 0.5f;

		CVar->Set(0);
		CVarSpeedReduction->HandlePunishmentTriggered();
		TestEqual(TEXT("MaxSpeed should stay unchanged while kk.Punishment.SpeedReductionEnabled is 0"),
			CVarMovement->MaxSpeed, 1200.0f);
		TestFalse(TEXT("bIsSpeedReductionActive should stay false while the CVar is 0"),
			CVarSpeedReduction->bIsSpeedReductionActive);

		CVar->Set(1);
		CVarSpeedReduction->HandlePunishmentTriggered();
		TestEqual(TEXT("MaxSpeed should reduce normally once kk.Punishment.SpeedReductionEnabled is restored to 1"),
			CVarMovement->MaxSpeed, 600.0f);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
