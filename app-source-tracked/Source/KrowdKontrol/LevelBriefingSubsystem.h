#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LevelBriefingData.h"
#include "LevelBriefingSubsystem.generated.h"

class UDataTable;
class AKrowdKontrolPlayerController;

// Issue #246, PRD "Mission Briefing & Live Quest Tracker" REQ-1: subscribes to
// ULevelLifecycleSubsystem::OnLevelBegin (mirrors UAbilityUnlockLevelSubsystem's
// established shape - see that class's header comment), looks up that map's row in
// LevelBriefingTable by the bare map name, and forwards it to the possessed
// player's AKrowdKontrolPlayerController for display.
UCLASS()
class KROWDKONTROL_API ULevelBriefingSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// Content asset: one row per level map name (e.g. "L_Level01"), EditDefaultsOnly
	// for the (never exercised by real WorldSubsystems, which have no per-instance
	// Details panel) case some other system wants to override it directly; in practice
	// Initialize() below auto-loads /Game/Data/DT_LevelBriefingTable for real game
	// worlds, mirroring ULevelSequenceSubsystem::LevelSequenceTable's identical
	// auto-load precedent. Public, so Automation tests inject an in-code
	// NewObject<UDataTable>() directly, no friendship needed.
	UPROPERTY(EditDefaultsOnly, Category = "Level Briefing")
	TObjectPtr<UDataTable> LevelBriefingTable;

	// Matches ULevelLifecycleSubsystem::FOnLevelBegin's signature exactly. Bound for
	// real in Initialize() below. Strips PIE session name-mangling via
	// UWorld::RemovePIEPrefix() (matches UAbilityUnlockLevelSubsystem's identical
	// idiom), looks up the bare map name as the DataTable RowName, and if found,
	// forwards it to the possessed player's AKrowdKontrolPlayerController. Silently
	// no-ops (with a one-shot warning) if the table is unset or the map has no row -
	// a level with no authored briefing content is a safe, deliberate no-op. If a
	// row is found but no AKrowdKontrolPlayerController is resolvable yet (order
	// isn't guaranteed relative to OnLevelBegin - same hazard
	// UAbilityUnlockLevelSubsystem::HandleLevelBegin already documents for its own
	// pawn lookup), the row is buffered for RetryPendingBriefingForController()
	// instead of being dropped, since OnLevelBegin only fires once per world.
	UFUNCTION()
	void HandleLevelBegin(FName MapName);

	// Called by AKrowdKontrolPlayerController::BeginPlay() once the controller
	// itself is guaranteed to exist. No-ops if HandleLevelBegin already found a
	// controller (or hasn't fired yet), matching
	// UAbilityUnlockLevelSubsystem::RetryPendingUnlockForPawn's identical shape.
	void RetryPendingBriefingForController(AKrowdKontrolPlayerController* Controller);

private:
	// One-shot guards so a given world instance only logs each missing-content case
	// once, matching UAbilityUnlockLevelSubsystem::bHasWarnedMissingAbilityUnlockComponent's
	// identical idiom.
	bool bHasWarnedMissingBriefingTable = false;
	bool bHasWarnedMissingBriefingRow = false;
	bool bHasWarnedMissingController = false;

	// Set when HandleLevelBegin finds a row but no AKrowdKontrolPlayerController, so
	// a later RetryPendingBriefingForController() call can still deliver it.
	bool bLevelBeginFiredWithNoController = false;
	FLevelBriefingRow PendingBriefingRow;
};
