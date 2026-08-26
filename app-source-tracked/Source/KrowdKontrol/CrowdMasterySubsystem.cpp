#include "CrowdMasterySubsystem.h"
#include "CrowdMasteryTotalSubsystem.h"
#include "LevelLifecycleSubsystem.h"
#include "EnemyBase.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"

void UCrowdMasterySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// Force ULevelLifecycleSubsystem to construct/Initialize() before we look it up
	// below - FSubsystemCollectionBase does not otherwise guarantee sibling
	// construction order between Initialize() calls.
	Collection.InitializeDependency<ULevelLifecycleSubsystem>();
	if (UWorld* World = GetWorld())
	{
		if (ULevelLifecycleSubsystem* LifecycleSubsystem = World->GetSubsystem<ULevelLifecycleSubsystem>())
		{
			LifecycleSubsystem->OnLevelBegin.AddDynamic(this, &UCrowdMasterySubsystem::HandleLevelBegin);
			LifecycleSubsystem->OnLevelClear.AddDynamic(this, &UCrowdMasterySubsystem::HandleLevelClear);
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

void UCrowdMasterySubsystem::HandleLevelClear()
{
	UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	if (!GameInstance)
	{
		if (!bHasWarnedMissingCrowdMasteryTotalSubsystem)
		{
			bHasWarnedMissingCrowdMasteryTotalSubsystem = true;
			UE_LOG(LogTemp, Warning,
				TEXT("UCrowdMasterySubsystem::HandleLevelClear: no GameInstance available - Crowd Mastery total was not deposited for this level."));
		}
		return;
	}

	if (UCrowdMasteryTotalSubsystem* TotalSubsystem = GameInstance->GetSubsystem<UCrowdMasteryTotalSubsystem>())
	{
		TotalSubsystem->DepositRunMastery(RunningMaxControlledCount);
	}
	else if (!bHasWarnedMissingCrowdMasteryTotalSubsystem)
	{
		bHasWarnedMissingCrowdMasteryTotalSubsystem = true;
		UE_LOG(LogTemp, Warning,
			TEXT("UCrowdMasterySubsystem::HandleLevelClear: no UCrowdMasteryTotalSubsystem available on this GameInstance - Crowd Mastery total was not deposited for this level."));
	}
}
