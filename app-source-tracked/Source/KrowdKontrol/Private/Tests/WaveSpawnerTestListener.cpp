#include "WaveSpawnerTestListener.h"

void UWaveSpawnerTestListener::HandleWaveSpawned(int32 WaveIndex)
{
	++SpawnedWaveCount;
	LastSpawnedWaveIndex = WaveIndex;
}

void UWaveSpawnerTestListener::HandleAllWavesComplete()
{
	++CompleteCallCount;
}
