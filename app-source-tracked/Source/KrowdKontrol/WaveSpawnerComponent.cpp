#include "WaveSpawnerComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

UWaveSpawnerComponent::UWaveSpawnerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWaveSpawnerComponent::StartWaves()
{
	if (bHasStarted)
	{
		return;
	}
	bHasStarted = true;

	if (Waves.Num() == 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UWaveSpawnerComponent: Waves is empty on '%s' - no waves will spawn."),
			*GetNameSafe(GetOwner()));

		bAllWavesCompleteFired = true;
		OnAllWavesComplete.Broadcast();
		return;
	}

	ScheduleWave(0);
}

void UWaveSpawnerComponent::TriggerNextWave()
{
	if (NextWaveIndex >= Waves.Num())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WaveTimerHandle);
	}

	SpawnWave(NextWaveIndex);
}

void UWaveSpawnerComponent::ScheduleWave(int32 WaveIndex)
{
	if (!Waves.IsValidIndex(WaveIndex))
	{
		return;
	}

	if (Waves[WaveIndex].DelaySeconds <= 0.0f)
	{
		SpawnWave(WaveIndex);
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			WaveTimerHandle,
			FTimerDelegate::CreateUObject(this, &UWaveSpawnerComponent::SpawnWave, WaveIndex),
			Waves[WaveIndex].DelaySeconds,
			false);
	}
}

void UWaveSpawnerComponent::SpawnWave(int32 WaveIndex)
{
	const FWaveEntry& Entry = Waves[WaveIndex];

	if (Entry.EnemyClass)
	{
		if (UWorld* World = GetWorld())
		{
			for (int32 i = 0; i < Entry.Count; ++i)
			{
				if (AActor* SpawnedActor = World->SpawnActor<AActor>(Entry.EnemyClass))
				{
					SpawnedActors.Add(SpawnedActor);
				}
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UWaveSpawnerComponent: Waves[%d].EnemyClass is unset on '%s' - wave will advance with no actors spawned."),
			WaveIndex, *GetNameSafe(GetOwner()));
	}

	OnWaveSpawned.Broadcast(WaveIndex);
	NextWaveIndex = WaveIndex + 1;

	if (NextWaveIndex >= Waves.Num())
	{
		if (!bAllWavesCompleteFired)
		{
			bAllWavesCompleteFired = true;
			OnAllWavesComplete.Broadcast();
		}
	}
	else
	{
		ScheduleWave(NextWaveIndex);
	}
}

void UWaveSpawnerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WaveTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}
