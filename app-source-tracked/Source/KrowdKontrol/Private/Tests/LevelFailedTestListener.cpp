#include "LevelFailedTestListener.h"

void ULevelFailedTestListener::HandleLevelFailed()
{
	++CallCount;
}
