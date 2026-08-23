#include "LevelLifecycleSubsystem.h"
#include "LevelClearTimeSubsystem.h"
#include "EnemyBase.h"
#include "BossBase.h"
#include "WaveSpawnerComponent.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

void ULevelLifecycleSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	EnsureLevelClearTimeSubscription();
}

void ULevelLifecycleSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	UE_LOG(LogTemp, Log,
		TEXT("ULevelLifecycleSubsystem::OnWorldBeginPlay: ENTERED for world '%s' (WorldType=%d)"),
		*InWorld.GetMapName(), static_cast<int32>(InWorld.WorldType));
	Super::OnWorldBeginPlay(InWorld);
	if (bHasFiredLevelBegin)
	{
		// The engine only invokes OnWorldBeginPlay() once per world through its normal
		// path, but this guard also covers direct/manual invocation (as the Automation
		// test does) - it's what keeps a second direct call from re-firing OnLevelBegin.
		return;
	}
	bHasFiredLevelBegin = true;
	EnsureLevelClearTimeSubscription();
	UE_LOG(LogTemp, Log,
		TEXT("ULevelLifecycleSubsystem::OnWorldBeginPlay: broadcasting OnLevelBegin for '%s'"),
		*InWorld.GetMapName());
	OnLevelBegin.Broadcast(FName(*InWorld.GetMapName()));
}

void ULevelLifecycleSubsystem::EnsureLevelClearTimeSubscription()
{
	UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	if (!GameInstance)
	{
		return;
	}
	if (ULevelClearTimeSubsystem* ClearTimeSubsystem = GameInstance->GetSubsystem<ULevelClearTimeSubsystem>())
	{
		ClearTimeSubsystem->SubscribeToLevelLifecycle(this);
	}
	else if (!bHasWarnedMissingLevelClearTimeSubsystem)
	{
		bHasWarnedMissingLevelClearTimeSubsystem = true;
		UE_LOG(LogTemp, Warning,
			TEXT("ULevelLifecycleSubsystem: no ULevelClearTimeSubsystem available on this GameInstance - clear-time tracking will not be wired for this level."));
	}
}

void ULevelLifecycleSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!bHasLoggedFirstTick)
	{
		bHasLoggedFirstTick = true;
		UE_LOG(LogTemp, Log,
			TEXT("ULevelLifecycleSubsystem::Tick: first Tick() observed for world '%s'"),
			GetWorld() ? *GetWorld()->GetMapName() : TEXT("<no world>"));
	}
	RefreshLevelClearState();
	RefreshBossCheckpointState();
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

	UE_LOG(LogTemp, Log,
		TEXT("ULevelLifecycleSubsystem::RefreshLevelClearState: all clear conditions met (TotalEnemies=%d, BankedEnemies=%d) - broadcasting OnLevelClear"),
		TotalEnemies, BankedEnemies);
	bHasFiredLevelClear = true;
	OnLevelClear.Broadcast();

	// Note: FName(*World->GetMapName()) reflects PIE-session name-mangling (e.g.
	// "UEDPIE_0_Level5") if this ever runs in PIE, matching OnWorldBeginPlay()'s
	// identical conversion above - packaged builds and Automation Framework tests
	// (CreateNewMap()) are unaffected. A designer-set FinalMapName using the bare map
	// name would silently never match in PIE.
	if (FinalMapName != NAME_None && FName(*World->GetMapName()) == FinalMapName)
	{
		OnRunComplete.Broadcast();
	}
}

void ULevelLifecycleSubsystem::RefreshBossCheckpointState()
{
	if (bHasReachedBossCheckpoint)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<ABossBase> It(World); It; ++It)
	{
		if (It->GetBossState() != EBossState::Idle)
		{
			bHasReachedBossCheckpoint = true;
			return;
		}
	}
}
