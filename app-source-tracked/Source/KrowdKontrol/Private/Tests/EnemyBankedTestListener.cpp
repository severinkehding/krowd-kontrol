#include "EnemyBankedTestListener.h"
#include "EnemyBase.h"

void UEnemyBankedTestListener::HandleEnemyBanked()
{
	++CallCount;

	if (ActorToReenter)
	{
		ActorToReenter->TransitionToBanked();
	}
}
