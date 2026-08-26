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
// - If the row's NextLevelMapName is set, HandleLevelClear() no longer loads it
//   automatically (issue #321 - this used to call UGameplayStatics::OpenLevel()
//   here, but that raced the post-run summary screen, tearing the world down
//   before the player could read it or press anything). See AdvanceToNextLevel()
//   below for the now caller-triggered load.
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

	// Loads ComputeNextLevelMapName()'s resolved next map, if any, guarded by
	// World->IsGameWorld() (same CreateNewMap()-Automation-World-hang hazard
	// HandleLevelClear() above already documents, issue #172's precedent). This used to
	// run automatically from HandleLevelClear() the instant OnLevelClear fired (issue
	// #216) - moved out to here, an explicit caller-triggered action, so the post-run
	// summary's NEXT LEVEL button (issue #321) - not an automatic broadcast - decides
	// when the level actually travels. No-op if the current map has no row or is
	// already the sequence's final level (NAME_None).
	UFUNCTION(BlueprintCallable, Category = "Level Sequence")
	void AdvanceToNextLevel();

	// Recorded unconditionally at the top of AdvanceToNextLevel(), before its
	// IsGameWorld() guard - the real UGameplayStatics::OpenLevel() call stays
	// unreachable in Automation's CreateNewMap() Editor Worlds (issue #172), but this
	// lets tests assert AdvanceToNextLevel() was actually reached and resolved the
	// right map, and that HandleLevelClear() never triggers it itself (issue #321's
	// critical fix: auto-advance only ever runs from the NEXT LEVEL button's click
	// handler now). Public for the same test-observability reason as
	// LevelSequenceTable above - stays NAME_None if AdvanceToNextLevel() was never
	// called or had nothing to advance to.
	UPROPERTY(VisibleAnywhere, Category = "Level Sequence")
	FName LastAdvanceAttemptedMapName = NAME_None;

private:
	// Looks up LevelSequenceTable by this world's bare (PIE-prefix-stripped) map
	// name. Returns nullptr if LevelSequenceTable is unset or has no matching row.
	const FLevelSequenceRow* FindCurrentMapRow() const;

	// One-shot guard so a map with no LevelSequenceTable row only logs once per
	// instance, matching ULevelBriefingSubsystem::bHasWarnedMissingBriefingRow's
	// identical idiom.
	bool bHasWarnedMissingSequenceRow = false;
};
