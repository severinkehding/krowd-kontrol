#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LevelLifecycleSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLevelBegin, FName, MapName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLevelClear);

// PRD "Run Lifecycle & Progression Signals" REQ-1, issue #169. World-scoped source of
// truth for "the level began"/"the level was cleared" - every downstream lifecycle
// system (clear-time tracking, restart flow, Crowd Mastery, post-run summary,
// run-complete) subscribes to these two signals instead of re-deriving enemy/spawner
// state itself; see this file's REQ-2..7 siblings in docs/prd-run-lifecycle.md. Ticks
// every frame to poll AEnemyBase/UWaveSpawnerComponent state via TActorIterator,
// mirroring UMusicSubsystem's identical "no existing cross-actor event bus" rationale
// (see MusicSubsystem.h). No map-type filtering (DoesSupportWorldType left at its
// default) - the PRD explicitly allows prototype maps to fire OnLevelBegin too.
UCLASS()
class KROWDKONTROL_API ULevelLifecycleSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

	friend class FKrowdKontrolLevelLifecycleSubsystemTest;

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	// Fires exactly once, at this world's begin-play, carrying the map name
	// (FName(*World->GetMapName()), same conversion AKrowdKontrolPlayerController
	// already uses for DiscardLevelTimer's LevelID).
	UPROPERTY(BlueprintAssignable, Category = "Level Lifecycle")
	FOnLevelBegin OnLevelBegin;

	// Fires exactly once, the moment every AEnemyBase in the world (including ones
	// spawned later by UWaveSpawnerComponent) is Banked, given at least one ever
	// existed, and no UWaveSpawnerComponent in the world has a pending wave
	// (IsWaveTimerActive() == true). Never fires if zero enemies ever spawned.
	UPROPERTY(BlueprintAssignable, Category = "Level Lifecycle")
	FOnLevelClear OnLevelClear;

	// Re-evaluates the OnLevelClear condition and broadcasts if newly met. Called
	// automatically from Tick(); public so the Automation Framework test can drive it
	// deterministically without a real per-frame tick loop - same rationale
	// UMusicSubsystem::RefreshMusicState() documents.
	UFUNCTION(BlueprintCallable, Category = "Level Lifecycle")
	void RefreshLevelClearState();

private:
	bool bHasFiredLevelBegin = false;
	bool bHasFiredLevelClear = false;
};
