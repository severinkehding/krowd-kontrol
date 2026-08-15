#include "PlayerEnergyComponent.h"

UPlayerEnergyComponent::UPlayerEnergyComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPlayerEnergyComponent::BeginPlay()
{
	Super::BeginPlay();
	// SafeMaxEnergy guards against a negative MaxEnergy (e.g. a bad editor value)
	// seeding CurrentEnergy negative - the same bug class SafeMaxDamagePerHit below
	// guards against for MaxDamagePerHit.
	const float SafeMaxEnergy = FMath::Max(0.0f, MaxEnergy);
	CurrentEnergy = SafeMaxEnergy;
}

float UPlayerEnergyComponent::ApplyContactDamage(float RawAmount, AActor* DamageSource)
{
	// DamageSource is unused for now - reserved for future combat-log/HUD attribution.

	// Clamping the lower bound to 0 (not just an upper clamp to MaxDamagePerHit) stops
	// a negative RawAmount from healing the player through this path - the only
	// legal way CurrentEnergy moves is downward, via this method. SafeMaxDamagePerHit
	// applies the same guard to MaxDamagePerHit itself: FMath::Clamp doesn't assume
	// Min <= Max, so a negative MaxDamagePerHit (e.g. a bad editor/DataTable value)
	// would otherwise flip this into a heal too. SafeMaxEnergy applies the identical
	// guard to MaxEnergy, used as the clamp's upper bound below.
	const float SafeMaxDamagePerHit = FMath::Max(0.0f, MaxDamagePerHit);
	const float SafeMaxEnergy = FMath::Max(0.0f, MaxEnergy);
	const float ClampedDamage = FMath::Clamp(RawAmount, 0.0f, SafeMaxDamagePerHit);
	const float PreviousEnergy = CurrentEnergy;
	CurrentEnergy = FMath::Clamp(CurrentEnergy - ClampedDamage, 0.0f, SafeMaxEnergy);

	if (CurrentEnergy != PreviousEnergy)
	{
		OnEnergyChanged.Broadcast(CurrentEnergy);
	}

	return PreviousEnergy - CurrentEnergy;
}
