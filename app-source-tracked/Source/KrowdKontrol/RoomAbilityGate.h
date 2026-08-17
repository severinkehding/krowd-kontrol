#pragma once

#include "CoreMinimal.h"
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
