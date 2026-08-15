#include "StationPowerUpComponent.h"

UStationPowerUpComponent::UStationPowerUpComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UStationPowerUpComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeSequence();
}

void UStationPowerUpComponent::InitializeSequence()
{
	if (bHasInitializedSequence)
	{
		return;
	}
	bHasInitializedSequence = true;

	if (OrderedLights.Num() == 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UStationPowerUpComponent: OrderedLights is empty on '%s' - the power-up sequence will never progress or complete."),
			*GetNameSafe(GetOwner()));
	}

	NextLightIndex = 0;
	bSequenceCompleteFired = false;

	for (AActor* Light : OrderedLights)
	{
		if (Light)
		{
			Light->SetActorHiddenInGame(true);
		}
	}
}

void UStationPowerUpComponent::NotifyPowerUpStageTriggered()
{
	if (NextLightIndex >= OrderedLights.Num())
	{
		return;
	}

	AActor* Light = OrderedLights[NextLightIndex];
	if (Light)
	{
		Light->SetActorHiddenInGame(false);
	}

	OnLightEnabled.Broadcast(NextLightIndex, Light);
	++NextLightIndex;

	if (!bSequenceCompleteFired && NextLightIndex >= OrderedLights.Num())
	{
		bSequenceCompleteFired = true;
		OnPowerUpSequenceComplete.Broadcast();
	}
}
