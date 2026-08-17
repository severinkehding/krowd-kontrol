#pragma once

#include "CoreMinimal.h"
#include "EnemyBase.h"
#include "Components/SceneComponent.h"
#include "EnemyBaseTestActor.generated.h"

// Minimal test-only subclass of AEnemyBase (issue #12) overriding the
// OnControlledEntry/OnAttackEntry hooks to count invocations (and record the last
// ability seen by OnControlledEntry), so KrowdKontrolEnemyBaseTest.cpp can confirm
// the hooks actually fire on real transitions. Used only by that test. AEnemyBase is
// UCLASS(Abstract), so a test-only concrete subclass is required to NewObject<>() it
// at all.
UCLASS()
class AEnemyBaseTestActor : public AEnemyBase
{
	GENERATED_BODY()

public:
	// A plain RootComponent (issue #122) so SetActorLocation in TickChaseMovement
	// tests has somewhere to write - AEnemyBase itself has no visual/mesh component,
	// unlike ABomberEnemy/ASniperEnemy which set RootComponent in their own
	// constructors.
	AEnemyBaseTestActor();

	int32 ControlledEntryCallCount = 0;
	int32 AttackEntryCallCount = 0;
	EAbilitySlot LastControlledEntryAbility = EAbilitySlot::Stun;

protected:
	virtual void OnControlledEntry(EAbilitySlot Ability) override;
	virtual void OnAttackEntry() override;
};
