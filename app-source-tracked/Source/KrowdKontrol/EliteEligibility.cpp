#include "EliteEligibility.h"

const int32 EliteEligibility::MinEligibleLevel = 4;

bool EliteEligibility::IsEligibleAtLevel(int32 LevelIndex)
{
	return LevelIndex >= MinEligibleLevel;
}
