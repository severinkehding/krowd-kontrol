#pragma once

#include "CoreMinimal.h"
#include "AbilitySlot.h"
#include "AbilityCastAppliedTestListener.generated.h"

class AEnemyBase;

// Listener for UAbilityCastComponent::FOnAbilityCastApplied (issue #138). Dynamic
// multicast delegates (DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams) only bind to
// UFUNCTIONs via AddDynamic - no AddLambda - so counting/inspecting broadcasts in
// tests needs this rather than a capturing lambda. Mirrors GizmoBarkTestListener.h's
// shape, adapted to FOnAbilityCastApplied's signature.
UCLASS()
class UAbilityCastAppliedTestListener : public UObject
{
	GENERATED_BODY()

public:
	int32 CallCount = 0;
	EAbilitySlot LastAbility = EAbilitySlot::Stun;

	UPROPERTY()
	TObjectPtr<AEnemyBase> LastTarget = nullptr;

	UFUNCTION()
	void HandleAbilityCastApplied(EAbilitySlot Ability, AEnemyBase* TargetEnemy);
};
