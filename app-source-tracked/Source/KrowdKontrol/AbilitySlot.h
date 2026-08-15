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
	Snare
};
