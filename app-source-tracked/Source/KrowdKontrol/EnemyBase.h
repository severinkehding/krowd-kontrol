#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbilitySlot.h"
#include "ThreatState.h"
#include "EnemyBase.generated.h"

class UPlayerEnergyComponent;
class UPointLightComponent;

// Idle -> Alert -> Attack -> Controlled -> Banked, with Banked as the only reachable
// "defeated" state. Transition table (no other edges exist):
//   Idle -> Alert: player enters DetectionRangeUnits (proximity check, no perception
//     system, no pathfinding).
//   Alert -> Attack: player enters GetAttackRangeUnits() (overridable per concrete
//     enemy type).
//   Alert/Attack -> Controlled: ReceiveControl(EAbilitySlot) is called.
//   Controlled -> Banked: TransitionToBanked() is called.
//   Controlled -> Alert: the Controlled-state duration elapses before
//     TransitionToBanked() is called (operator decision, issue #138, 2026-08-18).
// See issue #12, PRD 03, MISSION.md Hard Invariant 2.
UENUM(BlueprintType)
enum class EEnemyState : uint8
{
	Idle,
	Alert,
	Attack,
	Controlled,
	Banked
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEnemyBanked);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEnemyControlledExpired);

// Shared, structurally-safe foundation for every core enemy type (SN-1PR, RU-NNR,
// TR-UPR, B0-0MR - PRD 03): a linear Idle->Alert->Attack->Controlled->Banked state
// machine, self-driven by a per-tick proximity check against the player pawn (unlike
// ABossBase, which never self-aggros), plus ReceiveControl()/TransitionToBanked() as
// the two public hooks a future crowd-control-ability cast system and target-zone
// delivery system respectively need. A concrete subclass overrides
// GetAttackRangeUnits()/OnControlledEntry()/OnAttackEntry() for its own reaction
// visuals - it never has to re-derive or risk violating the "defeat always ends in
// Banked, never a kill" guarantee. Abstract: never placed/spawned directly, only
// subclassed. See issue #12.
UCLASS(Abstract)
class KROWDKONTROL_API AEnemyBase : public AActor, public IThreatState
{
	GENERATED_BODY()

	// Grants the Automation Framework tests direct access to TickCheckDetection
	// below, so a headless test can drive deterministic proximity checks without a
	// real per-frame Tick() loop - same rationale UAbilityCooldownComponent's
	// FKrowdKontrolAbilityCooldownTest friendship documents. Friendship isn't
	// inherited, so each concrete subclass's own test (Sniper, Bomber) needs its own
	// grant here to drive an instance through Idle->Alert->Attack deterministically -
	// and so does any other subsystem's test (e.g. FKrowdKontrolMusicSubsystemTest)
	// that needs to drive a plain AEnemyBaseTestActor through the same transitions.
	friend class FKrowdKontrolEnemyBaseTest;
	friend class FKrowdKontrolSniperEnemyTest;
	friend class FKrowdKontrolBomberEnemyTest;
	friend class FKrowdKontrolTrooperEnemyTest;
	friend class FKrowdKontrolRunnerEnemyTest;
	friend class FKrowdKontrolMusicSubsystemTest;
	friend class FKrowdKontrolOvercrowdDetectionComponentTest;
	friend class FKrowdKontrolOvercrowdAudioSubsystemTest;
	friend class FKrowdKontrolAbilityCastComponentTest;
	friend class FKrowdKontrolFlatCamera3DAbilityCastWiringTest;
	friend class FKrowdKontrolOvercrowdLevelThresholdTest;
	friend class FKrowdKontrolAbilityVFXColourTest;
	friend class FKrowdKontrolGizmoFirstContactComponentTest;
	friend class FKrowdKontrolSleepShieldBossTest;
	friend class FKrowdKontrolRootSurgeBossTest;
	friend class FKrowdKontrolFirstStunBeaconComponentTest;
	friend class FKrowdKontrolAbilityMatchupSignalComponentTest;
	friend class FKrowdKontrolLevelLifecycleSubsystemTest;
	friend class FKrowdKontrolCrowdMasterySubsystemTest;
	// Same grant, for the BeginPlay-wiring coverage test (issue #174 pass-2 code-review
	// finding), which drives one enemy to Controlled to prove the wired delegate
	// transmits. Non-transitive - see MusicSubsystem.h's friend-class comment.
	friend class FKrowdKontrolCrowdMasteryBeginPlayWiringTest;
	friend class FKrowdKontrolLevelClearTimeWiringTest;

	// Same grant, for UOvercrowdVisualEffectSubsystem's own test and the audio/visual
	// sync test (issue #20), which drive a plain AEnemyBaseTestActor through the same
	// Idle->Alert transition as FKrowdKontrolOvercrowdAudioSubsystemTest above.
	friend class FKrowdKontrolOvercrowdVisualEffectSubsystemTest;
	friend class FKrowdKontrolOvercrowdAudioVisualSyncTest;

