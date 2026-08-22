#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LevelBriefingData.h"
#include "LevelBriefingSubsystem.generated.h"

class UDataTable;

// Issue #246, PRD "Mission Briefing & Live Quest Tracker" REQ-1: subscribes to
// ULevelLifecycleSubsystem::OnLevelBegin (mirrors UAbilityUnlockLevelSubsystem's
// established shape - see that class's header comment), looks up that map's row in
// LevelBriefingTable by the bare map name, and forwards it to the possessed
// player's AKrowdKontrolPlayerController for display.
UCLASS()
class KROWDKONTROL_API ULevelBriefingSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

	friend class FKrowdKontrolLevelBriefingSubsystemTest;

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// Content asset: one row per level map name (e.g. "L_Level01"), authored
	// EditDefaultsOnly - designer-set later, matching
	// ULevelLifecycleSubsystem::FinalMapName's identical "designer sets this later"
	// gap. Automation tests inject an in-code NewObject<UDataTable>() via the
	// friendship above.
	UPROPERTY(EditDefaultsOnly, Category = "Level Briefing")
	TObjectPtr<UDataTable> LevelBriefingTable;

	// Matches ULevelLifecycleSubsystem::FOnLevelBegin's signature exactly. Bound for
	// real in Initialize() below. Strips PIE session name-mangling via
	// UWorld::RemovePIEPrefix() (matches UAbilityUnlockLevelSubsystem's identical
	// idiom), looks up the bare map name as the DataTable RowName, and if found,
	// forwards it to the possessed player's AKrowdKontrolPlayerController. Silently
	// no-ops (with a one-shot warning) if the table is unset or the map has no row -
	// a level with no authored briefing content is a safe, deliberate no-op.
	UFUNCTION()
	void HandleLevelBegin(FName MapName);

private:
	// One-shot guards so a given world instance only logs each missing-content case
	// once, matching UAbilityUnlockLevelSubsystem::bHasWarnedMissingAbilityUnlockComponent's
	// identical idiom.
	bool bHasWarnedMissingBriefingTable = false;
	bool bHasWarnedMissingBriefingRow = false;
};
