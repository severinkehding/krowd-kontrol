#include "SniperShotFiredTestListener.h"

void USniperShotFiredTestListener::HandleSniperShotFired()
{
	++CallCount;
}
