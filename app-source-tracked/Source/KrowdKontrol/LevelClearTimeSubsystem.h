#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "LevelClearTimeSubsystem.generated.h"

class ULevelClearTimeSaveGame;
class ULevelLifecycleSubsystem;

// Tracks per-level clear time and persists a personal best across play sessions
// (issue #3, PRD 06 REQ-2 - the tracking/persistence layer only; displaying the
// value is a separate issue, see #3's Notes). Local save-slot persistence only, per
// MISSION.md Hard Invariant 7 - no leaderboard, no networked storage.
//
// Wired to real begin/clear events via SubscribeToLevelLifecycle() (issue #170),
// called from ULevelLifecycleSubsystem itself (its Initialize()/OnWorldBeginPlay(),
// not any AActor::BeginPlay()) - see EnsureLevelClearTimeSubscription() on that class
// and SubscribeToLevelLifecycle()'s doc comment below for the wiring's shape and why
// the producer, not a controller, triggers the subscription.
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

	// Discards LevelID's in-progress timer without recording a clear time - used when a
	// run ends in failure (issue #171, PRD "Run Lifecycle & Progression Signals" REQ-3)
	// rather than a clear, so a failed attempt's elapsed time is never considered for the
	// best-time record. Deliberately a sibling of StopLevelTimerAndRecordClear, not a
	// repurposing of it - see that method's own doc comment for why conflating "stop" with
	// "stop and record" would be the wrong shape. Silently no-ops if LevelID has no active
	// timer (unlike StopLevelTimerAndRecordClear, which warns): discarding a timer that
	// was never started isn't a misuse the way stopping one and expecting a real elapsed
	// time back would be.
	UFUNCTION(BlueprintCallable, Category = "Level Clear Time")
	void DiscardLevelTimer(FName LevelID);

	// The deterministic, directly-testable core: compares ClearTimeSeconds against
	// LevelID's currently stored best (if any) and, if it's faster (or no best exists
	// yet), persists it as the new best. Returns true if this call became the new
	// best, false otherwise. Public and callable independently of Start/StopLevelTimer
	// so callers that already measured elapsed time themselves (and this class's own
	// Automation test, for exact deterministic values instead of a real wall-clock
	// delta) can record a clear directly. Negative input is clamped to 0. The return
	// value reflects the in-memory best-time comparison only, not persistence success -
	// see SaveGameToSlot's warn-and-continue handling in the .cpp.
	UFUNCTION(BlueprintCallable, Category = "Level Clear Time")
	bool RecordClearTime(FName LevelID, float ClearTimeSeconds);

	// Reads LevelID's currently stored best time into OutBestSeconds. Returns false
	// (OutBestSeconds set to 0) if no record exists for LevelID yet. Not BlueprintPure
	// - unlike a normal getter this reads the save slot from disk on every call (see
	// this class's .cpp for why: simplicity over caching, matching this codebase's
	// placeholder-first ethos - revisit only if profiling shows it matters).
	UFUNCTION(BlueprintCallable, Category = "Level Clear Time")
	bool GetBestClearTimeSeconds(FName LevelID, float& OutBestSeconds) const;

	// This run's clear time - as opposed to the persisted best GetBestClearTimeSeconds()
	// reads from disk - captured from StopLevelTimerAndRecordClear()'s return value the
	// moment HandleLevelClear() runs. Valid only after HandleLevelClear() has run at
	// least once for the current level; 0 before that. BlueprintPure (unlike
	// GetBestClearTimeSeconds, which is BlueprintCallable only because it reads from
	// disk) since this getter has no side effects.
	UFUNCTION(BlueprintPure, Category = "Level Clear Time")
	float GetLastClearTimeSeconds() const { return LastClearTimeSeconds; }

	// Subscribes this subsystem to LifecycleSubsystem's OnLevelBegin/OnLevelClear
	// delegates (issue #170, PRD "Run Lifecycle & Progression Signals" REQ-2) so a
	// level's clear-time timer starts and stops automatically. Takes the lifecycle
	// subsystem as a direct parameter rather than resolving World/GameInstance
	// internally - this subsystem's public API never calls GetWorld() or
	// GetGameInstance() (see this class's own top comment and
	// KrowdKontrolLevelClearTimeSubsystemTest.cpp's rationale: GetGameInstance() is
	// null in this project's CreateNewMap()-based Automation test worlds), so the
	// Automation Framework test can drive this directly against a bare
	// NewObject<>()-constructed instance. The real caller is
	// ULevelLifecycleSubsystem::EnsureLevelClearTimeSubscription(), invoked from that
	// class's own Initialize()/OnWorldBeginPlay() - NOT from any AActor::BeginPlay()
	// (a prior approach subscribing from AKrowdKontrolPlayerController::BeginPlay()
	// was rejected: UE 5.8 dispatches world-subsystem OnWorldBeginPlay() before
	// GameMode->StartPlay()/controller BeginPlay(), so that approach always missed
	// the level's first OnLevelBegin broadcast). Safe to call repeatedly -
	// AddUniqueDynamic no-ops on a repeat bind to the same handler. Warns and no-ops
	// if LifecycleSubsystem is null.
	UFUNCTION(BlueprintCallable, Category = "Level Clear Time")
	void SubscribeToLevelLifecycle(ULevelLifecycleSubsystem* LifecycleSubsystem);

	// Compares SimultaneousControlledCount against LevelID's currently stored Crowd
	// Mastery best (if any) and, if it's larger (or no best exists yet), persists it as
	// the new best. Returns true if this call became the new best. Same shape as
	// RecordClearTime above, mirrored for a maximized (not minimized) stat: '>' not '<',
	// negative input clamped to 0 via FMath::Max. See RecordClearTime's own doc comment
	// for the shared LoadOrCreateSaveGame/SaveGameToSlot/warn-and-continue behaviour this
	// reuses unchanged.
	UFUNCTION(BlueprintCallable, Category = "Crowd Mastery")
	bool RecordCrowdMasteryCount(FName LevelID, int32 SimultaneousControlledCount);

	// Reads LevelID's currently stored Crowd Mastery best into OutBestCount. Returns
	// false (OutBestCount set to 0) if no record exists for LevelID yet. Same
	// read-from-disk-every-call shape as GetBestClearTimeSeconds above - see that
	// method's doc comment for why (simplicity over caching).
	UFUNCTION(BlueprintCallable, Category = "Crowd Mastery")
	bool GetBestCrowdMasteryCount(FName LevelID, int32& OutBestCount) const;

