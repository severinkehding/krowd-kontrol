#include "LevelLifecycleTestListener.h"

void ULevelLifecycleTestListener::HandleLevelBegin(FName MapName)
{
	++LevelBeginCallCount;
	LastLevelBeginMapName = MapName;
}

void ULevelLifecycleTestListener::HandleLevelClear()
{
	++LevelClearCallCount;
	CallOrder.Add(TEXT("LevelClear"));
}

void ULevelLifecycleTestListener::HandleRunComplete()
{
	++RunCompleteCallCount;
	CallOrder.Add(TEXT("RunComplete"));
}
