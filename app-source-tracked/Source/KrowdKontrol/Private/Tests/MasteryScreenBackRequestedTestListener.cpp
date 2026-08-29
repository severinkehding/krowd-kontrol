#include "MasteryScreenBackRequestedTestListener.h"

void UMasteryScreenBackRequestedTestListener::HandleBackRequested()
{
	++CallCount;
}
