#include "GizmoBarkTestListener.h"
#include "GizmoNarrativeSubsystem.h"

void UGizmoBarkTestListener::HandleBarkTriggered(FName BarkID, TArray<FString> Lines)
{
	++CallCount;
	LastBarkID = BarkID;
	LastLines = Lines;

	if (SubsystemToReenter)
	{
		SubsystemToReenter->TriggerBark(ReentrantBarkID);
	}
}
