#include "TargetZoneTestActor.h"
#include "Components/BoxComponent.h"

ATargetZoneTestActor::ATargetZoneTestActor()
{
	PrimaryActorTick.bCanEverTick = false;

	CollisionComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionComponent"));
	RootComponent = CollisionComponent;
	CollisionComponent->SetBoxExtent(FVector(50.f, 50.f, 50.f));
	CollisionComponent->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	CollisionComponent->SetGenerateOverlapEvents(true);
}

void ATargetZoneTestActor::SetControlled(bool bNewControlled)
{
	bIsControlled = bNewControlled;
}

void ATargetZoneTestActor::SetHerdColourTag(FName NewColourTag)
{
	HerdColourTag = NewColourTag;
}

bool ATargetZoneTestActor::IsControlled() const
{
	return bIsControlled;
}

FName ATargetZoneTestActor::GetHerdColourTag() const
{
	return HerdColourTag;
}
