// Confirms UWaveSpawnerComponent (issue #21) spawns a configured sequence of enemy
// waves with the correct count/type, defers nonzero-delay waves until TriggerNextWave()
// is called, is safely idempotent past the last wave, and warns (without crashing) on
// the two misconfiguration cases this codebase's other spawner components already
// established a "warn, don't block" precedent for: an empty Waves array, and an unset
// EnemyClass on a single wave entry.
//
// Needs a real UWorld to spawn into (SpawnWave() calls GetWorld()->SpawnActor), so uses
// FAutomationEditorCommonUtils::CreateNewMap() the same way
// KrowdKontrolRoomEnemyBudgetControllerTest.cpp does. All waves in this test use
// DelaySeconds of either 0.0 (resolves synchronously, no timer) or a value large enough
// that only TriggerNextWave() - never a real elapsed tick - fires it, so no PIE tick
// loop is needed here either.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "WaveSpawnerComponent.h"
#include "WaveSpawnerTestListener.h"
#include "PlaceholderCubeActor.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolWaveSpawnerComponentTest,
	"KrowdKontrol.Unit.WaveSpawnerComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolWaveSpawnerComponentTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	AActor* OwnerActor = World->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("Owner actor should spawn into the test World"), OwnerActor))
	{
		return false;
	}

	// (1) Correct enemy count and types per wave: two all-zero-delay waves resolve
	// synchronously within one StartWaves() call.
	{
		UWaveSpawnerComponent* Spawner = NewObject<UWaveSpawnerComponent>(OwnerActor);
		if (!TestNotNull(TEXT("UWaveSpawnerComponent should construct"), Spawner))
		{
			return false;
		}
		Spawner->RegisterComponent();

		FWaveEntry WaveA;
		WaveA.EnemyType = EEnemyType::RU_NNR;
		WaveA.EnemyClass = APlaceholderCubeActor::StaticClass();
		WaveA.Count = 2;
		WaveA.DelaySeconds = 0.0f;

		FWaveEntry WaveB;
		WaveB.EnemyType = EEnemyType::SN_1PR;
		WaveB.EnemyClass = APlaceholderCubeActor::StaticClass();
		WaveB.Count = 3;
		WaveB.DelaySeconds = 0.0f;

		Spawner->Waves = { WaveA, WaveB };

		UWaveSpawnerTestListener* Listener = NewObject<UWaveSpawnerTestListener>();
		Spawner->OnWaveSpawned.AddDynamic(Listener, &UWaveSpawnerTestListener::HandleWaveSpawned);
		Spawner->OnAllWavesComplete.AddDynamic(Listener, &UWaveSpawnerTestListener::HandleAllWavesComplete);

		Spawner->StartWaves();

		TestEqual(TEXT("Both zero-delay waves should spawn their full enemy count"),
			Spawner->GetSpawnedActors().Num(), 5);
		TestEqual(TEXT("OnWaveSpawned should fire once per wave"),
			Listener->SpawnedWaveCount, 2);
		TestEqual(TEXT("OnWaveSpawned's last call should report the final wave index"),
			Listener->LastSpawnedWaveIndex, 1);
		TestEqual(TEXT("OnAllWavesComplete should fire exactly once"),
			Listener->CompleteCallCount, 1);
	}

	// (2) A nonzero delay defers a wave until explicitly triggered; (3) TriggerNextWave()
	// past the end is a safe no-op.
	{
		UWaveSpawnerComponent* Spawner = NewObject<UWaveSpawnerComponent>(OwnerActor);
		if (!TestNotNull(TEXT("UWaveSpawnerComponent should construct"), Spawner))
		{
			return false;
		}
		Spawner->RegisterComponent();

		FWaveEntry WaveA;
		WaveA.EnemyClass = APlaceholderCubeActor::StaticClass();
		WaveA.Count = 1;
		WaveA.DelaySeconds = 0.0f;

		FWaveEntry WaveB;
		WaveB.EnemyClass = APlaceholderCubeActor::StaticClass();
		WaveB.Count = 1;
		WaveB.DelaySeconds = 5.0f;

		Spawner->Waves = { WaveA, WaveB };

		UWaveSpawnerTestListener* Listener = NewObject<UWaveSpawnerTestListener>();
		Spawner->OnAllWavesComplete.AddDynamic(Listener, &UWaveSpawnerTestListener::HandleAllWavesComplete);

		Spawner->StartWaves();

		TestEqual(TEXT("Wave 0 (zero delay) should spawn immediately"),
			Spawner->GetSpawnedActors().Num(), 1);
		TestEqual(TEXT("NextWaveIndex should advance to the deferred wave"),
			Spawner->GetNextWaveIndex(), 1);
		TestEqual(TEXT("Deferred wave should not have completed the sequence yet"),
			Listener->CompleteCallCount, 0);

		Spawner->TriggerNextWave();

		TestEqual(TEXT("TriggerNextWave() should spawn the deferred wave immediately"),
			Spawner->GetSpawnedActors().Num(), 2);
		TestEqual(TEXT("OnAllWavesComplete should fire once both waves are done"),
			Listener->CompleteCallCount, 1);

		// (3) Triggering again past the last wave must be a safe no-op.
		Spawner->TriggerNextWave();

		TestEqual(TEXT("TriggerNextWave() past the end should not spawn anything else"),
			Spawner->GetSpawnedActors().Num(), 2);
	}

	// (4) Empty Waves warns and completes immediately, without spawning.
	{
		UWaveSpawnerComponent* Spawner = NewObject<UWaveSpawnerComponent>(OwnerActor);
		if (!TestNotNull(TEXT("UWaveSpawnerComponent should construct"), Spawner))
		{
			return false;
		}
		Spawner->RegisterComponent();

		UWaveSpawnerTestListener* Listener = NewObject<UWaveSpawnerTestListener>();
		Spawner->OnAllWavesComplete.AddDynamic(Listener, &UWaveSpawnerTestListener::HandleAllWavesComplete);

		AddExpectedError(TEXT("Waves is empty"), EAutomationExpectedErrorFlags::Contains, 1);
		Spawner->StartWaves();

		TestEqual(TEXT("Empty Waves should spawn nothing"),
			Spawner->GetSpawnedActors().Num(), 0);
		TestEqual(TEXT("Empty Waves should still complete the sequence immediately"),
			Listener->CompleteCallCount, 1);
	}

	// (5) An unset EnemyClass on one wave entry warns, skips spawning that wave, but
	// still advances.
	{
		UWaveSpawnerComponent* Spawner = NewObject<UWaveSpawnerComponent>(OwnerActor);
		if (!TestNotNull(TEXT("UWaveSpawnerComponent should construct"), Spawner))
		{
			return false;
		}
		Spawner->RegisterComponent();

		FWaveEntry UnsetWave;
		UnsetWave.Count = 2;
		UnsetWave.DelaySeconds = 0.0f;
		// EnemyClass intentionally left unset.

		FWaveEntry WaveAfter;
		WaveAfter.EnemyClass = APlaceholderCubeActor::StaticClass();
		WaveAfter.Count = 1;
		WaveAfter.DelaySeconds = 0.0f;

		Spawner->Waves = { UnsetWave, WaveAfter };

		UWaveSpawnerTestListener* Listener = NewObject<UWaveSpawnerTestListener>();
		Spawner->OnAllWavesComplete.AddDynamic(Listener, &UWaveSpawnerTestListener::HandleAllWavesComplete);

		AddExpectedError(TEXT("EnemyClass is unset"), EAutomationExpectedErrorFlags::Contains, 1);
		Spawner->StartWaves();

		TestEqual(TEXT("Unset EnemyClass wave should contribute no actors"),
			Spawner->GetSpawnedActors().Num(), 1);
		TestEqual(TEXT("Sequence should still reach OnAllWavesComplete"),
			Listener->CompleteCallCount, 1);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
