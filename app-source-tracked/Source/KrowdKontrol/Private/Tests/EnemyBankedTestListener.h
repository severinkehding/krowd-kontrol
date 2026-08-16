#pragma once
#include "CoreMinimal.h"
#include "EnemyBankedTestListener.generated.h"

// Test-only listener for AEnemyBase::OnEnemyBanked (issue #12). Dynamic multicast
// delegates (DECLARE_DYNAMIC_MULTICAST_DELEGATE) only bind to UFUNCTIONs via
// AddDynamic - no AddLambda - so counting broadcasts in
// KrowdKontrolEnemyBaseTest.cpp needs this rather than a capturing lambda. Mirrors
// UBossBankedTestListener. Used only by that test.
UCLASS()
class UEnemyBankedTestListener : public UObject
{
	GENERATED_BODY()

public:
	int32 CallCount = 0;

	UFUNCTION()
	void HandleEnemyBanked();
};