	// Same grant, for the issue #178 production-wiring case added to
	// KrowdKontrolHUDWiringTest.cpp, which drives a plain AEnemyBaseTestActor through
	// the same Idle->Alert transition to prove a real cast + real punishment trigger
	// locks the tray through the real pawn/controller/widget wiring.
	friend class FKrowdKontrolHUDWiringTest;

	// Same grant, for the punishment-arbitration test (issue #180), which drives a plain
	// AEnemyBaseTestActor through the same Idle->Alert transition to arm real Overcrowd
	// detection. Non-transitive - see MusicSubsystem.h's friend-class comment.
	friend class FKrowdKontrolPunishmentArbitrationComponentTest;

public:
	AEnemyBase();

	// IThreatState (issue #25): Alert/Attack/Controlled all read as "Hot" - any state
	// where the enemy is actively engaged, not just mid-attack. Idle and Banked both
	// read as "Idle" - not yet aggroed, or pacified. See ThreatState.h.
	virtual EThreatState GetThreatState() const override;

	// Fires exactly once, on the transition into Banked.
	UPROPERTY(BlueprintAssignable, Category = "Enemy")
	FOnEnemyBanked OnEnemyBanked;

	// Fires exactly once, when the Controlled-state duration elapses and
	// CurrentState reverts to Alert (see TickControlledDuration) - the Controlled ->
	// Alert edge in the transition table above. Never fires on the Controlled ->
	// Banked edge (that's OnEnemyBanked's edge instead).
	UPROPERTY(BlueprintAssignable, Category = "Enemy")
	FOnEnemyControlledExpired OnEnemyControlledExpired;

	EEnemyState GetEnemyState() const { return CurrentState; }

	// Only meaningful while GetEnemyState() == Controlled; retains its last value
	// otherwise (never reset on reversion or banking) - same "stale read, guarded by
	// state" contract CurrentState itself guards every other accessor with.
	EAbilitySlot GetControllingAbility() const { return ControllingAbility; }

	// Idle->Alert proximity radius. Base-defined, not overridden per concrete type -
	// issue #12's AC only calls out attack range as the per-type-overridable one.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Detection")
	float DetectionRangeUnits = 1500.0f;

	// No-op unless CurrentState is Alert or Attack. Interface/hook only - no CC-ability
	// logic here; the caller (future ability-cast system) owns that entirely.
	UFUNCTION(BlueprintCallable, Category = "Enemy")
	void ReceiveControl(EAbilitySlot Ability);

	// No-op unless CurrentState == Controlled. Interface/hook only - no delivery-to-
	// target-zone logic here; that stays level/room scope.
	UFUNCTION(BlueprintCallable, Category = "Enemy")
	void TransitionToBanked();

	// Elite configuration (PRD 03 REQ-4, issue #19): a secondary, non-reserved trim
	// colour plus a movement-speed multiplier layered on top of this type's own
	// GetMovementSpeedUnitsPerSecond() override - never a 5th EEnemyType or a new
	// subclass, per MISSION.md Hard Invariant 5. False (and multiplier inert) unless
	// SetIsElite(true) is called - normal spawns are entirely unaffected.
	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Elite")
	bool bIsElite = false;

	// Applied on top of GetMovementSpeedUnitsPerSecond() only while bIsElite is true -
	// see GetEffectiveMovementSpeedUnitsPerSecond(). EditDefaultsOnly so each concrete
	// type/Blueprint can tune its own Elite speed bump independently, satisfying the
	// issue's "configurable per enemy type" AC without needing a per-type override.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Elite", meta = (ClampMin = "1.0"))
	float EliteMovementSpeedMultiplier = 1.3f;

	// Trim-light intensity while bIsElite is true (0.0f while false) - see
	// GetEliteTrimLightComponent()/SetIsElite().
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Elite")
	float EliteTrimIntensity = 1600.0f;

	// Toggles bIsElite and the trim light's visibility together, so the two can never
	// desync. BlueprintCallable so UWaveSpawnerComponent::SpawnWave() (a plain
	// AActor* spawn path) can call it post-spawn via a Cast<AEnemyBase>, same shape as
	// ReceiveControl()/TransitionToBanked() above.
	UFUNCTION(BlueprintCallable, Category = "Enemy|Elite")
	void SetIsElite(bool bNewIsElite);

protected:
	virtual void Tick(float DeltaTime) override;

	// Binds OnEnemyControlledExpired to
	// UCrowdMasterySubsystem::HandleEnemyControlledExpired (issue #174 AC1) here, not
	// the constructor - GetWorld() has no valid subsystem collection to resolve
	// against until this actor has begun play, same "bound in BeginPlay, not the
	// constructor" precedent ADualZoneBoss::BeginPlay establishes for a delegate
	// binding that depends on world state.
	virtual void BeginPlay() override;

