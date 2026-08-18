#include "FirstStunBeaconComponent.h"
#include "PlaceholderTargetZoneActor.h"
#include "EnemyBase.h"
#include "EngineUtils.h"
#include "Engine/World.h"

UFirstStunBeaconComponent::UFirstStunBeaconComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UFirstStunBeaconComponent::HandleAbilityCastApplied(EAbilitySlot Ability, AEnemyBase* TargetEnemy)
{
	if (Ability != EAbilitySlot::Stun)
	{
		return;
	}

	if (bHasTriggeredBeacon)
	{
		return;
	}
	// Set before attempting the zone lookup - unlike
	// UGizmoFirstContactComponent::InitializeFirstContactBark's retryable guard (which
	// exists to survive a resolvable-later missing subsystem), GetWorld() is always
	// valid once this component's owner is in a live World, so there is no legitimate
	// retry case here.
	bHasTriggeredBeacon = true;

	APlaceholderTargetZoneActor* NearestZone = FindNearestTargetZone();
	if (!NearestZone)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UFirstStunBeaconComponent: no APlaceholderTargetZoneActor found in the world - ")
			TEXT("the first-Stun beacon cue cannot be shown."));
		return;
	}

	NearestZone->IntensifyBeacon();
}

APlaceholderTargetZoneActor* UFirstStunBeaconComponent::FindNearestTargetZone() const
{
	const AActor* Owner = GetOwner();
	if (!Owner || !GetWorld())
	{
		return nullptr;
	}

	const FVector OwnerLocation = Owner->GetActorLocation();
	APlaceholderTargetZoneActor* Nearest = nullptr;
	float NearestDistSquared = TNumericLimits<float>::Max();
	for (TActorIterator<APlaceholderTargetZoneActor> It(GetWorld()); It; ++It)
	{
		const float DistSquared = FVector::DistSquared(It->GetActorLocation(), OwnerLocation);
		if (DistSquared >= NearestDistSquared)
		{
			continue;
		}
		Nearest = *It;
		NearestDistSquared = DistSquared;
	}
	return Nearest;
}
