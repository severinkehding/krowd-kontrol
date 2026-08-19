// Confirms ULevelLifecycleSubsystem (issue #169, PRD "Run Lifecycle & Progression
// Signals" REQ-1) fires OnLevelBegin exactly once at world begin-play (carrying the
// map name) and OnLevelClear exactly once, after OnLevelBegin, the moment every
// AEnemyBase in the world (including ones spawned later by a UWaveSpawnerComponent)
// is Banked and no spawner has a pending wave - and that it does NOT fire while zero
// enemies have ever spawned, or while any spawner's IsWaveTimerActive() is true even
// with every currently-spawned enemy already Banked.
//
// RefreshLevelClearState()/OnWorldBeginPlay() are called directly (never via a real
// per-frame Tick() loop or World->BeginPlay()) for the same synchronous-determinism
// reasons KrowdKontrolMusicSubsystemTest.cpp documents - this repo's harness never
// drives either.
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

	// --- (c): wave-spawner blocks clear even with every current enemy Banked, and a
	// wave-spawned enemy must itself reach Banked before OnLevelClear fires. A second,
	// fresh CreateNewMap() world is needed because (a)/(b)'s subsystem instance already
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

		ULevelLifecycleTestListener* Listener = NewObject<ULevelLifecycleTestListener>();
		Subsystem->OnLevelClear.AddDynamic(Listener, &ULevelLifecycleTestListener::HandleLevelClear);

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

		Spawner->TriggerNextWave(); // spawns the wave's enemy immediately, clears the pending timer
		Subsystem->RefreshLevelClearState();
		TestEqual(TEXT("OnLevelClear must not fire while the newly wave-spawned enemy is not yet Banked"),
			Listener->LevelClearCallCount, 0);

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
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
