#pragma once
#include "CoreMinimal.h"
#include "SniperShotFiredTestListener.generated.h"

// Test-only listener for ASniperEnemy::OnSniperShotFired (issue #17). Dynamic
// multicast delegates (DECLARE_DYNAMIC_MULTICAST_DELEGATE) only bind to UFUNCTIONs
// via AddDynamic - no AddLambda - so counting broadcasts in
// KrowdKontrolSniperEnemyTest.cpp needs this rather than a capturing lambda. Mirrors
// UBossBankedTestListener/UEnemyBankedTestListener. Used only by that test.
UCLASS()
class USniperShotFiredTestListener : public UObject
{
	GENERATED_BODY()

public:
	int32 CallCount = 0;

	UFUNCTION()
	void HandleSniperShotFired();
};
