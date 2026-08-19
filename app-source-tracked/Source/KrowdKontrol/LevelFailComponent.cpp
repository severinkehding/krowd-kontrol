#include "LevelFailComponent.h"

ULevelFailComponent::ULevelFailComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void ULevelFailComponent::HandleEnergyChanged(float NewEnergy)
{
	if (NewEnergy <= 0.0f)
	{
		OnLevelFailed.Broadcast();
	}
}
