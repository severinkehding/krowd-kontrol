#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbilitySlot.h"
#include "ThreatState.h"
#include "Herdable.h"
#include "EnemyBase.generated.h"

class UPlayerEnergyComponent;
class UPointLightComponent;
class ARoomActor;
class UControlledDurationIndicatorComponent;

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
//   Controlled -> Alert: ReceiveControl(Ability) is called with an Ability different
//     from ControllingAbility while ControllingAbility's AbilityData flags
//     bWakesEarlyOnOtherAbilityHit (issue #257) - reuses the same edge/broadcast as
//     the duration-expiry case above, just triggered early.
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
class KROWDKONTROL_API AEnemyBase : public AActor, public IThreatState, public IHerdable
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
	friend class FKrowdKontrolAbilityPressHoldComponentTest;
	friend class FKrowdKontrolFlatCamera3DAbilityCastWiringTest;
	friend class FKrowdKontrolOvercrowdLevelThresholdTest;
	friend class FKrowdKontrolAbilityVFXColourTest;
	friend class FKrowdKontrolGizmoFirstContactComponentTest;
	friend class FKrowdKontrolSleepShieldBossTest;
	friend class FKrowdKontrolRootSurgeBossTest;
	friend class FKrowdKontrolFirstStunBeaconComponentTest;
	friend class FKrowdKontrolAbilityMatchupSignalComponentTest;
	friend class FKrowdKontrolLevelLifecycleSubsystemTest;
	friend class FKrowdKontrolLevelSequenceSubsystemTest;
	friend class FKrowdKontrolCrowdMasterySubsystemTest;
	// Same grant, for the BeginPlay-wiring coverage test (issue #174 pass-2 code-review
	// finding), which drives one enemy to Controlled to prove the wired delegate
	// transmits. Non-transitive - see MusicSubsystem.h's friend-class comment.
	friend class FKrowdKontrolCrowdMasteryBeginPlayWiringTest;
	friend class FKrowdKontrolLevelClearTimeWiringTest;

	// Same grant, for the post-run summary widget end-to-end wiring test (issue #175),
	// which drives real AEnemyBaseTestActor instances through Idle->Alert->Attack via
	// the private TickCheckDetection before ReceiveControl()/TransitionToBanked(), to
	// prove a real OnLevelClear broadcast reaches the widget with real clear-time/best/
	// Crowd-Mastery data. Non-transitive - see MusicSubsystem.h's friend-class comment.
	friend class FKrowdKontrolPostRunSummaryWidgetWiringTest;

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

	// Same grant, for the IHerdable coverage test (issue #211), which drives
	// AEnemyBaseTestActor through Idle->Alert via the private TickCheckDetection
	// before calling the public ReceiveControl()/TransitionToBanked() to reach
	// Controlled/Banked. Non-transitive - see MusicSubsystem.h's friend-class comment.
	friend class FKrowdKontrolEnemyBaseHerdableTest;

	// Same grant, for the ARoomActor banking-wiring end-to-end test (issue #211),
	// which drives a real concrete AEnemyBase subclass through Idle->Alert via the
	// private TickCheckDetection before ReceiveControl(), to prove a physical zone
	// overlap reaches Banked through the real OnActorBanked->TransitionToBanked wire.
	friend class FKrowdKontrolRoomActorBankingWiringTest;

	// Same grant, for the room-door-gating test (issue #218), which drives real
	// AEnemyBase subclasses through Idle->Alert via the private TickCheckDetection
	// before ReceiveControl()/TransitionToBanked(), to prove a door's gate opens/closes
	// as its ARoomActor's OwnedEnemies reach Banked. Non-transitive - see
	// MusicSubsystem.h's friend-class comment.
	friend class FKrowdKontrolRoomActorDoorGatingTest;

	// Same grant, for the room-detection-gate test (issue #244), which sets
	// OwningRoom via the public SetOwningRoom() and then drives Idle->Alert via the
	// private TickCheckDetection to prove the gate itself, not just the wiring.
	friend class FKrowdKontrolEnemyRoomDetectionGateTest;

	// Same grant, for the room-activation-countdown test (issue #245), which
	// drives owned enemies' TickCheckDetection directly to prove they stay Idle
	// while the room's first-entry countdown is running, and resume normal
	// detection once it activates. Non-transitive - see MusicSubsystem.h's
	// friend-class comment.
	friend class FKrowdKontrolRoomActivationCountdownTest;

	// Same grant, for the quest-tracker suggestion test (issue #249's pass-2 review
	// coverage), which drives a plain AEnemyBaseTestActor through Idle->Alert->
	// Controlled->Banked to prove HandleActorBanked() recomputes the suggested-ability
	// line, not just the banked count.
	friend class FKrowdKontrolQuestTrackerWidgetTest;

	// Same grant, for the quest-tracker room-state test (issue #248), which drives a
	// plain AEnemyBaseTestActor through Idle->Alert->Controlled->Banked to prove
	// ARoomActor::OnRoomClearedStateChanged reaches the widget's room-state line.
	friend class FKrowdKontrolQuestTrackerWidgetRoomStateTest;

	// Same grant, for the RoomActor-level GetRemainingEnemyCount() test (issue #248
	// test-coverage follow-up), which drives plain AEnemyBaseTestActors through
	// Idle->Alert->Controlled->Banked to prove the count itself, not just the
	// == 0 boundary IsRoomCleared() coverage already exercises.
	friend class FKrowdKontrolRoomActorRemainingEnemyCountTest;

	// Same grant, for the Controlled-duration indicator test (issue #225), which
	// drives a plain AEnemyBaseTestActor through Idle->Alert->Controlled and calls
	// the private TickControlledDuration directly to prove the indicator's
	// reflected FillFraction decreases in lockstep with the real duration tick,
	// not just a mocked value. Non-transitive - see MusicSubsystem.h's
	// friend-class comment.
	friend class FKrowdKontrolControlledDurationIndicatorComponentTest;

	// Same grant, for the colour-match duration-bonus test (issue #65), which
	// drives real ATrooperEnemy/ABomberEnemy instances through Idle->Alert->Attack
	// via the private TickCheckDetection before ReceiveControl(), to prove the
	// per-enemy GetControlledDurationOverrideSeconds() bonus (Root/Fear) and the
	// no-bonus mismatch/Stun cases against real concrete subclasses, not a
	// generic AEnemyBaseTestActor. Non-transitive - see MusicSubsystem.h's
	// friend-class comment.
	friend class FKrowdKontrolAbilityColourMatchTest;

