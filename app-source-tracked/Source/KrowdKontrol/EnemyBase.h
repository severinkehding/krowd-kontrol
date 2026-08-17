#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbilitySlot.h"
#include "ThreatState.h"
#include "EnemyBase.generated.h"

class UPlayerEnergyComponent;

// Idle -> Alert -> Attack -> Controlled -> Banked, with Banked as the only reachable
// "defeated" state. Transition table (no other edges exist):
//   Idle -> Alert: player enters DetectionRangeUnits (proximity check, no perception
//     system, no pathfinding).
//   Alert -> Attack: player enters GetAttackRangeUnits() (overridable per concrete
//     enemy type).
//   Alert/Attack -> Controlled: ReceiveControl(EAbilitySlot) is called.
//   Controlled -> Banked: TransitionToBanked() is called.
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
	friend class FKrowdKontrolMusicSubsystemTest;
	friend class FKrowdKontrolOvercrowdDetectionComponentTest;

public:
	AEnemyBase();

	// Fires exactly once, on the transition into Banked.
	UPROPERTY(BlueprintAssignable, Category = "Enemy")
	FOnEnemyBanked OnEnemyBanked;

	EEnemyState GetEnemyState() const { return CurrentState; }

	// IThreatState (issue #25): Alert/Attack/Controlled all read as "Hot" - any state
	// where the enemy is actively engaged, not just mid-attack. Idle and Banked both
	// read as "Idle" - not yet aggroed, or pacified. See ThreatState.h.
	virtual EThreatState GetThreatState() const override;

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

protected:
	virtual void Tick(float DeltaTime) override;

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

	EEnemyState CurrentState = EEnemyState::Idle;
};
