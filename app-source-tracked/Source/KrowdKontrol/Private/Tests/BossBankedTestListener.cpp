#include "BossBankedTestListener.h"
#include "BossBase.h"

void UBossBankedTestListener::HandleBossBanked()
{
	++CallCount;

	if (ActorToReenter)
	{
		ActorToReenter->TransitionToBanked();
	}
}
