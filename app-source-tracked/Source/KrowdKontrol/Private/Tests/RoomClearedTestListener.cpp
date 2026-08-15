#include "RoomClearedTestListener.h"

void URoomClearedTestListener::HandleRoomCleared()
{
	++CallCount;
}
