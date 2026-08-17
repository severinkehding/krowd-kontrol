#include "OvercrowdMuffleStateTestListener.h"

void UOvercrowdMuffleStateTestListener::HandleOvercrowdAudioMuffleStateChanged(EOvercrowdAudioMuffleState NewState)
{
	++CallCount;
	LastState = NewState;
}
