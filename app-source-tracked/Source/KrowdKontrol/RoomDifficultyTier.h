#pragma once

#include "CoreMinimal.h"
#include "RoomDifficultyTier.generated.h"

// A room's difficulty tier for the future room-pool shuffler (issue #49, PRD 05
// REQ-4). No prior difficulty-tier concept exists in MISSION.md or the codebase -
// MISSION.md's ramp today is only "room count and enemy density" (line 58). Three
// tiers is enough for a shuffler to sequence by without inventing unused granularity;
// revisit if a later issue needs finer control. Own file, matching EnemyType.h/
// AbilitySlot.h, since a future shuffler issue needs only this enum, not the whole
// RoomMetadataComponent.h.
UENUM(BlueprintType)
enum class ERoomDifficultyTier : uint8
{
	Easy,
	Medium,
	Hard
};
