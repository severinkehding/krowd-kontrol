#include "PanicOverloadStateTestListener.h"

void UPanicOverloadStateTestListener::HandlePanicOverloadStateChanged(EPanicOverloadState NewState)
{
	++CallCount;
	LastState = NewState;
}
