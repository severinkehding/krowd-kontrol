#include "AbilityLockoutChangedTestListener.h"

void UAbilityLockoutChangedTestListener::HandleAbilityLockoutChanged(EAbilitySlot Ability, bool bLocked)
{
	++CallCount;
	LastAbility = Ability;
	LastLocked = bLocked;
}
