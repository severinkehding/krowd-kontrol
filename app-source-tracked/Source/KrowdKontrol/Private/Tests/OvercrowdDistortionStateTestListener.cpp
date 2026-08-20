#include "OvercrowdDistortionStateTestListener.h"

void UOvercrowdDistortionStateTestListener::HandleOvercrowdVisualDistortionStateChanged(EOvercrowdVisualDistortionState NewState)
{
	++CallCount;
	LastState = NewState;
}
