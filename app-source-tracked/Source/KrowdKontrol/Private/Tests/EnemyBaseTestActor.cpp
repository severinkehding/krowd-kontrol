#include "EnemyBaseTestActor.h"
#include "Components/PointLightComponent.h"

AEnemyBaseTestActor::AEnemyBaseTestActor()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootSceneComponent"));

	// Inline creation, not a shared AEnemyBase helper - see EnemyBase.h's
	// EliteTrimLightComponent comment for why a common-base CreateDefaultSubobject
	// helper corrupts UE's CDO instancing once called from multiple sibling classes.
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
