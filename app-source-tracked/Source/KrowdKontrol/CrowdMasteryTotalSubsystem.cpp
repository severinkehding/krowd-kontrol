#include "CrowdMasteryTotalSubsystem.h"

void UCrowdMasteryTotalSubsystem::DepositRunMastery(int32 RunMasteryValue)
{
	AccumulatedTotal += FMath::Max(0, RunMasteryValue);
}

void UCrowdMasteryTotalSubsystem::ResetAccumulatedTotal()
{
	AccumulatedTotal = 0;
}
