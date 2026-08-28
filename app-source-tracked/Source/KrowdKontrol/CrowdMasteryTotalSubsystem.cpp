#include "CrowdMasteryTotalSubsystem.h"
#include "LevelClearTimeSaveGame.h"
#include "LevelClearTimeSubsystem.h"
#include "Kismet/GameplayStatics.h"

void UCrowdMasteryTotalSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadPersistedTotal();
}

void UCrowdMasteryTotalSubsystem::DepositRunMastery(int32 RunMasteryValue)
{
	AccumulatedTotal += FMath::Max(0, RunMasteryValue);
	PersistAccumulatedTotal();
}

void UCrowdMasteryTotalSubsystem::ResetAccumulatedTotal()
{
	AccumulatedTotal = 0;
	PersistAccumulatedTotal();
}

void UCrowdMasteryTotalSubsystem::LoadPersistedTotal()
{
	ULevelClearTimeSaveGame* SaveGameObject = LoadOrCreateSaveGame();
	AccumulatedTotal = SaveGameObject->AccumulatedCrowdMasteryTotal;
}

ULevelClearTimeSaveGame* UCrowdMasteryTotalSubsystem::LoadOrCreateSaveGame() const
{
	if (UGameplayStatics::DoesSaveGameExist(ULevelClearTimeSubsystem::SaveSlotName, 0))
	{
		if (USaveGame* Loaded = UGameplayStatics::LoadGameFromSlot(ULevelClearTimeSubsystem::SaveSlotName, 0))
		{
			if (ULevelClearTimeSaveGame* Typed = Cast<ULevelClearTimeSaveGame>(Loaded))
			{
				return Typed;
			}
			UE_LOG(LogTemp, Warning,
				TEXT("UCrowdMasteryTotalSubsystem::LoadOrCreateSaveGame: save slot '%s' loaded but was not a ULevelClearTimeSaveGame - starting from an empty record."),
				*ULevelClearTimeSubsystem::SaveSlotName);
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("UCrowdMasteryTotalSubsystem::LoadOrCreateSaveGame: save slot '%s' exists but failed to load - starting from an empty record."),
				*ULevelClearTimeSubsystem::SaveSlotName);
		}
	}
	return CastChecked<ULevelClearTimeSaveGame>(UGameplayStatics::CreateSaveGameObject(ULevelClearTimeSaveGame::StaticClass()));
}

void UCrowdMasteryTotalSubsystem::PersistAccumulatedTotal() const
{
	ULevelClearTimeSaveGame* SaveGameObject = LoadOrCreateSaveGame();
	SaveGameObject->AccumulatedCrowdMasteryTotal = AccumulatedTotal;
	if (!UGameplayStatics::SaveGameToSlot(SaveGameObject, ULevelClearTimeSubsystem::SaveSlotName, 0))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UCrowdMasteryTotalSubsystem::PersistAccumulatedTotal: SaveGameToSlot failed for slot '%s' - accumulated total was not persisted and will be lost on next launch."),
			*ULevelClearTimeSubsystem::SaveSlotName);
	}
}
