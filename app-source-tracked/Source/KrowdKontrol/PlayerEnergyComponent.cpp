#include "PlayerEnergyComponent.h"

UPlayerEnergyComponent::UPlayerEnergyComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPlayerEnergyComponent::BeginPlay()
{
	Super::BeginPlay();
	CurrentEnergy = MaxEnergy;
}

float UPlayerEnergyComponent::ApplyContactDamage(float RawAmount, AActor* DamageSource)
{
	// DamageSource is unused for now - reserved for future combat-log/HUD attribution.

	// Clamping the lower bound to 0 (not just an upper clamp to MaxDamagePerHit) stops
	// a negative RawAmount from healing the player through this path - the only
	// legal way CurrentEnergy moves is downward, via this method. SafeMaxDamagePerHit
	// applies the same guard to MaxDamagePerHit itself: FMath::Clamp doesn't assume
	// Min <= Max, so a negative MaxDamagePerHit (e.g. a bad editor/DataTable value)
	// would otherwise flip this into a heal too.
	const float SafeMaxDamagePerHit = FMath::Max(0.0f, MaxDamagePerHit);
	const float ClampedDamage = FMath::Clamp(RawAmount, 0.0f, SafeMaxDamagePerHit);
	const float PreviousEnergy = CurrentEnergy;
	CurrentEnergy = FMath::Clamp(CurrentEnergy - ClampedDamage, 0.0f, MaxEnergy);

	if (CurrentEnergy != PreviousEnergy)
	{
		OnEnergyChanged.Broadcast(CurrentEnergy);
	}

	return PreviousEnergy - CurrentEnergy;
}
