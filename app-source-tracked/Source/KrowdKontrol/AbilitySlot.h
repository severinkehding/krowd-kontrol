#pragma once

#include "CoreMinimal.h"
#include "AbilitySlot.generated.h"

// Matches the issue's exact ordering (Stun, Sleep, Root, Fear, Snare). Backs both
// UAbilityCooldownTrayWidget (issue #66/#91) and UAbilityCooldownComponent (issue #71).
UENUM(BlueprintType)
enum class EAbilitySlot : uint8
{
	Stun,
	Sleep,
	Root,
	Fear,
	Snare,

	// Sentinel, not a real slot - lets NumAbilitySlots constants derive their count
	// from this enum instead of hand-maintaining a separate "= 5" elsewhere. Hidden so
	// it never shows up in a Blueprint dropdown.
	Count UMETA(Hidden)
};
