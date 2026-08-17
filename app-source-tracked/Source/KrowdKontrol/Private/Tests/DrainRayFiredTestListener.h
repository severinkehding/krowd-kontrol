#pragma once
#include "CoreMinimal.h"
#include "DrainRayFiredTestListener.generated.h"

// Test-only listener for ARunnerEnemy::OnRunnerDrainFired (issue #13). Dynamic
// multicast delegates (DECLARE_DYNAMIC_MULTICAST_DELEGATE) only bind to UFUNCTIONs
// via AddDynamic - no AddLambda - so counting broadcasts in
// KrowdKontrolRunnerEnemyTest.cpp needs this rather than a capturing lambda. Mirrors
// USniperShotFiredTestListener/UBomberExplodedTestListener. Used only by that test.
UCLASS()
class UDrainRayFiredTestListener : public UObject
{
	GENERATED_BODY()

public:
	int32 CallCount = 0;

	UFUNCTION()
	void HandleDrainRayFired();
};
