#include "EnemyBaseTestActor.h"

AEnemyBaseTestActor::AEnemyBaseTestActor()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootSceneComponent"));
}

void AEnemyBaseTestActor::OnControlledEntry(EAbilitySlot Ability)
{
	++ControlledEntryCallCount;
	LastControlledEntryAbility = Ability;
}

void AEnemyBaseTestActor::OnAttackEntry()
{
	++AttackEntryCallCount;
}
