#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CrowdMasteryTotalSubsystem.generated.h"

// docs/prd-crowd-mastery-persistence.md REQ-1, issue #327: sole authority for the
// Crowd Mastery total accumulated across every run cleared this play session.
// GameInstance-scoped (not world-scoped like UCrowdMasterySubsystem, which tracks
// only the current level's running peak) so the total survives level transitions,
// level reruns, and returns to the main menu within a single launch - the same
// "GameInstance owns the cross-level total" precedent ULevelClearTimeSubsystem
// establishes for personal-best clear times.
//
// Public API deliberately never calls GetWorld() or GetGameInstance() - mirrors
// ULevelClearTimeSubsystem's own top-of-file rationale, so this subsystem stays
// directly NewObject<>()-testable with no UWorld/CreateNewMap() dependency. The
// real deposit call comes from UCrowdMasterySubsystem::HandleLevelClear(), which
// legitimately resolves this world's GameInstance since it is itself a
// UWorldSubsystem.
UCLASS()
class KROWDKONTROL_API UCrowdMasteryTotalSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// Adds RunMasteryValue to the accumulated total. Negative input is clamped to 0,
	// same clamp-to-0 idiom ULevelClearTimeSubsystem::RecordCrowdMasteryCount uses for
	// the analogous per-level stat - never subtracted from the running total.
	UFUNCTION(BlueprintCallable, Category = "Crowd Mastery")
	void DepositRunMastery(int32 RunMasteryValue);

	// The accumulated Crowd Mastery total for this GameInstance's play session so far.
	UFUNCTION(BlueprintPure, Category = "Crowd Mastery")
	int32 GetAccumulatedTotal() const { return AccumulatedTotal; }

	// Zeroes the accumulated total. No consumer yet in this issue - REQ-3's future
	// reset control calls this.
	UFUNCTION(BlueprintCallable, Category = "Crowd Mastery")
	void ResetAccumulatedTotal();

private:
	int32 AccumulatedTotal = 0;
};
