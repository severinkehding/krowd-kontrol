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
};
