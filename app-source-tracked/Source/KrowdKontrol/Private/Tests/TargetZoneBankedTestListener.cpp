#include "TargetZoneBankedTestListener.h"

void UTargetZoneBankedTestListener::HandleActorBanked(AActor* BankedActor)
{
	++CallCount;
	LastBankedActor = BankedActor;
}
