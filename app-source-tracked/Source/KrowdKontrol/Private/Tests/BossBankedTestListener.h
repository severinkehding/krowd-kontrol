#pragma once
#include "CoreMinimal.h"
#include "BossBankedTestListener.generated.h"

// Test-only listener for ABossBase::OnBossBanked (issue #44). Dynamic multicast
// delegates (DECLARE_DYNAMIC_MULTICAST_DELEGATE) only bind to UFUNCTIONs via
// AddDynamic - no AddLambda - so counting broadcasts in
// KrowdKontrolBossBaseTest.cpp needs this rather than a capturing lambda. Used
// only by that test.
UCLASS()
class UBossBankedTestListener : public UObject
{
	GENERATED_BODY()

public:
	int32 CallCount = 0;

	UFUNCTION()
	void HandleBossBanked();
};
