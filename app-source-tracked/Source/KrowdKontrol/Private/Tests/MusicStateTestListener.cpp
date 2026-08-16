#include "MusicStateTestListener.h"

void UMusicStateTestListener::HandleMusicStateChanged(EMusicState NewState)
{
	++CallCount;
	LastState = NewState;
}
