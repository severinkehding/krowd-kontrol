#include "AbilityCastAppliedTestListener.h"
#include "EnemyBase.h"

void UAbilityCastAppliedTestListener::HandleAbilityCastApplied(EAbilitySlot Ability, AEnemyBase* TargetEnemy)
{
	++CallCount;
	LastAbility = Ability;
	LastTarget = TargetEnemy;
}
