#include "LevelClearTimeSubsystem.h"
#include "LevelClearTimeSaveGame.h"
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

	const float ElapsedSeconds = static_cast<float>(FPlatformTime::Seconds() - *StartTime);
	ActiveLevelStartTimes.Remove(LevelID);
	RecordClearTime(LevelID, ElapsedSeconds);
	return ElapsedSeconds;
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
				TEXT("ULevelClearTimeSubsystem::RecordClearTime: SaveGameToSlot failed for level '%s' - best time updated in memory only for this session."),
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
		}
	}
	return CastChecked<ULevelClearTimeSaveGame>(UGameplayStatics::CreateSaveGameObject(ULevelClearTimeSaveGame::StaticClass()));
}
