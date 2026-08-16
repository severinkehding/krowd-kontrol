#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnemyType.h"
#include "WaveSpawnerComponent.generated.h"

// One entry in UWaveSpawnerComponent::Waves: spawn Count actors of EnemyClass, after
// DelaySeconds have elapsed since the previous wave started (or since StartWaves() for
// entry 0) - or immediately if DelaySeconds is 0. EnemyType is a tag only, mirroring
// FRoomTargetZone::EnemyType (RoomActor.h) - it is never branched on here, so the
// spawner stays type-agnostic; resolving EnemyType to a class is the caller's job. A
// boss/room can instead call TriggerNextWave() to fire this wave early regardless of
// DelaySeconds.
USTRUCT(BlueprintType)
struct FWaveEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave")
	EEnemyType EnemyType = EEnemyType::RU_NNR;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave")
	TSubclassOf<AActor> EnemyClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave", meta = (ClampMin = "0"))
	int32 Count = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave", meta = (ClampMin = "0.0"))
	float DelaySeconds = 0.0f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWaveSpawned, int32, WaveIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAllWavesComplete);

// Reusable, sequenced enemy-wave spawner (PRD 03 REQ-5, issue #21): spawns a
// caller-configured list of (EnemyType, EnemyClass, Count, DelaySeconds) waves in
// order, chaining wave-to-wave via FTimerManager for nonzero delays or synchronously
// for zero-delay waves. Mirrors URoomEnemyBudgetController (issue #82) and
// UStationPowerUpComponent (issue #60): a placeable component with no opinion on *when*
// it's triggered - it never references ARoomActor or ABossBase, and never branches on
// EEnemyType - so the same component serves both a room's setup and a boss encounter's
// setup with zero per-type or per-caller code.
//
// Does not wire itself to any trigger - callers invoke StartWaves() to begin the
// sequence, and TriggerNextWave() to fire the next pending wave immediately (this
// component has no opinion on what triggers it - a boss's own phase-change hook, a
// room's door-opened event, or an Automation test with no real tick loop all use the
// same entry point).
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class KROWDKONTROL_API UWaveSpawnerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWaveSpawnerComponent();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave Spawner")
	TArray<FWaveEntry> Waves;

	// Fires each time a wave finishes spawning, with its index in Waves.
	UPROPERTY(BlueprintAssignable, Category = "Wave Spawner")
	FOnWaveSpawned OnWaveSpawned;

	// Fires exactly once, when every entry in Waves has spawned (including immediately,
	// for an empty Waves array - see StartWaves()'s warning for that misconfiguration
	// case instead).
	UPROPERTY(BlueprintAssignable, Category = "Wave Spawner")
	FOnAllWavesComplete OnAllWavesComplete;

	// Begins the sequence: spawns wave 0 immediately, or schedules it after its
	// DelaySeconds. Idempotent so callers - including the Automation Framework test -
	// can drive it deterministically without needing a full actor BeginPlay lifecycle.
	// Logs a warning, and completes immediately with no spawns, if Waves is empty.
	UFUNCTION(BlueprintCallable, Category = "Wave Spawner")
	void StartWaves();

	// Fires the next pending wave immediately, clearing any pending timer for it first.
	// This component has no opinion on what triggers it - a boss's phase-change hook, a
	// room's door-opened event, or a test can all call this directly. No-ops once the
	// sequence is already complete (or before StartWaves() has ever been called).
	UFUNCTION(BlueprintCallable, Category = "Wave Spawner")
	void TriggerNextWave();

	const TArray<TObjectPtr<AActor>>& GetSpawnedActors() const { return SpawnedActors; }
	int32 GetNextWaveIndex() const { return NextWaveIndex; }

	// Test-support accessor, same rationale as GetSpawnedActors()/GetNextWaveIndex():
	// this harness never drives a real BeginPlay lifecycle (see EnemyBase.h's
	// "driven World->BeginPlay()" note), so the Automation test calls EndPlay()
	// directly to verify its timer cleanup and needs a way to observe the result.
	bool IsWaveTimerActive() const;

	// Public (not protected, despite overriding a protected UActorComponent method) so
	// the Automation test can invoke it directly - see IsWaveTimerActive()'s comment.
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void SpawnWave(int32 WaveIndex);
	void ScheduleWave(int32 WaveIndex);

	UPROPERTY()
	TArray<TObjectPtr<AActor>> SpawnedActors;

	int32 NextWaveIndex = 0;

	bool bHasStarted = false;
	bool bAllWavesCompleteFired = false;

	FTimerHandle WaveTimerHandle;
};
