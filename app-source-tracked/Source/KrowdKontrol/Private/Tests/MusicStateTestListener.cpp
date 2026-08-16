#include "MusicStateTestListener.h"
#include "MusicSubsystem.h"

void UMusicStateTestListener::HandleMusicStateChanged(EMusicState NewState)
{
	++CallCount;
	LastState = NewState;
	if (WatchedSubsystem)
	{
		ObservedStateDuringBroadcast = WatchedSubsystem->GetMusicState();
	}
}
