#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CrowdMasteryTotalSubsystem.generated.h"

class ULevelClearTimeSaveGame;

// docs/prd-crowd-mastery-persistence.md REQ-1, issue #327: sole authority for the
// Crowd Mastery total accumulated across every run cleared this play session.
// GameInstance-scoped (not world-scoped like UCrowdMasterySubsystem, which tracks
// only the current level's running peak) so the total survives level transitions,
// level reruns, and returns to the main menu within a single launch - the same
// "GameInstance owns the cross-level total" precedent ULevelClearTimeSubsystem
// establishes for personal-best clear times.
//
// Public API deliberately never calls GetWorld() or GetGameInstance() - mirrors
// ULevelClearTimeSubsystem's own rationale (see that class's
// SubscribeToLevelLifecycle() doc comment), so this subsystem stays directly
// NewObject<>()-testable with no UWorld/CreateNewMap() dependency. The
// real deposit call comes from UCrowdMasterySubsystem::HandleLevelClear(), which
// legitimately resolves this world's GameInstance since it is itself a
// UWorldSubsystem. Initialize()/LoadPersistedTotal()/PersistAccumulatedTotal()
// (PRD "Crowd Mastery Persistence" REQ-4, issue #330) only ever call slot-based
// UGameplayStatics functions, never GetWorld() or GetGameInstance(), so this rule
// still holds after cross-launch persistence was added.
UCLASS()
class KROWDKONTROL_API UCrowdMasteryTotalSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// Adds RunMasteryValue to the accumulated total and persists the new total to disk
	// (PRD "Crowd Mastery Persistence" REQ-4, issue #330). Negative input is clamped to
	// 0, same clamp-to-0 idiom ULevelClearTimeSubsystem::RecordCrowdMasteryCount uses
	// for the analogous per-level stat - never subtracted from the running total.
	UFUNCTION(BlueprintCallable, Category = "Crowd Mastery")
	void DepositRunMastery(int32 RunMasteryValue);

	// The accumulated Crowd Mastery total for this GameInstance's play session so far.
	UFUNCTION(BlueprintPure, Category = "Crowd Mastery")
	int32 GetAccumulatedTotal() const { return AccumulatedTotal; }

	// Zeroes the accumulated total and persists the reset to disk (PRD "Crowd Mastery
	// Persistence" REQ-4, issue #330), so a relaunch doesn't resurrect the pre-reset
	// value. REQ-3's reset control calls this.
	UFUNCTION(BlueprintCallable, Category = "Crowd Mastery")
	void ResetAccumulatedTotal();

	// Reads the persisted accumulated total from the shared save slot into
	// AccumulatedTotal (PRD "Crowd Mastery Persistence" REQ-4, issue #330). Called from
	// Initialize() at real GameInstance startup; public (not private, not called only
	// from Initialize()) so the Automation Framework test can drive it directly against
	// a bare NewObject<>()-constructed instance without a live FSubsystemCollectionBase.
	UFUNCTION(BlueprintCallable, Category = "Crowd Mastery")
	void LoadPersistedTotal();

private:
	ULevelClearTimeSaveGame* LoadOrCreateSaveGame() const;
	void PersistAccumulatedTotal() const;

	int32 AccumulatedTotal = 0;
};
