#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "LevelSequenceData.generated.h"

// The run's level sequence (issue #216, PRD "Level Progression & Teaching Arc"
// REQ-1, level-advance half). One row per level map name (e.g. "L_Level01"),
// authored in an EditDefaultsOnly UDataTable - mirrors LevelBriefingData.h's
// identical "one row per map name, never hardcoded per-map C++" shape, so a
// designer editing level content only has to learn one DataTable convention for
// this whole family of per-level config (briefing content, sequence order).
USTRUCT(BlueprintType)
struct FLevelSequenceRow : public FTableRowBase
{
	GENERATED_BODY()

	// The next level's map name (e.g. "L_Level02"). NAME_None marks this row's own
	// level (the DataTable's RowName) as the sequence's final level - clearing it
	// routes to ULevelLifecycleSubsystem::OnRunComplete instead of loading a
	// further map (see ULevelSequenceSubsystem::HandleLevelClear).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level Sequence")
	FName NextLevelMapName;
};
