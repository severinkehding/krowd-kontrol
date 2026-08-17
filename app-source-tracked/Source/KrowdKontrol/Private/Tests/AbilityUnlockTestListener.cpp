#include "AbilityUnlockTestListener.h"

void UAbilityUnlockTestListener::HandleAbilityUnlocked(EAbilitySlot Ability)
{
	UnlockedOrder.Add(Ability);
}
