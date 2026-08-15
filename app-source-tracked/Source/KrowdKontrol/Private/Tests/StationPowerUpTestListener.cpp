#include "StationPowerUpTestListener.h"

void UStationPowerUpTestListener::HandleLightEnabled(int32 LightIndex, AActor* LightActor)
{
	++LightEnabledCallCount;
	LastEnabledLightIndex = LightIndex;
	LastEnabledLightActor = LightActor;
}

void UStationPowerUpTestListener::HandleSequenceComplete()
{
	++SequenceCompleteCallCount;
}
