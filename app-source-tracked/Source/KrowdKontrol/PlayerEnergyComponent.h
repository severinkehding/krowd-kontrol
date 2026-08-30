#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlayerEnergyComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnergyChanged, float, NewEnergy);

// Tracks the player's energy (health) resource and enforces, by construction, that
// energy can only ever be reduced through ApplyContactDamage() (PRD 01 REQ-4: energy
// must only decrease from enemy contact, never from casting an ability). Each hit's
// raw damage is clamped to MaxDamagePerHit before being applied (REQ-6: enemy count,
// not per-hit damage, is the difficulty lever). See issue #78.
//
// ApplyContactDamage is the ONLY public method permitted to reduce CurrentEnergy - no
// other public mutator that can lower CurrentEnergy (setter, BlueprintCallable, or
// otherwise) may ever be added to this class. Doing so would reopen the exact hole
// this component exists to close. ApplyMaxEnergyBonus below is the sole, deliberate
// exception: it only ever raises CurrentEnergy (proportionally, alongside a MaxEnergy
// increase) and no-ops on any NewMaxEnergy that wouldn't raise the ceiling, so it
// cannot be used to work around the ApplyContactDamage-only-decrease rule above.
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class KROWDKONTROL_API UPlayerEnergyComponent : public UActorComponent
{
	GENERATED_BODY()

	// Grants the Automation Framework tests direct access to the private CurrentEnergy
	// field below, purely to seed deterministic starting values for each scenario.
	// Friendship isn't part of the public API, so this can't be used by gameplay code
	// to bypass ApplyContactDamage the way a public setter could. Used by this
	// component's own test, UEnergyMeterWidget's (issue #64, seeding a non-zero
	// CurrentEnergy without a live BeginPlay()), ABomberEnemy's (issue #15, same
	// reason, proving its explosion clamps through ApplyContactDamage), and
	// UPunishmentManagerComponent's (issue #177, driving CurrentEnergy to 0 to test
	// the "no energy change means no punishment trigger" edge case), and
	// ULevelFailComponent's (issue #171, seeding CurrentEnergy for a deterministic
	// single-call floor-to-0 to test the "OnLevelFailed fires exactly once" case), and
	// the level-restart flow's (issue #172, same deterministic floor-to-0 seed to
	// trigger OnLevelFailed -> HandleLevelFailed -> RequestLevelRestart), and the
	// boss-checkpoint restart flow's (issue #173, same seed, reused to drive the fail
	// path in both its no-checkpoint and checkpoint-reached cases).
	friend class FKrowdKontrolPlayerEnergyComponentTest;
	friend class FKrowdKontrolEnergyMeterWidgetTest;
	friend class FKrowdKontrolBomberEnemyTest;
	friend class FKrowdKontrolPunishmentManagerComponentTest;
	friend class FKrowdKontrolLevelFailedTest;
	friend class FKrowdKontrolLevelRestartTest;
	friend class FKrowdKontrolBossCheckpointRestartTest;

public:
	UPlayerEnergyComponent();

	// Ceiling CurrentEnergy is seeded to (on construction) and clamped to.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player Energy", meta = (ClampMin = "0.0"))
	float MaxEnergy = 100.0f;

	// Cap on how much a single ApplyContactDamage call can subtract, regardless of the
	// raw amount passed in.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player Energy", meta = (ClampMin = "0.0"))
	float MaxDamagePerHit = 10.0f;

	// Fires whenever ApplyContactDamage actually changes CurrentEnergy.
	// UEnergyMeterWidget::BindToEnergyComponent() (issue #64) is the first consumer;
	// nothing in a live game path calls that binding method yet.
	UPROPERTY(BlueprintAssignable, Category = "Player Energy")
	FOnEnergyChanged OnEnergyChanged;

	// The sole public mutator of CurrentEnergy. Clamps RawAmount to
	// [0, MaxDamagePerHit], subtracts, clamps CurrentEnergy to [0, MaxEnergy], and
	// broadcasts OnEnergyChanged when the value actually changes. Returns the actual
	// amount subtracted (post-clamp).
	UFUNCTION(BlueprintCallable, Category = "Player Energy")
	float ApplyContactDamage(float RawAmount, AActor* DamageSource);

	// Raises MaxEnergy to NewMaxEnergy and proportionally tops up CurrentEnergy so the
	// energy bar's fill fraction is preserved across the change (e.g. 60/100 ->
	// 75/125) - without this, a MaxEnergy bonus (e.g. a starter skill effect applied
	// at run start, KrowdKontrolPlayerController.cpp) would be invisible to the player
	// until they took contact damage back down past the old ceiling. No-ops if
	// NewMaxEnergy does not exceed the current MaxEnergy, so this can only ever raise
	// CurrentEnergy - never lower it - and does not reopen the
	// ApplyContactDamage-only-decrease invariant above.
	UFUNCTION(BlueprintCallable, Category = "Player Energy")
	void ApplyMaxEnergyBonus(float NewMaxEnergy);

	// Read-only accessor for CurrentEnergy - a future HUD can either bind
	// OnEnergyChanged or poll this. No corresponding setter exists on purpose.
	// BlueprintPure so this is reflected for MCP/Automation-driven checks too, not
	// just usable from C++/Blueprint HUD code.
	UFUNCTION(BlueprintPure, Category = "Player Energy")
	float GetCurrentEnergy() const { return CurrentEnergy; }

private:
	// Runtime state, not designer config - hence no EditDefaultsOnly/EditAnywhere, and
	// private (not BlueprintReadOnly) so no code path - Blueprint or C++ - can mutate
	// it except through ApplyContactDamage.
	UPROPERTY()
	float CurrentEnergy = 0.0f;
};
