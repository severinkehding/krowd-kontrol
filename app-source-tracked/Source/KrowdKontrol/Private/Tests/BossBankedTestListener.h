#pragma once
#include "CoreMinimal.h"
#include "BossBankedTestListener.generated.h"

class ABossBase;

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

	// When set, HandleBossBanked calls ActorToReenter->TransitionToBanked() from
	// inside the broadcast - lets a test prove TransitionToBanked()'s
	// flip-before-broadcast ordering is re-entrancy safe. Mirrors
	// UReentrantTerminalListener's ActorToReenter mechanism
	// (ReentrantTerminalListener.h).
	UPROPERTY()
	TObjectPtr<ABossBase> ActorToReenter = nullptr;

	UFUNCTION()
	void HandleBossBanked();
};
