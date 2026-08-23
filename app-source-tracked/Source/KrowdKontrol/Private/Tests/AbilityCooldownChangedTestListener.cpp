#include "AbilityCooldownChangedTestListener.h"

void UAbilityCooldownChangedTestListener::HandleAbilityCooldownChanged(EAbilitySlot Ability, bool bOnCooldown)
{
	LastAbility = Ability;
	if (bOnCooldown)
	{
		++TrueBroadcastCount;
	}
	else
	{
		++FalseBroadcastCount;
	}
}
