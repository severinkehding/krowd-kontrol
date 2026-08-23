#pragma once

#include "CoreMinimal.h"
#include "AbilitySlot.h"
#include "AbilityCooldownChangedTestListener.generated.h"

// Listener for UAbilityCooldownComponent::FOnAbilityCooldownChanged (issue #259).
// Dynamic multicast delegates (DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams) only bind
// to UFUNCTIONs via AddDynamic - no AddLambda - so counting/inspecting broadcasts in
// tests needs this rather than a capturing lambda. Mirrors
// AbilityLockoutChangedTestListener.h's shape, adapted to FOnAbilityCooldownChanged's
// identical (EAbilitySlot, bool) signature.
UCLASS()
class UAbilityCooldownChangedTestListener : public UObject
{
	GENERATED_BODY()

public:
	int32 TrueBroadcastCount = 0;
	int32 FalseBroadcastCount = 0;
	EAbilitySlot LastAbility = EAbilitySlot::Stun;

	UFUNCTION()
	void HandleAbilityCooldownChanged(EAbilitySlot Ability, bool bOnCooldown);
};
