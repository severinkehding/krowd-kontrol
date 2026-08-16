#include "EnemyBaseTestActor.h"

void AEnemyBaseTestActor::OnControlledEntry(EAbilitySlot Ability)
{
	++ControlledEntryCallCount;
	LastControlledEntryAbility = Ability;
}

void AEnemyBaseTestActor::OnAttackEntry()
{
	++AttackEntryCallCount;
}
