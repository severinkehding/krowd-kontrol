#pragma once

#include "CoreMinimal.h"

// Single source of truth for PRD 03 REQ-4's "elite variants introduced starting
// level 4-5": whether a given level number is eligible to include Elite-configured
// enemy spawns. No live level-progression subsystem exists yet (see
// AbilityUnlockComponent.h/OvercrowdDetectionComponent.h for the same caveat) - a
// caller (today, a test; later, whatever populates UWaveSpawnerComponent::Waves for
// a given level) supplies LevelIndex explicitly.
namespace EliteEligibility
{
	// Levels below this are never Elite-eligible; PRD 03 REQ-4's stated window is
	// "starting level 4-5" - MinEligibleLevel picks the lower bound of that range.
	KROWDKONTROL_API extern const int32 MinEligibleLevel;

	KROWDKONTROL_API bool IsEligibleAtLevel(int32 LevelIndex);
}
