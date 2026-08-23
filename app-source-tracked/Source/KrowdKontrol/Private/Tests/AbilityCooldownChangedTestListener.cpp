#include "AbilityCooldownChangedTestListener.h"

void UAbilityCooldownChangedTestListener::HandleAbilityCooldownChanged(EAbilitySlot Ability, bool bOnCooldown)
{
	LastAbility = Ability;
	bOnCooldown ? ++TrueBroadcastCount : ++FalseBroadcastCount;
}
