#pragma once
#include "CoreMinimal.h"
#include "TrooperRayFiredTestListener.generated.h"

// Test-only listener for ATrooperEnemy::OnTrooperRayFired (issue #14). Dynamic
// multicast delegates (DECLARE_DYNAMIC_MULTICAST_DELEGATE) only bind to UFUNCTIONs
// via AddDynamic - no AddLambda - so counting broadcasts in
// KrowdKontrolTrooperEnemyTest.cpp needs this rather than a capturing lambda. Mirrors
// USniperShotFiredTestListener/UBomberExplodedTestListener. Used only by that test.
UCLASS()
class UTrooperRayFiredTestListener : public UObject
{
	GENERATED_BODY()

public:
	int32 CallCount = 0;

	UFUNCTION()
	void HandleTrooperRayFired();
};
