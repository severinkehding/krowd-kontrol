#pragma once

#include "CoreMinimal.h"
#include "AbilitySlot.h"
#include "AbilityUnlockTestListener.generated.h"

// Test-only listener for UAbilityUnlockComponent::OnAbilityUnlocked. Dynamic
// multicast delegates (DECLARE_DYNAMIC_MULTICAST_DELEGATE) only bind to UFUNCTIONs via
// AddDynamic - no AddLambda - so recording broadcasts in
// KrowdKontrolAbilityUnlockSequenceTest.cpp needs this rather than a capturing lambda.
// Used only by that test.
UCLASS()
class UAbilityUnlockTestListener : public UObject
{
	GENERATED_BODY()

public:
	TArray<EAbilitySlot> UnlockedOrder;

	UFUNCTION()
	void HandleAbilityUnlocked(EAbilitySlot Ability);
};
