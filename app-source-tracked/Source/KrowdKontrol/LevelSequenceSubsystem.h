#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LevelSequenceData.h"
#include "LevelSequenceSubsystem.generated.h"

class UDataTable;

// Issue #216, PRD "Level Progression & Teaching Arc" REQ-1 (level-advance half):
// subscribes to ULevelLifecycleSubsystem::OnLevelClear (mirrors
// ULevelBriefingSubsystem's established WorldSubsystem::Initialize()-self-
// subscribes shape, just for OnLevelClear instead of OnLevelBegin) and, on fire,
// looks up the current map's row in LevelSequenceTable by its bare map name.
//
// - If the row's NextLevelMapName is set, loads that map via
//   UGameplayStatics::OpenLevel (guarded by World->IsGameWorld(), same guard
//   AKrowdKontrolPlayerController::RequestLevelRestart() uses -
//   CreateNewMap()-based Automation test Worlds are never game worlds, and a real
//   OpenLevel() call there hangs the in-process Automation run, issue #172).
// - If the row's NextLevelMapName is NAME_None (the sequence's final level), sets
//   ULevelLifecycleSubsystem::FinalMapName to this world's own map name instead
//   of loading anything. This runs synchronously inside the same
//   OnLevelClear.Broadcast() call ULevelLifecycleSubsystem::RefreshLevelClearState()
//   is still executing (all OnLevelClear subscribers, including this one, run to
//   completion before Broadcast() returns) - so by the time
//   RefreshLevelClearState() reaches its own "if (FinalMapName != NAME_None && ...)
//   OnRunComplete.Broadcast()" check immediately afterward, FinalMapName is
//   already set and the existing OnRunComplete path fires without this class ever
//   touching OnRunComplete itself.
// - If the current map has no row at all (not part of the configured sequence,
//   e.g. a prototype map), this is a silent (one-shot-warned) no-op -
//   FinalMapName is left untouched, matching ULevelLifecycleSubsystem::
//   FinalMapName's own "designer hasn't configured this" NAME_None default.
UCLASS()
class KROWDKONTROL_API ULevelSequenceSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// Content asset: one row per level map name, authored EditDefaultsOnly -
	// designer-set later, matching ULevelLifecycleSubsystem::FinalMapName /
	// ULevelBriefingSubsystem::LevelBriefingTable's identical "designer sets this
	// later" gap. Public, so Automation tests inject an in-code
	// NewObject<UDataTable>() directly, no friendship needed.
	UPROPERTY(EditDefaultsOnly, Category = "Level Sequence")
	TObjectPtr<UDataTable> LevelSequenceTable;

	// Matches ULevelLifecycleSubsystem::FOnLevelClear's signature exactly (no
	// parameters - OnLevelClear does not carry the map name, unlike OnLevelBegin).
	// Bound for real in Initialize() below.
	UFUNCTION()
	void HandleLevelClear();

	// Returns the next map's name per LevelSequenceTable for the current world's
	// map, or NAME_None if the current map has no row or its row explicitly ends
	// the sequence. Extracted from HandleLevelClear() so the Automation test can
	// assert the resolved target without ever calling the real,
	// Automation-World-hanging UGameplayStatics::OpenLevel() - mirrors
	// AKrowdKontrolPlayerController::ComputeRestartLevelName()'s identical
	// seam-extraction rationale.
	UFUNCTION(BlueprintPure, Category = "Level Sequence")
	FName ComputeNextLevelMapName() const;

private:
	// Looks up LevelSequenceTable by this world's bare (PIE-prefix-stripped) map
	// name. Returns nullptr if LevelSequenceTable is unset or has no matching row.
	const FLevelSequenceRow* FindCurrentMapRow() const;

	// One-shot guard so a map with no LevelSequenceTable row only logs once per
	// instance, matching ULevelBriefingSubsystem::bHasWarnedMissingBriefingRow's
	// identical idiom.
	bool bHasWarnedMissingSequenceRow = false;
};