private:
	ULevelClearTimeSaveGame* LoadOrCreateSaveGame() const;

	// Bound to OnLevelBegin via SubscribeToLevelLifecycle(). Starts this map's timer
	// and remembers MapName so the paired HandleLevelClear() (OnLevelClear carries no
	// parameters of its own) knows which level to stop timing.
	UFUNCTION()
	void HandleLevelBegin(FName MapName);

	// Bound to OnLevelClear via SubscribeToLevelLifecycle(). Stops and records the
	// clear time for CurrentLevelID, the map name HandleLevelBegin last received -
	// relies on ULevelLifecycleSubsystem's own documented guarantee that OnLevelClear
	// never fires before OnLevelBegin has fired at least once for the same world.
	UFUNCTION()
	void HandleLevelClear();

	// Wall-clock start time (FPlatformTime::Seconds()) per level with a currently
	// running timer. Not persisted - an in-progress run's elapsed time only exists for
	// the duration of the current play session, unlike the best-time record itself.
	TMap<FName, double> ActiveLevelStartTimes;

	// The map name from the most recent HandleLevelBegin() call - HandleLevelClear()
	// uses this since OnLevelClear's own signature carries no MapName.
	FName CurrentLevelID;

	// This run's clear time, captured by HandleLevelClear() - see
	// GetLastClearTimeSeconds()'s doc comment above.
	float LastClearTimeSeconds = 0.0f;
};
