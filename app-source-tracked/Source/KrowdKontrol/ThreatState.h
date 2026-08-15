#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ThreatState.generated.h"

// Whether an enemy is actively draining the player's energy (Hot) or not yet
// aggroed (Idle) - the survival-relevant signal PRD 01 REQ-1 requires be legible
// at a glance. No enemy AI state machine exists yet (see MISSION.md's
// Idle->Alert->Attack->Controlled->Banked state machine, `03`); this is the
// minimal data contract future AI writes to and future HUD work reads from. See
// issue #81.
UENUM(BlueprintType)
enum class EThreatState : uint8
{
	Idle,
	Hot
};

// Data contract only (issue #81) - no AI, animation, or HUD rendering logic here.
// Future enemy AI is expected to implement this and flip state on Alert/Attack
// entry; future HUD work is expected to read it via GetThreatState(). C++-only
// (not Blueprintable) until a real Blueprint consumer exists to inform whether
// GetThreatState() should become a BlueprintNativeEvent instead.
UINTERFACE(MinimalAPI)
class UThreatState : public UInterface
{
	GENERATED_BODY()
};

class KROWDKONTROL_API IThreatState
{
	GENERATED_BODY()

public:
	virtual EThreatState GetThreatState() const = 0;
};