	// Per-type attack range, in units. Base default is 0.0f (never reaches Attack on
	// its own); a concrete subclass overrides this per issue #12's AC.
	virtual float GetAttackRangeUnits() const { return 0.0f; }

	// Chase speed, in units/second, applied while CurrentState == Alert (closing
	// distance so Attack range can be reached). Overridable per concrete enemy type
	// (issue #122, PRD 03's per-type speed column) - same "protected virtual, safe
	// base default" shape as GetAttackRangeUnits() above. Base default matches
	// UCharacterMovementComponent's engine-default MaxWalkSpeed (600.0f), the same
	// "normal" reference point BomberEnemy.h's MovementSpeed comment already cites.
	virtual float GetMovementSpeedUnitsPerSecond() const { return 600.0f; }

	// GetMovementSpeedUnitsPerSecond() * (bIsElite ? EliteMovementSpeedMultiplier :
	// 1.0f) - TickChaseMovement calls this, not GetMovementSpeedUnitsPerSecond()
	// directly, so every concrete type's existing per-type override (issue #122)
	// gets the Elite bump for free with zero change to any of the 4 override
	// implementations themselves.
	float GetEffectiveMovementSpeedUnitsPerSecond() const;

	// Each concrete subclass's own non-reserved secondary trim light, lit only while
	// bIsElite is true - overridden to return that subclass's own EliteTrimLightComponent
	// property (see RunnerEnemy.h etc.), the same per-type-property shape every other
	// type-tied component (AttackTellLightComponent, DrainGlowLightComponent/
	// CoreGlowLightComponent/GlowLightComponent/EyeGlowLightComponent) already uses.
	// Base default nullptr, same "safe base default" shape as GetAttackRangeUnits()
	// above - SetIsElite() below null-checks before use.
	virtual UPointLightComponent* GetEliteTrimLightComponent() const { return nullptr; }

	// TActorIterator, not UGameplayStatics::GetPlayerPawn() - the latter needs a
	// driven World->BeginPlay() pass this module's Automation tests never run.
	// Assumes exactly one live APawn carries UPlayerEnergyComponent (true today;
	// revisit if local co-op/split-screen or a debug dummy pawn is ever added).
	// Returns nullptr (and logs a warning) if no such pawn is found. See issue #15,
	// the first enemy-attack code path to actually touch player state.
	UPlayerEnergyComponent* FindPlayerEnergyComponent() const;

	// C++-only (not BlueprintNativeEvent) until a real concrete subclass exists to
	// inform whether these hooks need Blueprint override - same rationale
	// BossBase.h/ThreatState.h/Herdable.h document for their own extension points.
	virtual void OnControlledEntry(EAbilitySlot Ability) {}
	virtual void OnAttackEntry() {}

	// Issue #121's per-enemy/per-ability duration-override point. -1.0f (base default)
	// means "no override, use AbilityData::BaseDurationSeconds unmodified"; a concrete
	// subclass returns a non-negative value to override it (e.g. ASniperEnemy for Sleep
	// - see issue #121's SN-1PR/Sleep=7s case).
	virtual float GetControlledDurationOverrideSeconds(EAbilitySlot Ability) const { return -1.0f; }

private:
	// Internal transition-guard logic, never subclass-overridable directly - keeps the
	// state machine's own invariants (proximity in, no direct state writes) enforced
	// in exactly one place. TickCheckDetection is private (not protected) for the same
	// reason.
	void TickCheckDetection(const FVector& PlayerLocation);

	// Moves the actor in a straight line toward PlayerLocation at
	// GetMovementSpeedUnitsPerSecond() units/second, clamped so it never overshoots
	// past the player within one tick. No-op outside Alert - Idle hasn't detected the
	// player yet, and Attack/Controlled/Banked have no PRD-specified reason to keep
	// closing distance once attack range is reached (REQ-2: no pathfinding, straight-
	// line only). Private/friend-testable, same shape as TickCheckDetection above.
	void TickChaseMovement(const FVector& PlayerLocation, float DeltaSeconds);
	void AdvanceToAlert();
	void AdvanceToAttack();

	// Decrements RemainingControlledSeconds while CurrentState == Controlled and
	// reverts to Alert once it reaches zero - the Controlled -> Alert edge documented
	// in the transition table above. No-op in every other state, same state-guard
	// shape TickCheckDetection/TickChaseMovement already use.
	void TickControlledDuration(float DeltaSeconds);

	EEnemyState CurrentState = EEnemyState::Idle;

	EAbilitySlot ControllingAbility = EAbilitySlot::Stun;

	float RemainingControlledSeconds = 0.0f;
};
