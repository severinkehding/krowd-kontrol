#include "LevelSequenceSubsystem.h"
#include "LevelLifecycleSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"

void ULevelSequenceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<ULevelLifecycleSubsystem>();
	if (UWorld* World = GetWorld())
	{
		if (ULevelLifecycleSubsystem* LifecycleSubsystem = World->GetSubsystem<ULevelLifecycleSubsystem>())
		{
			LifecycleSubsystem->OnLevelClear.AddDynamic(this, &ULevelSequenceSubsystem::HandleLevelClear);
		}
	}
}

const FLevelSequenceRow* ULevelSequenceSubsystem::FindCurrentMapRow() const
{
	if (!LevelSequenceTable)
	{
		return nullptr;
	}
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	const FString BareMapName = UWorld::RemovePIEPrefix(World->GetMapName());
	return LevelSequenceTable->FindRow<FLevelSequenceRow>(FName(*BareMapName), TEXT("ULevelSequenceSubsystem::FindCurrentMapRow"), false);
}

FName ULevelSequenceSubsystem::ComputeNextLevelMapName() const
{
	const FLevelSequenceRow* Row = FindCurrentMapRow();
	return Row ? Row->NextLevelMapName : NAME_None;
}

void ULevelSequenceSubsystem::HandleLevelClear()
{
	UWorld* World = GetWorld();
	const FLevelSequenceRow* Row = FindCurrentMapRow();
	if (!Row)
	{
		if (!bHasWarnedMissingSequenceRow)
		{
			bHasWarnedMissingSequenceRow = true;
			UE_LOG(LogTemp, Warning,
				TEXT("ULevelSequenceSubsystem::HandleLevelClear: no LevelSequenceTable row for the current map - level-sequence advance will not run."));
		}
		return;
	}

	if (Row->NextLevelMapName == NAME_None)
	{
		// Explicit end-of-sequence marker for this row. Set FinalMapName so
		// ULevelLifecycleSubsystem::RefreshLevelClearState()'s own FinalMapName
		// comparison - which runs immediately after this OnLevelClear subscriber
		// returns, inside the same OnLevelClear.Broadcast() call - fires the
		// existing OnRunComplete path instead of loading a further map.
		if (World)
		{
			if (ULevelLifecycleSubsystem* LifecycleSubsystem = World->GetSubsystem<ULevelLifecycleSubsystem>())
			{
				LifecycleSubsystem->FinalMapName = FName(*World->GetMapName());
			}
		}
		return;
	}
}

void ULevelSequenceSubsystem::AdvanceToNextLevel()
{
	const FName NextLevelMapName = ComputeNextLevelMapName();
	if (NextLevelMapName == NAME_None)
	{
		return;
	}

	// Real map travel only makes sense in an actual game world (PIE or packaged) -
	// never in the Editor-type Worlds FAutomationEditorCommonUtils::CreateNewMap()
	// returns for KrowdKontrol.Unit.* tests, where OpenLevel would try to travel a
	// World that was never loaded from a real map package, hanging the Automation
	// run (same hazard AKrowdKontrolPlayerController::RequestLevelRestart()
	// documents, issue #172).
	UWorld* World = GetWorld();
	if (World && World->IsGameWorld())
	{
		UGameplayStatics::OpenLevel(this, NextLevelMapName);
	}
}
