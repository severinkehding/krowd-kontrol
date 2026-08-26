// Confirms UCrowdMasterySubsystem (issue #174, docs/prd-run-lifecycle.md REQ-5, "Crowd Mastery") tracks a
// running max of simultaneously-Controlled AEnemyBase instances - sampled via
// SampleControlledCount()/HandleAbilityCastApplied()/HandleEnemyControlledExpired(),
// never decreasing once a peak is reached within a level - and resets that running max
// to 0 for real when ULevelLifecycleSubsystem::OnLevelBegin fires, via the
// Initialize()-time AddDynamic binding (not just a manually-simulated call). Also
// confirms AEnemyBase::OnEnemyControlledExpired (this issue's own new delegate) fires
// exactly once from a real TickControlledDuration()-driven expiry, and that
// Initialize() binds HandleLevelClear to the sibling OnLevelClear delegate the same
// real-AddDynamic way (issue #327, docs/prd-crowd-mastery-persistence.md REQ-1).
//
// SampleControlledCount()/HandleLevelBegin() are called/driven directly (never via a
// real per-frame Tick() loop, since UCrowdMasterySubsystem is not tick-driven at all)
// for the same synchronous-determinism reasons KrowdKontrolLevelLifecycleSubsystemTest.cpp
// documents for its own RefreshLevelClearState()/OnWorldBeginPlay() calls.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "CrowdMasterySubsystem.h"
#include "LevelLifecycleSubsystem.h"
#include "EnemyBaseTestActor.h"
#include "EnemyControlledExpiredTestListener.h"
#include "AbilitySlot.h"
#include "AbilityData.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolCrowdMasterySubsystemTest,
	"KrowdKontrol.Unit.CrowdMasterySubsystem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolCrowdMasterySubsystemTest::RunTest(const FString& Parameters)
{
	const FVector ZeroDistanceLocation(0.0f, 0.0f, 0.0f);

	// (a) zero enemies: a fresh world with no enemies ever Controlled has a running
	// max of 0.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}

		UCrowdMasterySubsystem* Subsystem = World->GetSubsystem<UCrowdMasterySubsystem>();
		if (!TestNotNull(TEXT("UWorld should auto-instantiate UCrowdMasterySubsystem"), Subsystem))
		{
			return false;
		}

		Subsystem->SampleControlledCount();
		TestEqual(TEXT("Running max should be 0 with no enemies ever spawned"),
			Subsystem->GetRunningMaxControlledCount(), 0);
	}

	// (b) peak tracked across overlapping casts: 3 enemies driven to Controlled, each
	// sampled via HandleAbilityCastApplied (simulating the real OnAbilityCastApplied
	// broadcast site) - running max should reach 3.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}

		UCrowdMasterySubsystem* Subsystem = World->GetSubsystem<UCrowdMasterySubsystem>();
		if (!TestNotNull(TEXT("UWorld should auto-instantiate UCrowdMasterySubsystem"), Subsystem))
		{
			return false;
		}

		for (int32 i = 0; i < 3; ++i)
		{
			AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
			if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), Enemy))
			{
				return false;
			}
			Enemy->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
			Enemy->TickCheckDetection(ZeroDistanceLocation); // Alert -> Attack
			Enemy->ReceiveControl(EAbilitySlot::Stun);        // -> Controlled
			Subsystem->HandleAbilityCastApplied(EAbilitySlot::Stun, Enemy);
		}

		TestEqual(TEXT("Running max should reach 3 after 3 overlapping Controlled enemies"),
			Subsystem->GetRunningMaxControlledCount(), 3);

		// (c) peak survives expiry: one of the three expires, but the running max is a
		// max, not a current count - it must not decrease.
		AEnemyBaseTestActor* ExpiringEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), ExpiringEnemy))
		{
			return false;
		}
		ExpiringEnemy->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
		ExpiringEnemy->ReceiveControl(EAbilitySlot::Stun);        // Alert -> Controlled
		const float StunDurationSeconds = AbilityData::Get(EAbilitySlot::Stun).BaseDurationSeconds;
		ExpiringEnemy->TickControlledDuration(StunDurationSeconds - 1.0f);
		ExpiringEnemy->TickControlledDuration(1.5f); // total elapsed now exceeds StunDurationSeconds -> Alert
		Subsystem->HandleEnemyControlledExpired();

		TestEqual(TEXT("Running max should still be 3 after one Controlled enemy expires"),
			Subsystem->GetRunningMaxControlledCount(), 3);
	}

	// (d) true-peak accuracy: 2 enemies Controlled simultaneously (true peak), one
	// expires, then a 3rd casts while only 1 remains Controlled - the tracked max must
	// be the true historical peak (2), never the final or first sample.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}

		UCrowdMasterySubsystem* Subsystem = World->GetSubsystem<UCrowdMasterySubsystem>();
		if (!TestNotNull(TEXT("UWorld should auto-instantiate UCrowdMasterySubsystem"), Subsystem))
		{
			return false;
		}

		AEnemyBaseTestActor* FirstEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		AEnemyBaseTestActor* SecondEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), FirstEnemy)
			|| !TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), SecondEnemy))
		{
			return false;
		}

		FirstEnemy->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
		FirstEnemy->ReceiveControl(EAbilitySlot::Stun);        // -> Controlled
		Subsystem->HandleAbilityCastApplied(EAbilitySlot::Stun, FirstEnemy);

		SecondEnemy->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
		SecondEnemy->ReceiveControl(EAbilitySlot::Stun);        // -> Controlled
		Subsystem->HandleAbilityCastApplied(EAbilitySlot::Stun, SecondEnemy);

		TestEqual(TEXT("True peak of 2 simultaneously-Controlled enemies should be tracked"),
			Subsystem->GetRunningMaxControlledCount(), 2);

		const float StunDurationSeconds = AbilityData::Get(EAbilitySlot::Stun).BaseDurationSeconds;
		FirstEnemy->TickControlledDuration(StunDurationSeconds - 1.0f);
		FirstEnemy->TickControlledDuration(1.5f); // FirstEnemy expires -> Alert
		Subsystem->HandleEnemyControlledExpired();

		// Only SecondEnemy is Controlled now (count = 1), strictly below the peak of 2
		// reached above - would fail if SampleControlledCount() ever regressed from
		// FMath::Max(...) to a plain assignment (the current count would overwrite the
		// tracked peak here instead of leaving it at 2).
		TestEqual(TEXT("Running max must not drop to the current (lower) count after a peak was reached"),
			Subsystem->GetRunningMaxControlledCount(), 2);

		AEnemyBaseTestActor* ThirdEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), ThirdEnemy))
		{
			return false;
		}
		ThirdEnemy->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
		ThirdEnemy->ReceiveControl(EAbilitySlot::Stun);        // -> Controlled (only SecondEnemy + ThirdEnemy Controlled now = 2, but FirstEnemy already reverted)
		Subsystem->HandleAbilityCastApplied(EAbilitySlot::Stun, ThirdEnemy);

		TestEqual(TEXT("Tracked max should be the true historical peak (2), not the final or first sample"),
			Subsystem->GetRunningMaxControlledCount(), 2);
	}

	// (e) reset on OnLevelBegin: the running max resets to 0 for real when
	// ULevelLifecycleSubsystem::OnLevelBegin fires, via the Initialize()-time
	// AddDynamic binding - not just a manually-simulated HandleLevelBegin() call.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}

		ULevelLifecycleSubsystem* LifecycleSubsystem = World->GetSubsystem<ULevelLifecycleSubsystem>();
		UCrowdMasterySubsystem* Subsystem = World->GetSubsystem<UCrowdMasterySubsystem>();
		if (!TestNotNull(TEXT("UWorld should auto-instantiate ULevelLifecycleSubsystem"), LifecycleSubsystem)
			|| !TestNotNull(TEXT("UWorld should auto-instantiate UCrowdMasterySubsystem"), Subsystem))
		{
			return false;
		}

		// Fire OnLevelBegin once, before driving any enemies, for a clean "begin, then
		// peak, then begin again" sequence - running max is already 0, so this is not
		// itself an observable change.
		LifecycleSubsystem->OnWorldBeginPlay(*World);

		AEnemyBaseTestActor* FirstEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		AEnemyBaseTestActor* SecondEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), FirstEnemy)
			|| !TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), SecondEnemy))
		{
			return false;
		}
		FirstEnemy->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
		FirstEnemy->ReceiveControl(EAbilitySlot::Stun);        // -> Controlled
		Subsystem->HandleAbilityCastApplied(EAbilitySlot::Stun, FirstEnemy);
		SecondEnemy->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
		SecondEnemy->ReceiveControl(EAbilitySlot::Stun);        // -> Controlled
		Subsystem->HandleAbilityCastApplied(EAbilitySlot::Stun, SecondEnemy);

		TestEqual(TEXT("Running max should be 2 before a second OnLevelBegin"),
			Subsystem->GetRunningMaxControlledCount(), 2);

		// Fire OnLevelBegin for real a second time via Broadcast() (not
		// OnWorldBeginPlay(), which is guarded by bHasFiredLevelBegin and won't re-fire
		// on the same LifecycleSubsystem instance) - this actually exercises the
		// Initialize()-time AddDynamic binding, proving
		// UCrowdMasterySubsystem::HandleLevelBegin is really wired to
		// ULevelLifecycleSubsystem::OnLevelBegin, not just callable standalone.
		LifecycleSubsystem->OnLevelBegin.Broadcast(FName(*World->GetMapName()));

		TestEqual(TEXT("Running max should reset to 0 after a fresh OnLevelBegin"),
			Subsystem->GetRunningMaxControlledCount(), 0);
	}

	// (f) OnEnemyControlledExpired actually fires from a real TickControlledDuration
	// call - this issue's own new delegate, exercised end-to-end.
	{
		AEnemyBaseTestActor* Enemy = NewObject<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("AEnemyBaseTestActor should construct"), Enemy))
		{
			return false;
		}

		UEnemyControlledExpiredTestListener* Listener = NewObject<UEnemyControlledExpiredTestListener>();
		Enemy->OnEnemyControlledExpired.AddDynamic(Listener, &UEnemyControlledExpiredTestListener::HandleEnemyControlledExpired);

		Enemy->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
		Enemy->ReceiveControl(EAbilitySlot::Stun);              // Alert -> Controlled
		const float StunDurationSeconds = AbilityData::Get(EAbilitySlot::Stun).BaseDurationSeconds;
		Enemy->TickControlledDuration(StunDurationSeconds - 1.0f);
		TestEqual(TEXT("OnEnemyControlledExpired must not fire before the duration elapses"),
			Listener->CallCount, 0);
		Enemy->TickControlledDuration(1.5f); // total elapsed now exceeds StunDurationSeconds

		TestEqual(TEXT("OnEnemyControlledExpired should fire exactly once when the duration elapses"),
			Listener->CallCount, 1);
	}

	// (g) OnLevelClear wiring: Initialize() really binds HandleLevelClear to
	// ULevelLifecycleSubsystem::OnLevelClear, not just a standalone-callable method -
	// same "prove the real AddDynamic binding, not just the handler in isolation"
	// rationale case (e) above uses for the sibling OnLevelBegin binding. The
	// null-GameInstance early-return itself is covered "for free" by every other case
	// in this file already calling HandleLevelClear() indirectly-never (CreateNewMap()
	// worlds have no GameInstance, so it silently no-ops - see
	// ULevelLifecycleSubsystem::EnsureLevelClearTimeSubscription()'s identical
	// no-warning rationale for why that's not itself asserted here).
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}

		ULevelLifecycleSubsystem* LifecycleSubsystem = World->GetSubsystem<ULevelLifecycleSubsystem>();
		UCrowdMasterySubsystem* Subsystem = World->GetSubsystem<UCrowdMasterySubsystem>();
		if (!TestNotNull(TEXT("UWorld should auto-instantiate ULevelLifecycleSubsystem"), LifecycleSubsystem)
			|| !TestNotNull(TEXT("UWorld should auto-instantiate UCrowdMasterySubsystem"), Subsystem))
		{
			return false;
		}

		TestTrue(TEXT("Initialize() should bind HandleLevelClear to OnLevelClear"),
			LifecycleSubsystem->OnLevelClear.Contains(Subsystem,
				GET_FUNCTION_NAME_CHECKED(UCrowdMasterySubsystem, HandleLevelClear)));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
