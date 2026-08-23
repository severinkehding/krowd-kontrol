// Confirms ULevelLifecycleSubsystem (issue #169, PRD "Run Lifecycle & Progression
// Signals" REQ-1) fires OnLevelBegin exactly once at world begin-play (carrying the
// map name) and OnLevelClear exactly once, after OnLevelBegin, the moment every
// AEnemyBase in the world (including ones spawned later by a UWaveSpawnerComponent)
// is Banked and no spawner has a pending wave - and that it does NOT fire while zero
// enemies have ever spawned, while OnLevelBegin has never fired, or while any
// spawner's IsWaveTimerActive() is true even with every currently-spawned enemy
// already Banked. Also confirms OnLevelBegin's own re-entrancy guard.
//
// RefreshLevelClearState()/OnWorldBeginPlay() are called directly in most cases (never
// via World->BeginPlay()) for the same synchronous-determinism reasons
// KrowdKontrolMusicSubsystemTest.cpp documents. Case (j) is the one exception: it
// drives Subsystem->Tick() explicitly (a single manual call per assertion, still not a
// real per-frame engine tick loop) to exercise the bHasLoggedFirstTick guard added by
// issue #234, since RefreshLevelClearState() alone bypasses Tick() entirely.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "LevelLifecycleSubsystem.h"
#include "LevelLifecycleTestListener.h"
#include "EnemyBaseTestActor.h"
#include "AbilitySlot.h"
#include "WaveSpawnerComponent.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolLevelLifecycleSubsystemTest,
	"KrowdKontrol.Unit.LevelLifecycleSubsystem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolLevelLifecycleSubsystemTest::RunTest(const FString& Parameters)
{
	const FVector ZeroDistanceLocation(0.0f, 0.0f, 0.0f);

	// --- (a)/(b): zero-enemy guard, begin-fires-once, spawn->bank->clear-fires-once,
	// clear-fires-after-begin, and exactly-once re-entrancy guards on both.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}

		ULevelLifecycleSubsystem* Subsystem = World->GetSubsystem<ULevelLifecycleSubsystem>();
		if (!TestNotNull(TEXT("UWorld should auto-instantiate ULevelLifecycleSubsystem"), Subsystem))
		{
			return false;
		}

		// (i): IsTickableWhenPaused() must return true - this subsystem's Tick() must not
		// silently stall if the world is ever paused (e.g. UBriefingCardWidget's briefing
		// card). Can't be exercised via a real paused Tick() here - CreateNewMap() worlds
		// have no live AGameModeBase, so UGameplayStatics::SetGamePaused() is a documented
		// no-op (see KrowdKontrolAbilityCastComponentTest.cpp) - so this is a direct
		// tripwire on the override itself, not an end-to-end pause simulation.
		TestTrue(TEXT("ULevelLifecycleSubsystem must tick through pause - see IsTickableWhenPaused() override"),
			Subsystem->IsTickableWhenPaused());

		ULevelLifecycleTestListener* Listener = NewObject<ULevelLifecycleTestListener>();
		Subsystem->OnLevelBegin.AddDynamic(Listener, &ULevelLifecycleTestListener::HandleLevelBegin);
		Subsystem->OnLevelClear.AddDynamic(Listener, &ULevelLifecycleTestListener::HandleLevelClear);

		// (a) zero-enemy guard: no enemy has ever spawned yet.
		Subsystem->RefreshLevelClearState();
		TestEqual(TEXT("OnLevelClear must not fire with zero enemies ever spawned"),
			Listener->LevelClearCallCount, 0);

		// (b) OnLevelBegin fires exactly once, carrying the map name.
		Subsystem->OnWorldBeginPlay(*World);
		TestEqual(TEXT("OnLevelBegin should fire exactly once"), Listener->LevelBeginCallCount, 1);
		TestEqual(TEXT("OnLevelBegin should carry the world's map name"),
			Listener->LastLevelBeginMapName, FName(*World->GetMapName()));
		TestEqual(TEXT("OnLevelClear must not have fired yet"), Listener->LevelClearCallCount, 0);

		AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), Enemy))
		{
			return false;
		}
		Enemy->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
		Enemy->TickCheckDetection(ZeroDistanceLocation); // Alert -> Attack
		Enemy->ReceiveControl(EAbilitySlot::Stun);        // -> Controlled
		Enemy->TransitionToBanked();                      // -> Banked

		Subsystem->RefreshLevelClearState();
		TestEqual(TEXT("OnLevelClear should fire exactly once, after OnLevelBegin already fired"),
			Listener->LevelClearCallCount, 1);

		// A second RefreshLevelClearState() call must not re-fire OnLevelClear.
		Subsystem->RefreshLevelClearState();
		TestEqual(TEXT("A second RefreshLevelClearState() call must not re-fire OnLevelClear"),
			Listener->LevelClearCallCount, 1);
	}

	// --- (e): bHasFiredLevelBegin's own guard, isolated via friend access. This World's
	// OnWorldBeginPlay() is only ever called once here - calling Super::OnWorldBeginPlay()
	// twice on the same World hard-errors in the engine itself (a separate, unrelated
	// !bHasCalledBeginPlay ensure), so bHasFiredLevelBegin is force-set true *before* the
	// one call to isolate just this subsystem's own early-return branch.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}

		ULevelLifecycleSubsystem* Subsystem = World->GetSubsystem<ULevelLifecycleSubsystem>();
		if (!TestNotNull(TEXT("UWorld should auto-instantiate ULevelLifecycleSubsystem"), Subsystem))
		{
			return false;
		}

		ULevelLifecycleTestListener* Listener = NewObject<ULevelLifecycleTestListener>();
		Subsystem->OnLevelBegin.AddDynamic(Listener, &ULevelLifecycleTestListener::HandleLevelBegin);

		Subsystem->bHasFiredLevelBegin = true; // friend access - simulate "already fired"
		Subsystem->OnWorldBeginPlay(*World);
		TestEqual(TEXT("OnWorldBeginPlay must not broadcast OnLevelBegin when bHasFiredLevelBegin is already set"),
			Listener->LevelBeginCallCount, 0);
	}

	// --- (c): OnLevelClear must not fire before OnLevelBegin has ever fired, even if
	// every currently-spawned enemy is already Banked. A fresh CreateNewMap() world is
	// needed because block (a)/(b) already called OnWorldBeginPlay().
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}

		ULevelLifecycleSubsystem* Subsystem = World->GetSubsystem<ULevelLifecycleSubsystem>();
		if (!TestNotNull(TEXT("UWorld should auto-instantiate ULevelLifecycleSubsystem"), Subsystem))
		{
			return false;
		}

		ULevelLifecycleTestListener* Listener = NewObject<ULevelLifecycleTestListener>();
		Subsystem->OnLevelClear.AddDynamic(Listener, &ULevelLifecycleTestListener::HandleLevelClear);

		AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), Enemy))
		{
			return false;
		}
		Enemy->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
		Enemy->TickCheckDetection(ZeroDistanceLocation); // Alert -> Attack
		Enemy->ReceiveControl(EAbilitySlot::Stun);        // -> Controlled
		Enemy->TransitionToBanked();                      // -> Banked

		// OnWorldBeginPlay() has NOT been called on this Subsystem.
		Subsystem->RefreshLevelClearState();
		TestEqual(TEXT("OnLevelClear must not fire before OnLevelBegin has ever fired"),
			Listener->LevelClearCallCount, 0);
	}

	// --- (d): wave-spawner blocks clear even with every current enemy Banked, and a
	// wave-spawned enemy must itself reach Banked before OnLevelClear fires. A fresh
	// CreateNewMap() world is needed because (a)/(b)'s subsystem instance already
	// latched bHasFiredLevelClear.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}

		ULevelLifecycleSubsystem* Subsystem = World->GetSubsystem<ULevelLifecycleSubsystem>();
		if (!TestNotNull(TEXT("UWorld should auto-instantiate ULevelLifecycleSubsystem"), Subsystem))
		{
			return false;
		}

		// FinalMapName set to this test world's own map, so OnRunComplete's suppression
		// while a wave is pending isn't just implied by OnLevelClear's own gating (which
		// (d) already proves above) - it's independently observed here too, per this
		// class's early-return chain being the only thing standing between them.
		Subsystem->FinalMapName = FName(*World->GetMapName());

		ULevelLifecycleTestListener* Listener = NewObject<ULevelLifecycleTestListener>();
		Subsystem->OnLevelClear.AddDynamic(Listener, &ULevelLifecycleTestListener::HandleLevelClear);
		Subsystem->OnRunComplete.AddDynamic(Listener, &ULevelLifecycleTestListener::HandleRunComplete);

		Subsystem->OnWorldBeginPlay(*World);

		AEnemyBaseTestActor* FirstEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), FirstEnemy))
		{
			return false;
		}
		FirstEnemy->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
		FirstEnemy->TickCheckDetection(ZeroDistanceLocation); // Alert -> Attack
		FirstEnemy->ReceiveControl(EAbilitySlot::Stun);        // -> Controlled
		FirstEnemy->TransitionToBanked();                      // -> Banked

		AActor* OwnerActor = World->SpawnActor<AActor>();
		if (!TestNotNull(TEXT("Owner actor should spawn into the test World"), OwnerActor))
		{
			return false;
		}
		UWaveSpawnerComponent* Spawner = NewObject<UWaveSpawnerComponent>(OwnerActor);
		if (!TestNotNull(TEXT("UWaveSpawnerComponent should construct"), Spawner))
		{
			return false;
		}
		Spawner->RegisterComponent();

		FWaveEntry Wave;
		Wave.EnemyClass = AEnemyBaseTestActor::StaticClass();
		Wave.Count = 1;
		Wave.DelaySeconds = 9999.0f; // never fires on its own within this test
		Spawner->Waves.Add(Wave);
		Spawner->StartWaves(); // IsWaveTimerActive() becomes true here

		// OnLevelClear must NOT fire while the spawner has a pending wave, even though
		// the one currently-spawned enemy is already fully Banked.
		Subsystem->RefreshLevelClearState();
		TestEqual(TEXT("OnLevelClear must not fire while a spawner's IsWaveTimerActive() is true"),
			Listener->LevelClearCallCount, 0);
		TestEqual(TEXT("OnRunComplete must not fire while a spawner's IsWaveTimerActive() is true, even with FinalMapName matching"),
			Listener->RunCompleteCallCount, 0);

		Spawner->TriggerNextWave(); // spawns the wave's enemy immediately, clears the pending timer
		Subsystem->RefreshLevelClearState();
		TestEqual(TEXT("OnLevelClear must not fire while the newly wave-spawned enemy is not yet Banked"),
			Listener->LevelClearCallCount, 0);
		TestEqual(TEXT("OnRunComplete must not fire while the newly wave-spawned enemy is not yet Banked"),
			Listener->RunCompleteCallCount, 0);

		if (!TestEqual(TEXT("The wave should have spawned exactly one enemy"),
			Spawner->GetSpawnedActors().Num(), 1))
		{
			return false;
		}
		AEnemyBase* SpawnedEnemy = Cast<AEnemyBase>(Spawner->GetSpawnedActors()[0]);
		if (!TestNotNull(TEXT("The wave-spawned actor should be an AEnemyBase"), SpawnedEnemy))
		{
			return false;
		}
		SpawnedEnemy->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
		SpawnedEnemy->TickCheckDetection(ZeroDistanceLocation); // Alert -> Attack
		SpawnedEnemy->ReceiveControl(EAbilitySlot::Stun);        // -> Controlled
		SpawnedEnemy->TransitionToBanked();                      // -> Banked

		Subsystem->RefreshLevelClearState();
		TestEqual(TEXT("OnLevelClear should fire once every spawned enemy (including the wave-spawned one) is Banked and no spawner has a pending wave"),
			Listener->LevelClearCallCount, 1);
		TestEqual(TEXT("OnRunComplete should fire once OnLevelClear fires and FinalMapName matches, even though a wave was pending earlier in this same case"),
			Listener->RunCompleteCallCount, 1);
	}

	// --- (f): OnRunComplete fires exactly once when OnLevelClear fires for a world
	// whose map name matches the configured FinalMapName (issue #176, PRD REQ-7).
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}

		ULevelLifecycleSubsystem* Subsystem = World->GetSubsystem<ULevelLifecycleSubsystem>();
		if (!TestNotNull(TEXT("UWorld should auto-instantiate ULevelLifecycleSubsystem"), Subsystem))
		{
			return false;
		}

		Subsystem->FinalMapName = FName(*World->GetMapName());

		ULevelLifecycleTestListener* Listener = NewObject<ULevelLifecycleTestListener>();
		Subsystem->OnLevelClear.AddDynamic(Listener, &ULevelLifecycleTestListener::HandleLevelClear);
		Subsystem->OnRunComplete.AddDynamic(Listener, &ULevelLifecycleTestListener::HandleRunComplete);

		Subsystem->OnWorldBeginPlay(*World);

		AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), Enemy))
		{
			return false;
		}
		Enemy->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
		Enemy->TickCheckDetection(ZeroDistanceLocation); // Alert -> Attack
		Enemy->ReceiveControl(EAbilitySlot::Stun);        // -> Controlled
		Enemy->TransitionToBanked();                      // -> Banked

		Subsystem->RefreshLevelClearState();
		TestEqual(TEXT("OnLevelClear should still fire once the map matches FinalMapName"),
			Listener->LevelClearCallCount, 1);
		TestEqual(TEXT("OnRunComplete should fire exactly once when the cleared map matches FinalMapName"),
			Listener->RunCompleteCallCount, 1);

		// OnRunComplete's doc comment promises "immediately after OnLevelClear" - assert
		// the actual call order, not just that both counts landed on 1, so a future
		// refactor that reorders/splits the two Broadcast() calls fails this test.
		const TArray<FString> ExpectedOrder = { TEXT("LevelClear"), TEXT("RunComplete") };
		TestEqual(TEXT("OnRunComplete must fire immediately after OnLevelClear, not before or interleaved"),
			Listener->CallOrder, ExpectedOrder);

		// A second RefreshLevelClearState() call must not re-fire OnRunComplete, mirroring
		// OnLevelClear's own re-entrancy guarantee it rides on.
		Subsystem->RefreshLevelClearState();
		TestEqual(TEXT("A second RefreshLevelClearState() call must not re-fire OnRunComplete"),
			Listener->RunCompleteCallCount, 1);
	}

	// --- (g): OnRunComplete must NOT fire on level-clear for a non-final map (issue
	// #176 acceptance criteria's second required case).
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}

		ULevelLifecycleSubsystem* Subsystem = World->GetSubsystem<ULevelLifecycleSubsystem>();
		if (!TestNotNull(TEXT("UWorld should auto-instantiate ULevelLifecycleSubsystem"), Subsystem))
		{
			return false;
		}

		Subsystem->FinalMapName = FName(TEXT("SomeOtherMap_NotThisTestWorld"));

		ULevelLifecycleTestListener* Listener = NewObject<ULevelLifecycleTestListener>();
		Subsystem->OnLevelClear.AddDynamic(Listener, &ULevelLifecycleTestListener::HandleLevelClear);
		Subsystem->OnRunComplete.AddDynamic(Listener, &ULevelLifecycleTestListener::HandleRunComplete);

		Subsystem->OnWorldBeginPlay(*World);

		AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), Enemy))
		{
			return false;
		}
		Enemy->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
		Enemy->TickCheckDetection(ZeroDistanceLocation); // Alert -> Attack
		Enemy->ReceiveControl(EAbilitySlot::Stun);        // -> Controlled
		Enemy->TransitionToBanked();                      // -> Banked

		Subsystem->RefreshLevelClearState();
		TestEqual(TEXT("OnLevelClear should still fire for a non-final map"),
			Listener->LevelClearCallCount, 1);
		TestEqual(TEXT("OnRunComplete must not fire on level-clear for a non-final map"),
			Listener->RunCompleteCallCount, 0);
	}

	// --- (h): default FinalMapName (NAME_None) must never fire OnRunComplete, even
	// though it trivially can't equal any real map's FName either - this locks in the
	// "unconfigured means off" default explicitly rather than leaving it implicit.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}

		ULevelLifecycleSubsystem* Subsystem = World->GetSubsystem<ULevelLifecycleSubsystem>();
		if (!TestNotNull(TEXT("UWorld should auto-instantiate ULevelLifecycleSubsystem"), Subsystem))
		{
			return false;
		}

		TestEqual(TEXT("FinalMapName should default to NAME_None"), Subsystem->FinalMapName, FName(NAME_None));

		ULevelLifecycleTestListener* Listener = NewObject<ULevelLifecycleTestListener>();
		Subsystem->OnRunComplete.AddDynamic(Listener, &ULevelLifecycleTestListener::HandleRunComplete);

		Subsystem->OnWorldBeginPlay(*World);

		AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), Enemy))
		{
			return false;
		}
		Enemy->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
		Enemy->TickCheckDetection(ZeroDistanceLocation); // Alert -> Attack
		Enemy->ReceiveControl(EAbilitySlot::Stun);        // -> Controlled
		Enemy->TransitionToBanked();                      // -> Banked

		Subsystem->RefreshLevelClearState();
		TestEqual(TEXT("OnRunComplete must not fire with the default (NAME_None) FinalMapName"),
			Listener->RunCompleteCallCount, 0);
	}

	// --- (j): Tick() must still drive RefreshLevelClearState()/RefreshBossCheckpointState()
	// through the new one-shot Tick() log guard added by issue #234 - this is the actual
	// per-frame entry point that fires in real PIE/packaged play, not just its two
	// direct-call halves the rest of this file exercises.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}

		ULevelLifecycleSubsystem* Subsystem = World->GetSubsystem<ULevelLifecycleSubsystem>();
		if (!TestNotNull(TEXT("UWorld should auto-instantiate ULevelLifecycleSubsystem"), Subsystem))
		{
			return false;
		}

		ULevelLifecycleTestListener* Listener = NewObject<ULevelLifecycleTestListener>();
		Subsystem->OnLevelClear.AddDynamic(Listener, &ULevelLifecycleTestListener::HandleLevelClear);

		Subsystem->OnWorldBeginPlay(*World);

		AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), Enemy))
		{
			return false;
		}
		Enemy->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
		Enemy->TickCheckDetection(ZeroDistanceLocation); // Alert -> Attack
		Enemy->ReceiveControl(EAbilitySlot::Stun);        // -> Controlled
		Enemy->TransitionToBanked();                      // -> Banked

		// Drive through the real Tick() entry point, not RefreshLevelClearState() directly -
		// this is what the bHasLoggedFirstTick guard sits in front of.
		Subsystem->Tick(0.016f);
		TestEqual(TEXT("OnLevelClear should fire via Tick() -> RefreshLevelClearState(), not just via direct RefreshLevelClearState() calls"),
			Listener->LevelClearCallCount, 1);

		// A second Tick() must not re-fire, and must not regress now that Tick() also
		// carries the one-shot bHasLoggedFirstTick branch alongside the pre-existing calls.
		Subsystem->Tick(0.016f);
		TestEqual(TEXT("A second Tick() call must not re-fire OnLevelClear"),
			Listener->LevelClearCallCount, 1);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
