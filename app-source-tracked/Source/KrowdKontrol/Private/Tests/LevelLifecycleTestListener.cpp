#include "LevelLifecycleTestListener.h"

void ULevelLifecycleTestListener::HandleLevelBegin(FName MapName)
{
	++LevelBeginCallCount;
	LastLevelBeginMapName = MapName;
}

void ULevelLifecycleTestListener::HandleLevelClear()
{
	++LevelClearCallCount;
}

void ULevelLifecycleTestListener::HandleRunComplete()
{
	++RunCompleteCallCount;
}
