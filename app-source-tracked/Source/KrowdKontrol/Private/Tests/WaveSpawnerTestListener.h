#pragma once

#include "CoreMinimal.h"
#include "WaveSpawnerTestListener.generated.h"

// Test-only listener for UWaveSpawnerComponent::OnWaveSpawned/OnAllWavesComplete.
// Dynamic multicast delegates (DECLARE_DYNAMIC_MULTICAST_DELEGATE) only bind to
// UFUNCTIONs via AddDynamic - no AddLambda - so counting broadcasts in
// KrowdKontrolWaveSpawnerComponentTest.cpp needs this rather than a capturing lambda.
// Used only by that test.
UCLASS()
class UWaveSpawnerTestListener : public UObject
{
	GENERATED_BODY()

public:
	int32 SpawnedWaveCount = 0;
	int32 LastSpawnedWaveIndex = -1;
	int32 CompleteCallCount = 0;

	UFUNCTION()
	void HandleWaveSpawned(int32 WaveIndex);

	UFUNCTION()
	void HandleAllWavesComplete();
};
