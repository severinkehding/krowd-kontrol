#include "CrowdMasterySubsystem.h"
#include "LevelLifecycleSubsystem.h"
#include "EnemyBase.h"
#include "EngineUtils.h"

void UCrowdMasterySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UWorld* World = GetWorld())
	{
		if (ULevelLifecycleSubsystem* LifecycleSubsystem = World->GetSubsystem<ULevelLifecycleSubsystem>())
		{
			LifecycleSubsystem->OnLevelBegin.AddDynamic(this, &UCrowdMasterySubsystem::HandleLevelBegin);
		}
	}
}

void UCrowdMasterySubsystem::SampleControlledCount()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	int32 ControlledCount = 0;
	for (TActorIterator<AEnemyBase> It(World); It; ++It)
	{
		if (It->GetEnemyState() == EEnemyState::Controlled)
		{
			++ControlledCount;
		}
	}
	RunningMaxControlledCount = FMath::Max(RunningMaxControlledCount, ControlledCount);
}

void UCrowdMasterySubsystem::HandleAbilityCastApplied(EAbilitySlot Ability, AEnemyBase* TargetEnemy)
{
	SampleControlledCount();
}

void UCrowdMasterySubsystem::HandleEnemyControlledExpired()
{
	SampleControlledCount();
}

void UCrowdMasterySubsystem::HandleLevelBegin(FName MapName)
{
	RunningMaxControlledCount = 0;
}