public:
	AEnemyBase();

	// IThreatState (issue #25): Alert/Attack/Controlled all read as "Hot" - any state
	// where the enemy is actively engaged, not just mid-attack. Idle and Banked both
	// read as "Idle" - not yet aggroed, or pacified. See ThreatState.h.
	virtual EThreatState GetThreatState() const override;

	// IHerdable (issue #79/#211): IsControlled() reads the same CurrentState the
	// public GetEnemyState() accessor exposes - Controlled is the one state where an
	// ATargetZone should recognize this actor as bankable. GetHerdColourTag() reads
	// through AbilityData::Get(ControllingAbility) rather than a local switch, so the
	// ability<->colour mapping has exactly one source of truth (AbilityData.cpp).
	// Both inherit ControllingAbility's own "stale read, guarded by state" contract -
	// GetHerdColourTag() is only meaningful while IsControlled() is true, same as
	// GetControllingAbility() itself already documents.
	virtual bool IsControlled() const override;
	virtual FName GetHerdColourTag() const override;

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

	// Only meaningful while GetEnemyState() == Controlled; retains its last value
	// otherwise (never reset on reversion or banking) - same "stale read, guarded by
	// state" contract GetControllingAbility() itself documents.
	float GetRemainingControlledSeconds() const { return RemainingControlledSeconds; }

	// The actual duration applied when this Controlled window began - includes any
	// GetControlledDurationOverrideSeconds() override (e.g. Sniper's 7s Sleep lock),
	// never the ability's unmodified AbilityData::BaseDurationSeconds when an
	// override applies. Same "stale read, guarded by state" contract as
	// GetRemainingControlledSeconds() above. Defaults to 0.0f before the first
	// ReceiveControl() call - callers computing Remaining/Total must first confirm
	// GetEnemyState() == Controlled (or has been) to avoid a 0.0f/0.0f division.
	float GetTotalControlledSeconds() const { return TotalControlledSeconds; }

	// The Controlled-duration bar (issue #225, PRD docs/prd-enemy-effect-indicator.md
	// REQ-1) - base-class-owned since every field it reads
	// (RemainingControlledSeconds/TotalControlledSeconds) already is. Public so the
	// Automation test (and any future MCP-driven holdout, which per project
	// convention can only read reflected UPROPERTY state) can assert
	// bIsVisible/FillFraction directly without needing friendship for this alone.
	UControlledDurationIndicatorComponent* GetControlledDurationIndicatorComponent() const { return ControlledDurationIndicatorComponent; }

	// Idle->Alert proximity radius. Base-defined, not overridden per concrete type -
	// issue #12's AC only calls out attack range as the per-type-overridable one.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Detection")
	float DetectionRangeUnits = 1500.0f;

	// Herd-follow speed (issue #214), in units/second, applied while CurrentState ==
	// Controlled and ControllingAbility doesn't already claim this tick's movement via
	// Snare (bAllowsMovementWhileControlled) or Fear (bFleesFromCasterWhileControlled) -
	// see TickFollowMovement(). Deliberately its own flat base-class property, not a
	// fraction of the per-type-virtual GetMovementSpeedUnitsPerSecond() - every
	// controlled enemy trails the player at the same, predictable pace regardless of
	// its own type's chase speed. Default (300.0f) is half AEnemyBase's own base
	// chase-speed default (600.0f) - fast enough to keep pace with a walking player,
	// slow enough to read as trailing, not chasing.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Herd")
	float FollowSpeedUnitsPerSecond = 300.0f;

	// The trailing gap TickFollowMovement stops at, in units, so a Controlled enemy
	// never stacks on/overlaps the player pawn while following (issue #214, operator
	// decision 2026-08-22: pied-piper trailing, not stacking). Base-defined, not
	// overridden per concrete type - same precedent DetectionRangeUnits above sets.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Herd")
	float FollowDistanceUnits = 200.0f;

	// Radius of the circle multiple simultaneous followers are placed around,
	// centered on the player (issue #215). Only applied when 2+ enemies are
	// simultaneously Controlled-and-following - a solo follower's offset is always
	// FVector::ZeroVector regardless of this value, matching #214's unmodified
	// single-enemy behavior. See ComputeFollowSeparationOffset().
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Herd")
	float FollowSeparationRadiusUnits = 150.0f;

	// Issue #244: the room whose OwnedEnemies list this enemy was added to (nullptr
	// if none - see ARoomActor::AddOwnedEnemy()'s doc comment on the auto-discovery
	// that normally sets this with zero .umap authoring). Public so ARoomActor can
	// set it without needing friendship, same shape as ReceiveControl()/
	// TransitionToBanked() being the public hooks other systems call into this class.
	UFUNCTION(BlueprintCallable, Category = "Enemy|Detection")
	void SetOwningRoom(ARoomActor* Room) { OwningRoom = Room; }

	ARoomActor* GetOwningRoom() const { return OwningRoom.Get(); }

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

	// FollowSpeedUnitsPerSecond * (bIsElite ? EliteMovementSpeedMultiplier : 1.0f) -
	// TickFollowMovement calls this, not FollowSpeedUnitsPerSecond directly, mirroring
	// GetEffectiveMovementSpeedUnitsPerSecond()'s exact shape so an Elite enemy is
	// harder to herd in every movement mode, not just while chasing.
	float GetEffectiveFollowSpeedUnitsPerSecond() const;

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

	// Fires on every Controlled -> Alert edge (see the transition table above: both
	// the natural-duration-expiry case and the issue #257 early-wake-on-other-ability
	// case), right before OnEnemyControlledExpired broadcasts. Every other ability
	// already clears a concrete subclass's own AttackTellLightComponent immediately on
	// OnControlledEntry, but Root (bAllowsAttackWhileControlled) deliberately leaves it
	// running for the Controlled window's duration - this is the matching hook a
	// concrete subclass overrides to clear it once that window ends without banking
	// (pass-1 review follow-up, issue #255).
	virtual void OnControlledExpired() {}

	// Issue #121's per-enemy/per-ability duration-override point. -1.0f (base default)
	// means "no override, use AbilityData::BaseDurationSeconds unmodified"; a concrete
	// subclass returns a non-negative value to override it (e.g. ASniperEnemy for Sleep
	// - see issue #121's SN-1PR/Sleep=7s case).
	virtual float GetControlledDurationOverrideSeconds(EAbilitySlot Ability) const { return -1.0f; }

	// True while this enemy's attack behaviour (per-type telegraph/tell/fire loop)
	// should keep running: always during Attack, and also during Controlled if
	// ControllingAbility is flagged AbilityData::bAllowsAttackWhileControlled (Root
	// only - issue #255: Root does not silence an attack the enemy was already
	// capable of, unlike Stun/Sleep's full-immobilize flavour. Root no longer implies
	// immobile movement either, as of issue #214's TickFollowMovement - it still
	// doesn't grant Snare's own independent TickChaseMovement-driven movement, so it
	// falls through to the same pied-piper follow every other non-Snare/Fear
	// Controlled ability gets).
	// Concrete subclasses' own AdvanceXTelegraph functions call this instead of a raw
	// GetEnemyState() == Attack check.
	bool IsAttackBehaviorActive() const;

	// True while this enemy's chase movement (TickChaseMovement) should keep running:
	// always during Alert, and also during Controlled if ControllingAbility is flagged
	// AbilityData::bAllowsMovementWhileControlled (Snare only - issue #254: Snare slows
	// but does not freeze). Mirrors IsAttackBehaviorActive()'s shape exactly.
	bool IsMovementBehaviorActive() const;

	// AbilityData::Get(ControllingAbility).ControlledSpeedMultiplier while Controlled,
	// 1.0f (full speed) otherwise - TickChaseMovement and every concrete subclass's own
	// AdvanceXTelegraph consult this instead of a raw DeltaSeconds, so Snare's 50% slow
	// (or Root's inert 1.0f "full speed while allowed to attack") applies uniformly to
	// both movement and attack-telegraph timing with one source of truth.
	// Virtual for issue #65's per-matchup potency bonus: RU-NNR deepens Snare's slow
	// from the 50% base to 75% on colour match (docs/prd-ability-shapes.md's locked
	// table) - a potency override, where the other three matchups override duration
	// via GetControlledDurationOverrideSeconds() instead.
	virtual float GetControlledSpeedMultiplier() const;

