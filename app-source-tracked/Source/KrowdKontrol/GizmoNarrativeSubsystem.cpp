#include "GizmoNarrativeSubsystem.h"

void UGizmoNarrativeSubsystem::RegisterBark(const FGizmoBark& BarkDefinition)
{
	RegisteredBarks.Add(BarkDefinition.BarkID, BarkDefinition);
}

void UGizmoNarrativeSubsystem::TriggerBark(FName BarkID)
{
	FGizmoBark* Bark = RegisteredBarks.Find(BarkID);
	if (!Bark)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UGizmoNarrativeSubsystem::TriggerBark: unknown bark ID '%s' - no-op."),
			*BarkID.ToString());
		return;
	}

	if (Bark->bHasBeenTriggered)
	{
		return;
	}

	Bark->bHasBeenTriggered = true;
	OnBarkTriggered.Broadcast(Bark->BarkID, Bark->Lines);
}

bool UGizmoNarrativeSubsystem::HasBarkFired(FName BarkID) const
{
	const FGizmoBark* Bark = RegisteredBarks.Find(BarkID);
	return Bark && Bark->bHasBeenTriggered;
}
