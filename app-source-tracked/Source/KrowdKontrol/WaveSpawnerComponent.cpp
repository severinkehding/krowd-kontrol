#include "WaveSpawnerComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "EnemyBase.h"
#include "EliteEligibility.h"

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
	if (!bHasStarted)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UWaveSpawnerComponent: TriggerNextWave() called on '%s' before StartWaves() - ignoring."),
			*GetNameSafe(GetOwner()));
		return;
	}

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

					if (Entry.bIsElite && EliteEligibility::IsEligibleAtLevel(LevelIndex))
					{
						if (AEnemyBase* SpawnedEnemy = Cast<AEnemyBase>(SpawnedActor))
						{
							SpawnedEnemy->SetIsElite(true);
						}
						else
						{
							UE_LOG(LogTemp, Warning,
								TEXT("UWaveSpawnerComponent: Waves[%d] is flagged bIsElite but its EnemyClass ('%s') does not derive from AEnemyBase - ignoring the flag for this spawn."),
								WaveIndex, *GetNameSafe(Entry.EnemyClass));
						}
					}
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

	// NextWaveIndex must advance before the broadcast: a handler (e.g. a boss's
	// phase-change hook) may call TriggerNextWave() synchronously from inside it, and
	// TriggerNextWave() targets NextWaveIndex - broadcasting first would leave it
	// pointing at the wave that's still spawning, re-triggering it instead of the next
	// one.
	NextWaveIndex = WaveIndex + 1;
	const int32 NextWaveIndexBeforeBroadcast = NextWaveIndex;

	OnWaveSpawned.Broadcast(WaveIndex);

	// A reentrant TriggerNextWave() call from inside that broadcast already advanced
	// NextWaveIndex further and ran its own scheduling/completion below - detect that
	// and stop here so this frame doesn't do it a second time.
	if (NextWaveIndex != NextWaveIndexBeforeBroadcast)
	{
		return;
	}

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

bool UWaveSpawnerComponent::IsWaveTimerActive() const
{
	if (const UWorld* World = GetWorld())
	{
		return World->GetTimerManager().IsTimerActive(WaveTimerHandle);
	}
	return false;
}
