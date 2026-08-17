#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AbilitySlot.h"
#include "AbilityUnlockComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAbilityUnlocked, EAbilitySlot, Ability);

// Tracks, per run, which of the 5 crowd-control abilities (EAbilitySlot) are unlocked
// (issue #69, resolving the level-count/unlock-order ambiguity per MISSION.md's
// operator decision 2026-08-17 locking the Alpha at 5 hand-authored levels). A new run
// starts with only Stun unlocked; NotifyLevelReached() is the sole entry point that
// unlocks Sleep/Root/Fear/Snare, one per level, as levels 2-5 are reached.
//
// Deliberately does not wire into UAbilityCooldownComponent or
// UAbilityCooldownTrayWidget - both already reserve "is this ability available"
// gating for a separate, later mechanic (see AbilityCooldownComponent.h, issue #71).
// IsAbilityUnlocked() is BlueprintPure so that wiring can consume this component's
// state later without any further C++ change here.
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class KROWDKONTROL_API UAbilityUnlockComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAbilityUnlockComponent();

	static constexpr int32 NumAbilitySlots = static_cast<int32>(EAbilitySlot::Count);

	// Fires exactly once per ability, the first time NotifyLevelReached() reaches that
	// ability's mapped level. Broadcast order tracks call order, not a fixed internal
	// sequence - callers invoking NotifyLevelReached with ascending level values (2, 3,
	// 4, 5) will see Sleep, Root, Fear, Snare in that order, but nothing here enforces
	// ascending calls.
	UPROPERTY(BlueprintAssignable, Category = "Ability Unlock")
	FOnAbilityUnlocked OnAbilityUnlocked;

	UFUNCTION(BlueprintPure, Category = "Ability Unlock")
	bool IsAbilityUnlocked(EAbilitySlot Ability) const;

	// Explicit level-progression signal a caller (today, a test double; later, a real
	// level-progression subsystem - PRD 05/03 don't expose one yet, per issue #69's
	// Notes) invokes once per level reached. Idempotent per level: unlocking the
	// ability mapped to LevelIndex only happens once, repeat calls are no-ops. Level 1
	// (Stun, already unlocked at construction) is a silent no-op - a real
	// level-progression signal firing once per level transition will naturally include
	// level 1 as an expected call. Any other value outside the 2-5 mapped range (0,
	// negative, or > 5) is still a no-op but logs a warning, since those aren't
	// expected from a real level-progression signal.
	UFUNCTION(BlueprintCallable, Category = "Ability Unlock")
	void NotifyLevelReached(int32 LevelIndex);

private:
	void UnlockAbility(EAbilitySlot Ability);

	// Runtime state, not designer config - hence no EditDefaultsOnly/EditAnywhere, and
	// private so no code path - Blueprint or C++ - can mutate it except through
	// NotifyLevelReached().
	UPROPERTY()
	TArray<bool> SlotUnlocked;
};