private:
	// Internal transition-guard logic, never subclass-overridable directly - keeps the
	// state machine's own invariants (proximity in, no direct state writes) enforced
	// in exactly one place. TickCheckDetection is private (not protected) for the same
	// reason.
	void TickCheckDetection(const FVector& PlayerLocation);

	// Issue #244: true if OwningRoom is unset (unscoped legacy behaviour - an enemy
	// with no owning room, e.g. a level with zero ARoomActors) or if PlayerLocation's
	// nearest room (ARoomActor::FindNearestRoom - the same rule OwnedEnemies
	// auto-discovery and ADoorConnectorActor's GatingRoom derivation already use, PR
	// #229) is this enemy's own OwningRoom. Only ever called from
	// TickCheckDetection's Idle->Alert branch - never gates Alert->Attack or any
	// other transition, per the issue's "already-Alert enemies are unaffected" AC.
	bool IsPlayerInOwningRoom(const FVector& PlayerLocation) const;

	// Moves the actor in a straight line toward PlayerLocation at
	// GetMovementSpeedUnitsPerSecond() units/second, clamped so it never overshoots
	// past the player within one tick. No-op outside Alert - Idle hasn't detected the
	// player yet, and Attack/Controlled/Banked have no PRD-specified reason to keep
	// closing distance once attack range is reached (REQ-2: no pathfinding, straight-
	// line only). Private/friend-testable, same shape as TickCheckDetection above.
	void TickChaseMovement(const FVector& PlayerLocation, float DeltaSeconds);

	// Moves the actor in a straight line away from CasterLocation at
	// GetEffectiveMovementSpeedUnitsPerSecond() units/second, with no
	// remaining-distance clamp - fleeing has no destination to overshoot, unlike
	// TickChaseMovement's toward-player movement. No-op unless CurrentState ==
	// Controlled and ControllingAbility is flagged
	// AbilityData::bFleesFromCasterWhileControlled (Fear only - see
	// AbilityData.h). A CasterLocation exactly coincident with this actor's own
	// location (degenerate away-direction) is also a no-op, using the same
	// KINDA_SMALL_NUMBER dead-zone style as TickChaseMovement's own guard
	// (against SizeSquared() here rather than Size(), since this function
	// already needs a squared distance for GetSafeNormal() below - both are
	// negligible at game scale). Private/friend-testable, same shape as
	// TickChaseMovement above.
	void TickFleeMovement(const FVector& CasterLocation, float DeltaSeconds);

	// Moves the actor in a straight line toward PlayerLocation at
	// GetEffectiveFollowSpeedUnitsPerSecond() units/second, stopping once within
	// FollowDistanceUnits of the player rather than closing all the way to their
	// location - the "pied-piper" herd-follow movement (issue #214, operator decision
	// 2026-08-22: controlled enemies trail the player, they don't stack on the pawn).
	// No-op outside Controlled, and also a no-op while ControllingAbility already
	// claims this tick's movement through a different flavour - Snare
	// (bAllowsMovementWhileControlled, TickChaseMovement, issue #254) or Fear
	// (bFleesFromCasterWhileControlled, TickFleeMovement, issue #253) - so a Controlled
	// enemy is never moved twice in the same tick by competing movement rules. Root is
	// deliberately NOT excluded here (review follow-up, issue #214): docs/prd-herd-
	// mechanic.md's operator design decision applies to every Controlled enemy with no
	// per-ability carve-out, and ARootSurgeBoss::HasRootLockedAdd() only checks
	// GetEnemyState()/GetControllingAbility(), never position, so a Root-Controlled
	// add trailing the player doesn't affect that boss's Vulnerable-state gate.
	// Private/friend-testable, same shape as TickChaseMovement/TickFleeMovement above.
	// Issue #215: the actual movement target is PlayerLocation plus
	// ComputeFollowSeparationOffset() below, not PlayerLocation directly - see that
	// method's doc comment for why (spreads multiple simultaneous followers into a
	// legible train; resolves to zero for a solo follower, preserving #214 exactly).
	void TickFollowMovement(const FVector& PlayerLocation, float DeltaSeconds);

	// Issue #215: resolves this enemy's own distinct follow-target offset among all
	// other simultaneously-Controlled-and-following enemies, so multiple followers
	// spread into a visually legible train instead of converging on the exact same
	// point (TickFollowMovement above adds this to PlayerLocation before computing
	// ToTarget). Iterates all AEnemyBase actors in the world via TActorIterator,
	// mirroring UCrowdMasterySubsystem::SampleControlledCount()'s exact
	// "TActorIterator<AEnemyBase>, Controlled-state filter" shape (issue explicitly
	// forbids touching that subsystem's own logic, so this is an independent,
	// parallel iteration, not a call into it) - then narrows further to actors that
	// are *actually* moved by TickFollowMovement this tick, applying the identical
	// Snare/Fear movement-conflict gate TickFollowMovement's own gate uses (a
	// Snare-Controlled or Fear-Controlled enemy is moved by TickChaseMovement/
	// TickFleeMovement instead, so it should not consume a follow slot). Returns
	// FVector::ZeroVector whenever fewer than 2 such followers exist (including
	// GetWorld() == nullptr, the common case in this file's own NewObject-only
	// Automation tests) - the "no unnecessary offset for a solo follower" AC.
	// Slot order is simply TActorIterator's own encounter order, not sorted by any
	// stable key - the AC only requires distinct positions THIS tick among the
	// current followers, not a persistent per-enemy slot across ticks, and no
	// Unreal API guarantees TActorIterator/GetUniqueID() ordering stability across
	// spawn/destroy churn or level reloads. Private/friend-testable, same shape as
	// TickFollowMovement itself.
	FVector ComputeFollowSeparationOffset() const;

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

	float TotalControlledSeconds = 0.0f;

	UPROPERTY()
	TObjectPtr<UControlledDurationIndicatorComponent> ControlledDurationIndicatorComponent;

	// UPROPERTY so this reference doesn't leave a dangling pointer if the room is
	// ever garbage-collected while nothing else references it - same rationale
	// ARoomActor::OwnedEnemies (RoomActor.h:161-162) is UPROPERTY-marked.
	UPROPERTY()
	TObjectPtr<ARoomActor> OwningRoom;
};
