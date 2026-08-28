#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "LevelClearTimeSaveGame.generated.h"

// Plain data container persisted via UGameplayStatics::SaveGameToSlot/LoadGameFromSlot
// by ULevelClearTimeSubsystem (issue #3, PRD 06 REQ-2). Keyed per level (FName), not
// global, per the issue's acceptance criteria - one personal-best entry per level ID.
UCLASS()
class KROWDKONTROL_API ULevelClearTimeSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level Clear Time")
	TMap<FName, float> BestClearTimesByLevel;

	// Crowd Mastery best (issue #174, docs/prd-run-lifecycle.md REQ-5): the largest number of
	// simultaneously-Controlled AEnemyBase instances ever observed in a single level,
	// keyed per level (FName) same as BestClearTimesByLevel above - persisted through
	// the same save slot, not a separate save-game class.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd Mastery")
	TMap<FName, int32> BestCrowdMasteryByLevel;

	// Crowd Mastery accumulated total (PRD "Crowd Mastery Persistence" REQ-4, issue
	// #330): the running Crowd Mastery total accumulated across every run cleared,
	// this launch and every prior one - the persisted mirror of
	// UCrowdMasteryTotalSubsystem::AccumulatedTotal. That subsystem remains the sole
	// runtime authority; this field exists only so the total survives a fresh launch.
	// Same shared save slot as the fields above, not a separate save-game class.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd Mastery")
	int32 AccumulatedCrowdMasteryTotal = 0;
};
