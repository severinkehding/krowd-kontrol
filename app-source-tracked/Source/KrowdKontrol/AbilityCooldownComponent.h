#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AbilitySlot.h"
#include "AbilityCooldownComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAbilityCooldownChanged, EAbilitySlot, Ability, bool, bOnCooldown);

// Gives each of the 5 crowd-control abilities (PRD 02 REQ-5) an independent, short
// cooldown timer, entirely separate from PRD 08's not-yet-built Punishment 1 (ability
// lockout) - IsOnCooldown()/GetRemainingCooldownSeconds() are cooldown-only queries,
// deliberately not a shared "unavailable" flag, so a future lockout mechanic can add
// its own structurally distinct state without ever touching this component. See issue
// #71.
//
// TryStartCooldown is the ONLY public method permitted to start a cooldown - no other
// public mutator (setter, BlueprintCallable, or otherwise) may ever be added to this
// class. Doing so would reopen the exact hole this component exists to close.
// OnAbilityCooldownChanged (issue #259) does not relax this - a BlueprintAssignable
// delegate property is an event subscription point, not a mutator; it can only be
// broadcast from inside TryStartCooldown/AdvanceCooldowns, the same two functions that
// already own all state changes.
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class KROWDKONTROL_API UAbilityCooldownComponent : public UActorComponent
{
	GENERATED_BODY()

	// Grants the Automation Framework test direct access to AdvanceCooldowns below,
	// which isn't part of the public API - see the comment on AdvanceCooldowns for why.
	friend class FKrowdKontrolAbilityCooldownTest;

	// Grants the tray-widget binding test direct access to AdvanceCooldowns so it can
	// drive cooldown expiry without a real tick loop, proving the tray's fill/readout
	// mirrors the real component's state through activation and expiry (issue #259).
	friend class FKrowdKontrolAbilityCooldownTrayWidgetTest;

public:
	UAbilityCooldownComponent();

	static constexpr int32 NumAbilitySlots = static_cast<int32>(EAbilitySlot::Count);

	// Placeholder cooldown duration, in seconds, seeded into AbilityCooldownDurations at
	// construction - not a locked design value, per the issue's Notes real per-ability
	// balance is a future playtesting decision.
	static constexpr float DefaultAbilityCooldownSeconds = 3.0f;

	// One entry per EAbilitySlot, independently re-tunable per slot in the editor
	// without a code change.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability Cooldown")
	TArray<float> AbilityCooldownDurations;

	// Fires exactly once per true state transition - (Ability, true) when a cooldown
	// starts, (Ability, false) when it expires. A blocked recast (already on cooldown)
	// or an already-expired advance never re-broadcasts. Added for issue #259 so
	// UAbilityCooldownTrayWidget can bind directly to real cooldown state instead of
	// running its own placeholder timer - mirrors UAbilityLockoutComponent's identical
	// OnAbilityLockoutChanged.
	UPROPERTY(BlueprintAssignable, Category = "Ability Cooldown")
	FOnAbilityCooldownChanged OnAbilityCooldownChanged;

	// The sole legal way to start a cooldown. Returns true and starts the timer if the
	// slot was not already on cooldown; returns false and changes nothing if it was -
	// this IS the "prevents recasting" enforcement point a future cast system gates its
	// actual cast on.
	UFUNCTION(BlueprintCallable, Category = "Ability Cooldown")
	bool TryStartCooldown(EAbilitySlot AbilitySlot);

	UFUNCTION(BlueprintPure, Category = "Ability Cooldown")
	bool IsOnCooldown(EAbilitySlot AbilitySlot) const;

	UFUNCTION(BlueprintPure, Category = "Ability Cooldown")
	float GetRemainingCooldownSeconds(EAbilitySlot AbilitySlot) const;

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Decrements every slot's remaining time. Called every frame from TickComponent()
	// in real gameplay, and directly by the Automation test (via friend, since it can't
	// drive a live tick loop under the -nullrhi headless run). Deliberately not public
	// or BlueprintCallable - TryStartCooldown must stay the only way a Blueprint graph
	// can influence cooldown state, otherwise any graph could call
	// AdvanceCooldowns(9999.f) to clear a cooldown early. DeltaSeconds must be
	// non-negative; TickComponent's DeltaTime always is, and the only other caller
	// (the test) respects that too.
	void AdvanceCooldowns(float DeltaSeconds);

private:
	// Runtime state, not designer config - hence no EditDefaultsOnly/EditAnywhere, and
	// private so no code path - Blueprint or C++ - can mutate it except through
	// TryStartCooldown()/AdvanceCooldowns().
	UPROPERTY()
	TArray<float> RemainingCooldownSeconds;
};
