#include "LevelLifecycleSubsystem.h"
#include "EnemyBase.h"
#include "WaveSpawnerComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"

void ULevelLifecycleSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void ULevelLifecycleSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (bHasFiredLevelBegin)
	{
		// The engine only invokes OnWorldBeginPlay() once per world through its normal
		// path, but this guard also covers direct/manual invocation (as the Automation
		// test does) - it's what keeps a second direct call from re-firing OnLevelBegin.
		return;
	}
	bHasFiredLevelBegin = true;
	OnLevelBegin.Broadcast(FName(*InWorld.GetMapName()));
}

void ULevelLifecycleSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	RefreshLevelClearState();
}

TStatId ULevelLifecycleSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULevelLifecycleSubsystem, STATGROUP_Tickables);
}

void ULevelLifecycleSubsystem::RefreshLevelClearState()
{
	if (!bHasFiredLevelBegin || bHasFiredLevelClear)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	int32 TotalEnemies = 0;
	int32 BankedEnemies = 0;
	for (TActorIterator<AEnemyBase> It(World); It; ++It)
	{
		++TotalEnemies;
		if (It->GetEnemyState() == EEnemyState::Banked)
		{
			++BankedEnemies;
		}
	}

	if (TotalEnemies == 0 || BankedEnemies != TotalEnemies)
	{
		return;
	}

	// TActorIterator<AActor> + GetComponents(), not a bare TObjectIterator<UWaveSpawnerComponent>:
	// the latter walks every spawner across every loaded UWorld, which would let another
	// concurrently-loaded Automation test world's spawner block this world's OnLevelClear.
	for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
	{
		TArray<UWaveSpawnerComponent*> Spawners;
		ActorIt->GetComponents<UWaveSpawnerComponent>(Spawners);
		for (UWaveSpawnerComponent* Spawner : Spawners)
		{
			if (Spawner->IsWaveTimerActive())
			{
				return;
			}
		}
	}

	bHasFiredLevelClear = true;
	OnLevelClear.Broadcast();
}
