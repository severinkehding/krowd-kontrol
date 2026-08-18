#pragma once

#include "CoreMinimal.h"
#include "AbilitySlot.h"
#include "AbilityMatchupSignalTestListener.generated.h"

class AEnemyBase;

// Listener for UAbilityMatchupSignalComponent::FOnAbilityMatchupSignal (issue #37).
// Dynamic multicast delegates (DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams) only
// bind to UFUNCTIONs via AddDynamic - no AddLambda - so counting/inspecting
// broadcasts in tests needs this rather than a capturing lambda. Mirrors
// AbilityCastAppliedTestListener.h's shape, extended for the third bool param.
UCLASS()
class UAbilityMatchupSignalTestListener : public UObject
{
	GENERATED_BODY()

public:
	int32 CallCount = 0;
	EAbilitySlot LastAbility = EAbilitySlot::Stun;
	bool LastWasColourMatched = false;

	UPROPERTY()
	TObjectPtr<AEnemyBase> LastTarget = nullptr;

	UFUNCTION()
	void HandleAbilityMatchupSignal(EAbilitySlot Ability, AEnemyBase* TargetEnemy, bool bWasColourMatched);
};
