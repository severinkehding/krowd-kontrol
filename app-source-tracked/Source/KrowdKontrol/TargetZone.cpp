#include "TargetZone.h"
#include "Components/BoxComponent.h"
#include "Herdable.h"

ATargetZone::ATargetZone()
{
	PrimaryActorTick.bCanEverTick = false;

	ZoneCollisionComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("ZoneCollisionComponent"));
	RootComponent = ZoneCollisionComponent;
	// Placeholder size, matches this codebase's placeholder-first convention - not a
	// locked design value.
	ZoneCollisionComponent->SetBoxExtent(FVector(150.f, 150.f, 100.f));
	// No gameplay collision channel is defined yet to depend on instead - this stock
	// profile detects any IHerdable actor regardless of that actor's own collision
	// setup.
	ZoneCollisionComponent->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	ZoneCollisionComponent->SetGenerateOverlapEvents(true);
	ZoneCollisionComponent->OnComponentBeginOverlap.AddUniqueDynamic(this, &ATargetZone::HandleZoneOverlap);
}

void ATargetZone::HandleZoneOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	IHerdable* Herdable = Cast<IHerdable>(OtherActor);
	if (!Herdable || !Herdable->IsControlled())
	{
		return;
	}
	// ZoneColourTag == NAME_None on both sides (an unconfigured zone and an
	// unconfigured IHerdable actor) matching by default is correct per the issue -
	// colour-matching is a hard requirement this issue does not redefine.
	if (Herdable->GetHerdColourTag() != ZoneColourTag)
	{
		return;
	}
	OnActorBanked.Broadcast(OtherActor);
}
