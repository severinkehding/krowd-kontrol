#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AbilitySlot.h"
#include "AbilityLockoutComponent.generated.h"

class AEnemyBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAbilityLockoutChanged, EAbilitySlot, Ability, bool, bLocked);

// Real gameplay logic behind Punishment 1 (PRD "Punishment System (Punishments 1 & 2 +
// arbitration)" REQ-2, issue #178): locks the player's most recently successfully cast
// ability (Stun fallback if nothing has been cast yet this run) for a fixed duration
// whenever UPunishmentManagerComponent::OnPunishmentTriggered fires. Structurally a
// near-copy of UAbilityCooldownComponent - per-slot remaining-time state, tick-driven
// decrement, friend-class test hook - but deliberately a separate mechanism from
// cooldown (see AbilityCooldownComponent.h's own class comment for why), and it
// broadcasts OnAbilityLockoutChanged on activation/expiry transitions, unlike cooldown
// which is poll-only.
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class KROWDKONTROL_API UAbilityLockoutComponent : public UActorComponent
{
	GENERATED_BODY()

	// Grants the Automation Framework test direct access to AdvanceLockouts below,
	// which isn't part of the public API - mirrors
	// FKrowdKontrolAbilityCooldownTest's friendship on UAbilityCooldownComponent.
	friend class FKrowdKontrolAbilityLockoutComponentTest;

	// Grants the tray-widget binding test direct access to AdvanceLockouts so it can
	// drive lockout expiry without a real tick loop, proving the tray's locked-slot
	// state mirrors the lockout state through activation and expiry.
	friend class FKrowdKontrolAbilityCooldownTrayWidgetTest;

public:
	UAbilityLockoutComponent();

	static constexpr int32 NumAbilitySlots = static_cast<int32>(EAbilitySlot::Count);

	// Fixed lockout duration applied regardless of which ability gets locked - per the
	// issue's Notes, distinct from and longer than
	// UAbilityCooldownComponent::DefaultAbilityCooldownSeconds (3.0f). Per-ability
	// duration tuning is explicitly out of this issue's scope.
	static constexpr float DefaultLockoutDurationSeconds = 8.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability Lockout", meta = (ClampMin = "0.0"))
	float LockoutDurationSeconds = DefaultLockoutDurationSeconds;

	// Fires exactly once per true state transition - (Ability, true) when a lockout
	// starts, (Ability, false) when it expires. A re-trigger while a slot is already
	// locked silently refreshes the timer without a redundant broadcast.
	UPROPERTY(BlueprintAssignable, Category = "Ability Lockout")
	FOnAbilityLockoutChanged OnAbilityLockoutChanged;

	// Reflected runtime state (pass-1 E2E fix, issue #180) - mirrors "is any ability
	// slot currently locked". Kept in sync from StartLockout/AdvanceLockouts/
	// EndAllLockouts rather than recomputed on demand, so tools that can only read
	// UPROPERTY state directly (not invoke a BlueprintPure function that takes a
	// parameter, e.g. an E2E behavioral holdout) can still observe lockout activity.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ability Lockout")
	bool bIsLockoutActive = false;

	UFUNCTION(BlueprintPure, Category = "Ability Lockout")
	bool IsAbilityLocked(EAbilitySlot Ability) const;

	UFUNCTION(BlueprintPure, Category = "Ability Lockout")
	float GetRemainingLockoutSeconds(EAbilitySlot Ability) const;

	// Immediately clears every currently-locked slot, broadcasting
	// OnAbilityLockoutChanged(Slot, false) for each one that actually transitions -
	// used by UPunishmentArbitrationComponent (issue #180) to revert this punishment's
	// effects in full the instant a higher-priority one (Overcrowd) preempts it. Unlike
	// AdvanceLockouts(), this is instant and unconditional, not timer-driven.
	void EndAllLockouts();

	// Bound to UAbilityCastComponent::OnAbilityCastApplied - records the most recently
	// successfully cast ability so a subsequent punishment trigger knows what to lock.
	// TargetEnemy is unused, matches the delegate's signature only.
	UFUNCTION()
	void HandleAbilityCastApplied(EAbilitySlot Ability, AEnemyBase* TargetEnemy);

	// Bound to UPunishmentManagerComponent::OnPunishmentTriggered - locks
	// LastCastAbility (Stun if nothing cast yet this run) for LockoutDurationSeconds.
	UFUNCTION()
	void HandlePunishmentTriggered();

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Decrements every locked slot's remaining time. Called every frame from
	// TickComponent() in real gameplay, and directly by the Automation test (via
	// friend, since it can't drive a live tick loop under the -nullrhi headless run).
	// Broadcasts OnAbilityLockoutChanged(Slot, false) exactly on the >0 -> <=0
	// transition within this call, not for slots already expired before this call and
	// not repeatedly on every subsequent tick once a slot is already expired.
	void AdvanceLockouts(float DeltaSeconds);

private:
	// Sets Ability's remaining lockout time to LockoutDurationSeconds, broadcasting
	// OnAbilityLockoutChanged(Ability, true) only on the <=0 -> >0 transition, so a
	// re-trigger while already locked refreshes the timer without a redundant
	// broadcast.
	void StartLockout(EAbilitySlot Ability);

	// Recomputes bIsLockoutActive from RemainingLockoutSeconds - called after every
	// state-changing operation (StartLockout/AdvanceLockouts/EndAllLockouts) so the
	// reflected property never drifts from the real per-slot state it mirrors.
	void RefreshIsLockoutActive();

	// Runtime state, not designer config - hence no EditDefaultsOnly/EditAnywhere,
	// private so no code path - Blueprint or C++ - can mutate it except through
	// StartLockout()/AdvanceLockouts(). A TArray per slot (not a single scalar) since
	// two triggers in quick succession could target two different most-recently-cast
	// abilities, and both lockouts must expire independently - see this component's
	// own .cpp for the fuller rationale.
	UPROPERTY()
	TArray<float> RemainingLockoutSeconds;

	EAbilitySlot LastCastAbility = EAbilitySlot::Stun;
};
