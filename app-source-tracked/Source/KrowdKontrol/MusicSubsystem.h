#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MusicSubsystem.generated.h"

class UAudioComponent;
class USoundBase;

// 3 discrete states, no layering/dynamic stems - Calm/Combat per issue #25's AC,
// BossIntensity added by issue #41 for the boss-fight music-intensity swap
// MISSION.md `12` always intended as a later, separate item (see this header's
// prior revision for the "explicitly out of scope" note this issue resolves).
UENUM(BlueprintType)
enum class EMusicState : uint8
{
	Calm,
	Combat,
	BossIntensity
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMusicStateChanged, EMusicState, NewState);

// Plays a calm placeholder track by default and crossfades to a combat placeholder
// track the instant any AEnemyBase in the world reports IThreatState::Hot (issue #25,
// PRD 12, MISSION.md `12`). Ticks every frame to poll enemy threat state via
// TActorIterator, since no cross-actor aggregation of enemy state exists anywhere
// else in this codebase yet. CalmTrack/CombatTrack are Config-driven (not a
// per-instance Details panel slot, since subsystems are auto-instantiated, not
// placed) - see DefaultGame.ini's [/Script/KrowdKontrol.MusicSubsystem] section.
UCLASS(Config = Game)
class KROWDKONTROL_API UMusicSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

	// Grants the Automation Framework test direct access to CurrentMusicComponent, to
	// assert the crossfade audio path actually spawns/replaces a UAudioComponent on
	// each state switch - not just the CurrentState enum/delegate. Access to spawn/
	// drive AEnemyBaseTestActor instances comes from the separate, non-transitive
	// friend grant on AEnemyBase (see EnemyBase.h); friendship isn't inherited between
	// the two classes.
	friend class FKrowdKontrolMusicSubsystemTest;

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// The default-state track actually starts here, not in Initialize(): Initialize()
	// runs during world/subsystem construction, before the world's AudioDevice is
	// guaranteed live, so a SpawnSound2D() call made there can silently produce no
	// audible playback. OnWorldBeginPlay() is UWorldSubsystem's dedicated "world has
	// truly begun play, actor/audio-dependent setup is now safe" hook.
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	EMusicState GetMusicState() const { return CurrentState; }

	// Re-evaluates whether any enemy is Hot and crossfades if the aggregate state
	// changed. Called automatically from Tick(); exposed publicly (mirroring
	// URoomEnemyBudgetController::InitializeRoom()'s rationale) so the Automation
	// Framework test can drive it deterministically without relying on a real
	// per-frame tick loop.
	UFUNCTION(BlueprintCallable, Category = "Music")
	void RefreshMusicState();

	// Fires every time CurrentState actually changes (never on a no-op refresh).
	UPROPERTY(BlueprintAssignable, Category = "Music")
	FOnMusicStateChanged OnMusicStateChanged;

	UPROPERTY(EditDefaultsOnly, Category = "Music", meta = (ClampMin = "0.0"))
	float CrossfadeDurationSeconds = 2.0f;

	// Placeholder-first (MISSION.md): Config-driven so a designer can point these at
	// real assets later without a code change. Left unset (soft-invalid) is a normal,
	// non-error state during that placeholder period - PlayTrackForState() no-ops
	// playback (but SetMusicState()/Initialize() still update CurrentState/broadcast
	// as usual) if the resolved track is null, same "graceful, no crash" rationale as
	// RoomEnemyBudgetController::EnemyClassToSpawn. Still logs one warning the first
	// time this happens, in case it turns out not to be placeholder-period-transient.
	UPROPERTY(Config, EditDefaultsOnly, Category = "Music")
	TSoftObjectPtr<USoundBase> CalmTrack;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Music")
	TSoftObjectPtr<USoundBase> CombatTrack;

	// Heightened-intensity variant of CombatTrack (issue #41) - same mood/genre,
	// higher energy, not a different song (AC #1). Resolved and played through the
	// exact same PlayTrackForState() crossfade path as CalmTrack/CombatTrack.
	UPROPERTY(Config, EditDefaultsOnly, Category = "Music")
	TSoftObjectPtr<USoundBase> BossIntensityTrack;

private:
	bool IsAnyEnemyInCombat() const;

	// True if any ABossBase in the world is Armed or Vulnerable (i.e. its fight is
	// underway) - needed because ABossBase is not an AEnemyBase, so a boss fight
	// with no spawned adds (ADualZoneBoss, ASleepShieldBoss) would otherwise never
	// register as Combat via IsAnyEnemyInCombat() alone (issue #41).
	bool IsAnyBossEngaged() const;

	// True if any Armed/Vulnerable ABossBase's IsTwistTelegraphed() is currently
	// true (issue #41). Takes priority over Combat in RefreshMusicState().
	bool IsAnyBossTwistTelegraphed() const;

	void SetMusicState(EMusicState NewState);

	// Shared by SetMusicState() and Initialize(): stops CurrentMusicComponent (if any),
	// resolves the track for State, and spawns/fades in the replacement - or warns
	// once if no track is configured. Factored out so Initialize()'s one-shot bypass
	// of SetMusicState()'s no-op guard (see Initialize()'s definition) can't drift out
	// of sync with SetMusicState()'s own playback behavior.
	void PlayTrackForState(EMusicState State);

	UPROPERTY()
	TObjectPtr<UAudioComponent> CurrentMusicComponent;

	EMusicState CurrentState = EMusicState::Calm;

	// One-shot guard so Initialize() only ever kicks off the default-state track once.
	bool bHasStartedInitialTrack = false;

	// One-shot guard so a still-unset CalmTrack/CombatTrack only logs once per
	// UMusicSubsystem instance, not once per Calm/Combat transition.
	bool bHasWarnedMissingTrack = false;
};
