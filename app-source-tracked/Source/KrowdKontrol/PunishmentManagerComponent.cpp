#include "PunishmentManagerComponent.h"

UPunishmentManagerComponent::UPunishmentManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPunishmentManagerComponent::HandleEnergyChanged(float NewEnergy)
{
	// NewEnergy is unused - this is a pure re-broadcast, the value itself is not
	// part of the punishment-trigger signal (see class comment on OnPunishmentTriggered).
	OnPunishmentTriggered.Broadcast();
}
