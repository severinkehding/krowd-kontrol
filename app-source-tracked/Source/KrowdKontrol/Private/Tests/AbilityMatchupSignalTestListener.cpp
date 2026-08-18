#include "AbilityMatchupSignalTestListener.h"
#include "EnemyBase.h"

void UAbilityMatchupSignalTestListener::HandleAbilityMatchupSignal(EAbilitySlot Ability, AEnemyBase* TargetEnemy, bool bWasColourMatched)
{
	++CallCount;
	LastAbility = Ability;
	LastTarget = TargetEnemy;
	LastWasColourMatched = bWasColourMatched;
}
