#pragma once

#include "CoreMinimal.h"
#include "AbilitySlot.h"
#include "AbilityLockoutChangedTestListener.generated.h"

// Listener for UAbilityLockoutComponent::FOnAbilityLockoutChanged (issue #178).
// Dynamic multicast delegates (DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams) only bind
// to UFUNCTIONs via AddDynamic - no AddLambda - so counting/inspecting broadcasts in
// tests needs this rather than a capturing lambda. Mirrors
// AbilityCastAppliedTestListener.h's shape, adapted to FOnAbilityLockoutChanged's
// (EAbilitySlot, bool) signature.
UCLASS()
class UAbilityLockoutChangedTestListener : public UObject
{
	GENERATED_BODY()

public:
	int32 CallCount = 0;
	EAbilitySlot LastAbility = EAbilitySlot::Stun;
	bool LastLocked = false;

	UFUNCTION()
	void HandleAbilityLockoutChanged(EAbilitySlot Ability, bool bLocked);
};
