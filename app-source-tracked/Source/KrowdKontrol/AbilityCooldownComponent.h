#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AbilitySlot.h"
#include "AbilityCooldownComponent.generated.h"

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
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class KROWDKONTROL_API UAbilityCooldownComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAbilityCooldownComponent();

	static constexpr int32 NumAbilitySlots = 5;

	// Placeholder cooldown duration, in seconds, seeded into AbilityCooldownDurations at
	// construction - not a locked design value, per the issue's Notes real per-ability
	// balance is a future playtesting decision.
	static constexpr float DefaultAbilityCooldownSeconds = 3.0f;

	// One entry per EAbilitySlot, independently re-tunable per slot in the editor
	// without a code change.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability Cooldown")
	TArray<float> AbilityCooldownDurations;

	// The sole legal way to start a cooldown. Returns true and starts the timer if the
	// slot was not already on cooldown; returns false and changes nothing if it was -
	// this IS the "prevents recasting" enforcement point a future cast system gates its
	// actual cast on.
	UFUNCTION(BlueprintCallable, Category = "Ability Cooldown")
	bool TryStartCooldown(EAbilitySlot AbilitySlot);

	// Decrements every slot's remaining time. Called every frame from TickComponent()
	// in real gameplay, and directly by the Automation test (which can't drive a live
	// tick loop under the -nullrhi headless run).
	UFUNCTION(BlueprintCallable, Category = "Ability Cooldown")
	void AdvanceCooldowns(float DeltaSeconds);

	UFUNCTION(BlueprintPure, Category = "Ability Cooldown")
	bool IsOnCooldown(EAbilitySlot AbilitySlot) const;

	UFUNCTION(BlueprintPure, Category = "Ability Cooldown")
	float GetRemainingCooldownSeconds(EAbilitySlot AbilitySlot) const;

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	// Runtime state, not designer config - hence no EditDefaultsOnly/EditAnywhere, and
	// private so no code path - Blueprint or C++ - can mutate it except through
	// TryStartCooldown()/AdvanceCooldowns().
	UPROPERTY()
	TArray<float> RemainingCooldownSeconds;
};
