#include "RoomEnemyBudgetController.h"
#include "Engine/World.h"

URoomEnemyBudgetController::URoomEnemyBudgetController()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URoomEnemyBudgetController::BeginPlay()
{
	Super::BeginPlay();
	InitializeRoom();
}

void URoomEnemyBudgetController::InitializeRoom()
{
	if (bHasInitializedRoom)
	{
		return;
	}
	bHasInitializedRoom = true;

	RemainingBudget = TotalRoomBudget;
	ActiveEnemyCount = 0;

	while (ActiveEnemyCount < MaxConcurrentDensity && RemainingBudget > 0)
	{
		SpawnEnemy();
	}

	CheckForRoomCleared();
}

void URoomEnemyBudgetController::NotifyEnemyBanked()
{
	ActiveEnemyCount = FMath::Max(0, ActiveEnemyCount - 1);

	if (RemainingBudget > 0 && ActiveEnemyCount < MaxConcurrentDensity)
	{
		SpawnEnemy();
	}

	CheckForRoomCleared();
}

void URoomEnemyBudgetController::SpawnEnemy()
{
	if (UWorld* World = GetWorld())
	{
		if (EnemyClassToSpawn)
		{
			World->SpawnActor<AActor>(EnemyClassToSpawn);
		}
	}

	// Counters advance even if nothing actually spawned (unset EnemyClassToSpawn, or
	// no World yet) - InitializeRoom()'s fill loop depends on RemainingBudget always
	// decrementing here, or it would spin forever whenever EnemyClassToSpawn is unset.
	--RemainingBudget;
	++ActiveEnemyCount;
}

void URoomEnemyBudgetController::CheckForRoomCleared()
{
	if (!bRoomClearedFired && RemainingBudget <= 0 && ActiveEnemyCount <= 0)
	{
		bRoomClearedFired = true;
		OnRoomCleared.Broadcast();
	}
}
