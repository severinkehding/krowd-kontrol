#include "LevelClearTimeSubsystem.h"
#include "LevelClearTimeSaveGame.h"
#include "LevelLifecycleSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformTime.h"

const FString ULevelClearTimeSubsystem::SaveSlotName = TEXT("KrowdKontrol_LevelClearTimes");

void ULevelClearTimeSubsystem::StartLevelTimer(FName LevelID)
{
	ActiveLevelStartTimes.Add(LevelID, FPlatformTime::Seconds());
}

float ULevelClearTimeSubsystem::StopLevelTimerAndRecordClear(FName LevelID)
{
	const double* StartTime = ActiveLevelStartTimes.Find(LevelID);
	if (!StartTime)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("ULevelClearTimeSubsystem::StopLevelTimerAndRecordClear: no active timer for level '%s' - no-op."),
			*LevelID.ToString());
		return 0.0f;
	}

	const float ElapsedSeconds = FMath::Max(0.0f, static_cast<float>(FPlatformTime::Seconds() - *StartTime));
	ActiveLevelStartTimes.Remove(LevelID);
	RecordClearTime(LevelID, ElapsedSeconds);
	return ElapsedSeconds;
}

void ULevelClearTimeSubsystem::DiscardLevelTimer(FName LevelID)
{
	ActiveLevelStartTimes.Remove(LevelID);
}

bool ULevelClearTimeSubsystem::RecordClearTime(FName LevelID, float ClearTimeSeconds)
{
	const float ClampedSeconds = FMath::Max(0.0f, ClearTimeSeconds);

	ULevelClearTimeSaveGame* SaveGameObject = LoadOrCreateSaveGame();
	const float* ExistingBest = SaveGameObject->BestClearTimesByLevel.Find(LevelID);
	const bool bIsNewBest = !ExistingBest || ClampedSeconds < *ExistingBest;

	if (bIsNewBest)
	{
		SaveGameObject->BestClearTimesByLevel.Add(LevelID, ClampedSeconds);
		if (!UGameplayStatics::SaveGameToSlot(SaveGameObject, SaveSlotName, 0))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("ULevelClearTimeSubsystem::RecordClearTime: SaveGameToSlot failed for level '%s' - new best time was not persisted and will be lost."),
				*LevelID.ToString());
		}
	}

	return bIsNewBest;
}

bool ULevelClearTimeSubsystem::GetBestClearTimeSeconds(FName LevelID, float& OutBestSeconds) const
{
	ULevelClearTimeSaveGame* SaveGameObject = LoadOrCreateSaveGame();
	if (const float* Best = SaveGameObject->BestClearTimesByLevel.Find(LevelID))
	{
		OutBestSeconds = *Best;
		return true;
	}
	OutBestSeconds = 0.0f;
	return false;
}

void ULevelClearTimeSubsystem::SubscribeToLevelLifecycle(ULevelLifecycleSubsystem* LifecycleSubsystem)
{
	if (!LifecycleSubsystem)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("ULevelClearTimeSubsystem::SubscribeToLevelLifecycle: LifecycleSubsystem is null - clear-time tracking will not be wired for this level."));
		return;
	}
	LifecycleSubsystem->OnLevelBegin.AddUniqueDynamic(this, &ULevelClearTimeSubsystem::HandleLevelBegin);
	LifecycleSubsystem->OnLevelClear.AddUniqueDynamic(this, &ULevelClearTimeSubsystem::HandleLevelClear);
}

void ULevelClearTimeSubsystem::HandleLevelBegin(FName MapName)
{
	UE_LOG(LogTemp, Warning,
		TEXT("ULevelClearTimeSubsystem::HandleLevelBegin: starting timer for '%s'"),
		*MapName.ToString());
	CurrentLevelID = MapName;
	StartLevelTimer(MapName);
}

void ULevelClearTimeSubsystem::HandleLevelClear()
{
	UE_LOG(LogTemp, Warning,
		TEXT("ULevelClearTimeSubsystem::HandleLevelClear: stopping timer and recording clear for '%s'"),
		*CurrentLevelID.ToString());
	StopLevelTimerAndRecordClear(CurrentLevelID);
}

bool ULevelClearTimeSubsystem::RecordCrowdMasteryCount(FName LevelID, int32 SimultaneousControlledCount)
{
	const int32 ClampedCount = FMath::Max(0, SimultaneousControlledCount);

	ULevelClearTimeSaveGame* SaveGameObject = LoadOrCreateSaveGame();
	const int32* ExistingBest = SaveGameObject->BestCrowdMasteryByLevel.Find(LevelID);
	const bool bIsNewBest = !ExistingBest || ClampedCount > *ExistingBest;

	if (bIsNewBest)
	{
		SaveGameObject->BestCrowdMasteryByLevel.Add(LevelID, ClampedCount);
		if (!UGameplayStatics::SaveGameToSlot(SaveGameObject, SaveSlotName, 0))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("ULevelClearTimeSubsystem::RecordCrowdMasteryCount: SaveGameToSlot failed for level '%s' - new Crowd Mastery best was not persisted and will be lost."),
				*LevelID.ToString());
		}
	}

	return bIsNewBest;
}

bool ULevelClearTimeSubsystem::GetBestCrowdMasteryCount(FName LevelID, int32& OutBestCount) const
{
	ULevelClearTimeSaveGame* SaveGameObject = LoadOrCreateSaveGame();
	if (const int32* Best = SaveGameObject->BestCrowdMasteryByLevel.Find(LevelID))
	{
		OutBestCount = *Best;
		return true;
	}
	OutBestCount = 0;
	return false;
}

ULevelClearTimeSaveGame* ULevelClearTimeSubsystem::LoadOrCreateSaveGame() const
{
	if (UGameplayStatics::DoesSaveGameExist(SaveSlotName, 0))
	{
		if (USaveGame* Loaded = UGameplayStatics::LoadGameFromSlot(SaveSlotName, 0))
		{
			if (ULevelClearTimeSaveGame* Typed = Cast<ULevelClearTimeSaveGame>(Loaded))
			{
				return Typed;
			}
			UE_LOG(LogTemp, Warning,
				TEXT("ULevelClearTimeSubsystem::LoadOrCreateSaveGame: save slot '%s' loaded but was not a ULevelClearTimeSaveGame - starting from an empty record."),
				*SaveSlotName);
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("ULevelClearTimeSubsystem::LoadOrCreateSaveGame: save slot '%s' exists but failed to load - starting from an empty record."),
				*SaveSlotName);
		}
	}
	return CastChecked<ULevelClearTimeSaveGame>(UGameplayStatics::CreateSaveGameObject(ULevelClearTimeSaveGame::StaticClass()));
}
