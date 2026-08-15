#include "ThreatStateTestActor.h"

void AThreatStateTestActor::SetThreatState(EThreatState NewState)
{
	CurrentThreatState = NewState;
}

EThreatState AThreatStateTestActor::GetThreatState() const
{
	return CurrentThreatState;
}
