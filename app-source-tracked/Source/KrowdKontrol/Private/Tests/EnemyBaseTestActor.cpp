#include "EnemyBaseTestActor.h"
#include "Components/PointLightComponent.h"

AEnemyBaseTestActor::AEnemyBaseTestActor()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootSceneComponent"));

	// Inline creation, not a shared AEnemyBase helper - matches every other type-tied
	// component's shape (AttackTellLightComponent, etc). See app-changelog/issue-19.md's
	// "Design note" for why a shared-helper version was tried and reverted (a
	// GC/test-lifetime issue, not a declaration-shape problem).
	EliteTrimLightComponent = CreateDefaultSubobject<UPointLightComponent>(TEXT("TestActorEliteTrimLightComponent"));
	EliteTrimLightComponent->SetupAttachment(RootComponent);
	EliteTrimLightComponent->SetLightColor(FLinearColor(0.1f, 1.0f, 0.15f));
	EliteTrimLightComponent->SetIntensity(0.0f); // off unless bIsElite
	EliteTrimLightComponent->SetAttenuationRadius(300.0f);
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
