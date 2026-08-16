#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbilitySlot.h"
#include "EnemyBase.generated.h"

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
class KROWDKONTROL_API AEnemyBase : public AActor
{
	GENERATED_BODY()

	// Grants the Automation Framework tests direct access to TickCheckDetection
	// below, so a headless test can drive deterministic proximity checks without a
	// real per-frame Tick() loop - same rationale UAbilityCooldownComponent's
	// FKrowdKontrolAbilityCooldownTest friendship documents. Friendship isn't
	// inherited, so FKrowdKontrolSniperEnemyTest needs its own grant here (not just
	// on ASniperEnemy) to drive a concrete subclass instance through Idle->Alert->
	// Attack the same deterministic way.
	friend class FKrowdKontrolEnemyBaseTest;
	friend class FKrowdKontrolSniperEnemyTest;

public:
	AEnemyBase();

	// Fires exactly once, on the transition into Banked.
	UPROPERTY(BlueprintAssignable, Category = "Enemy")
	FOnEnemyBanked OnEnemyBanked;

	EEnemyState GetEnemyState() const { return CurrentState; }

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
	void AdvanceToAlert();
	void AdvanceToAttack();

	EEnemyState CurrentState = EEnemyState::Idle;
};
