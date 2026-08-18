#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "LevelClearTimeSubsystem.generated.h"

class ULevelClearTimeSaveGame;

// Tracks per-level clear time and persists a personal best across play sessions
// (issue #3, PRD 06 REQ-2 - the tracking/persistence layer only; displaying the
// value is a separate issue, see #3's Notes). Local save-slot persistence only, per
// MISSION.md Hard Invariant 7 - no leaderboard, no networked storage.
//
// No real caller wiring exists yet - the level-progression system this is meant for
// (PRD 05) doesn't expose a "level begins"/"level cleared" event today, matching
// UGizmoNarrativeSubsystem's (issue #57) same "foundation, no live wiring yet"
// precedent. A future level-progression issue calls StartLevelTimer/
// StopLevelTimerAndRecordClear from real begin/clear events.
UCLASS()
class KROWDKONTROL_API ULevelClearTimeSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// Save slot every instance shares - a single slot holding every level's best time
	// (keyed inside by FName), not one slot per level. Public so the Automation
	// Framework test (and anything else that needs a clean-slate starting state) can
	// clear real on-disk state via UGameplayStatics::DeleteGameInSlot() directly.
	static const FString SaveSlotName;

	// Starts tracking elapsed time for LevelID. Calling this again for a LevelID with
	// an already-running timer restarts it from the current time - no warning, since
	// re-entering a level before clearing it is a normal player action, not a misuse.
	UFUNCTION(BlueprintCallable, Category = "Level Clear Time")
	void StartLevelTimer(FName LevelID);

	// Stops LevelID's timer and records the elapsed time via RecordClearTime() below.
	// Returns the elapsed seconds measured (0 and no-op if StartLevelTimer was never
	// called for this LevelID - logs a warning, mirroring
	// UGizmoNarrativeSubsystem::TriggerBark's unknown-ID no-op-with-warning
	// convention, rather than crashing or recording a bogus zero-second clear).
	UFUNCTION(BlueprintCallable, Category = "Level Clear Time")
	float StopLevelTimerAndRecordClear(FName LevelID);

	// The deterministic, directly-testable core: compares ClearTimeSeconds against
	// LevelID's currently stored best (if any) and, if it's faster (or no best exists
	// yet), persists it as the new best. Returns true if this call became the new
	// best, false otherwise. Public and callable independently of Start/StopLevelTimer
	// so callers that already measured elapsed time themselves (and this class's own
	// Automation test, for exact deterministic values instead of a real wall-clock
	// delta) can record a clear directly. Negative input is clamped to 0.
	UFUNCTION(BlueprintCallable, Category = "Level Clear Time")
	bool RecordClearTime(FName LevelID, float ClearTimeSeconds);

	// Reads LevelID's currently stored best time into OutBestSeconds. Returns false
	// (OutBestSeconds set to 0) if no record exists for LevelID yet. Not BlueprintPure
	// - unlike a normal getter this reads the save slot from disk on every call (see
	// this class's .cpp for why: simplicity over caching, matching this codebase's
	// placeholder-first ethos - revisit only if profiling shows it matters).
	UFUNCTION(BlueprintCallable, Category = "Level Clear Time")
	bool GetBestClearTimeSeconds(FName LevelID, float& OutBestSeconds) const;

private:
	ULevelClearTimeSaveGame* LoadOrCreateSaveGame() const;

	// Wall-clock start time (FPlatformTime::Seconds()) per level with a currently
	// running timer. Not persisted - an in-progress run's elapsed time only exists for
	// the duration of the current play session, unlike the best-time record itself.
	TMap<FName, double> ActiveLevelStartTimes;
};
