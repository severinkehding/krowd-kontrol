#include "TargetZone.h"
#include "Components/BoxComponent.h"
#include "EnemyTypeIndicatorComponent.h"
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
	// KNOWN GAP: this fires once per overlapping component, not once per actor. An
	// IHerdable actor with more than one collision component overlapping the zone
	// simultaneously would broadcast OnActorBanked once per component instead of once
	// per actor. No current fixture has more than one collision component, so this is
	// untested; revisit once a production IHerdable actor with multiple physical
	// components exists.
	ZoneCollisionComponent->OnComponentBeginOverlap.AddUniqueDynamic(this, &ATargetZone::HandleZoneOverlap);
}

float ATargetZone::GetBankingRadiusUnits() const
{
	const FVector Extent = ZoneCollisionComponent->GetScaledBoxExtent();
	return FMath::Max(Extent.X, Extent.Y);
}

void ATargetZone::HandleZoneOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	IHerdable* Herdable = Cast<IHerdable>(OtherActor);
	if (!Herdable || !Herdable->IsControlled())
	{
		return;
	}
	// Type-keyed acceptance (operator ruling 2026-08-22, PR #212 review): the pen
	// cares what enemy TYPE arrives, never which ability controls it. The previous
	// colour gate (controlling-ability colour vs counter-ability colour) made
	// Stun - colour-neutral, the only Level 1 ability - bankable nowhere, i.e.
	// Level 1 unwinnable; MISSION `02` specifies colour-matching as a bonus
	// (control duration), not the win-condition gate. bAcceptAnyEnemyType's
	// default (true) preserves issue #80's documented unconfigured-matches
	// behaviour for zones nothing has typed.
	if (!bAcceptAnyEnemyType)
	{
		const UEnemyTypeIndicatorComponent* TypeIndicator =
			OtherActor->FindComponentByClass<UEnemyTypeIndicatorComponent>();
		if (!TypeIndicator || TypeIndicator->EnemyType != ZoneEnemyType)
		{
			return;
		}
	}
	OnActorBanked.Broadcast(OtherActor);
}
