#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "LevelBriefingData.generated.h"

// Per-level pre-level-briefing content (issue #246, PRD "Mission Briefing & Live
// Quest Tracker" REQ-1). One row per level map name (e.g. "L_Level01"), authored in
// an EditDefaultsOnly UDataTable - never hardcoded C++ strings per map.
USTRUCT(BlueprintType)
struct FLevelBriefingRow : public FTableRowBase
{
	GENERATED_BODY()

	// e.g. "LEVEL 1".
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level Briefing")
	FText LevelDisplayName;

	// Imperative one-liners, e.g. "PACIFY ALL 8 ROBOTS — STUN THEM, HERD THEM TO
	// THEIR PENS". Rendered one per line.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level Briefing")
	TArray<FText> ObjectiveLines;

	// e.g. "NEW: SLEEP — PRESS 2 — STRONG VS SNIPERS". Empty = no unlock line shown
	// this level.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level Briefing")
	FText NewAbilityUnlockLine;
};
