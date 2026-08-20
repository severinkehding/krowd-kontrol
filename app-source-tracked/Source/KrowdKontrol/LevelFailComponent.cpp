#include "LevelFailComponent.h"

ULevelFailComponent::ULevelFailComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void ULevelFailComponent::HandleEnergyChanged(float NewEnergy)
{
	if (bHasFired || NewEnergy > 0.0f)
	{
		return;
	}
	bHasFired = true;
	OnLevelFailed.Broadcast();
}
