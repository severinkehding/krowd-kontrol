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
	// Set before attempting the zone lookup, not after: if no APlaceholderTargetZoneActor
	// exists in the world yet on the first successful Stun cast, the beacon cue is
	// permanently skipped for the rest of the session rather than retried on a later
	// cast - accepted for now since target zones are expected to already exist
	// wherever Stun is castable; revisit if a level ever streams zones in after
	// gameplay starts.
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
