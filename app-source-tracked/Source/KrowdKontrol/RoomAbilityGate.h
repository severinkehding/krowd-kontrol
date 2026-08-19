#pragma once

#include "CoreMinimal.h"
#include "AbilitySlot.h"
#include "RoomAbilityGate.generated.h"

// Whether a room requires a specific unlocked ability to clear, and if so which one
// (issue #49, PRD 05 REQ-4). Not a reuse of EAbilitySlot (AbilitySlot.h) - that enum
// has no None value, and AbilityCooldownComponent/AbilityCooldownTrayWidget/
// AbilityUnlockComponent all derive their sizing from its hidden Count sentinel, so
// inserting a None there would risk shifting that. Same 5-ability order as
// EAbilitySlot (Stun, Sleep, Root, Fear, Snare), with None first.
UENUM(BlueprintType)
enum class ERoomAbilityGate : uint8
{
	None,
	Stun,
	Sleep,
	Root,
	Fear,
	Snare
};

// Enforces the shared-order promise the comment above makes: RoomPoolShufflerComponent
// maps ERoomAbilityGate -> EAbilitySlot via a -1 offset cast (issue #53, PRD 05 REQ-5),
// which is only correct as long as both enums keep this exact relative order. A future
// edit to either enum that breaks this will fail the build here instead of silently
// misgating rooms at runtime.
static_assert(static_cast<uint8>(ERoomAbilityGate::Stun)  - 1 == static_cast<uint8>(EAbilitySlot::Stun),  "ERoomAbilityGate/EAbilitySlot order drift: Stun");
static_assert(static_cast<uint8>(ERoomAbilityGate::Sleep) - 1 == static_cast<uint8>(EAbilitySlot::Sleep), "ERoomAbilityGate/EAbilitySlot order drift: Sleep");
static_assert(static_cast<uint8>(ERoomAbilityGate::Root)  - 1 == static_cast<uint8>(EAbilitySlot::Root),  "ERoomAbilityGate/EAbilitySlot order drift: Root");
static_assert(static_cast<uint8>(ERoomAbilityGate::Fear)  - 1 == static_cast<uint8>(EAbilitySlot::Fear),  "ERoomAbilityGate/EAbilitySlot order drift: Fear");
static_assert(static_cast<uint8>(ERoomAbilityGate::Snare) - 1 == static_cast<uint8>(EAbilitySlot::Snare), "ERoomAbilityGate/EAbilitySlot order drift: Snare");
