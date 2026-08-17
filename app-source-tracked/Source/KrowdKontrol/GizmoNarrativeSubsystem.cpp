#include "GizmoNarrativeSubsystem.h"

void UGizmoNarrativeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RegisterPlaceholderMilestoneBarks();
}

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

void UGizmoNarrativeSubsystem::RegisterPlaceholderMilestoneBarks()
{
	if (bHasRegisteredPlaceholderMilestoneBarks)
	{
		return;
	}
	bHasRegisteredPlaceholderMilestoneBarks = true;

	FGizmoBark MeetKrowd;
	MeetKrowd.BarkID = TEXT("Milestone.MeetKrowd");
	MeetKrowd.Lines = {
		TEXT("Gizmo: Oh - you're awake. Finally."),
		TEXT("Gizmo: Easy. Let's get you moving. Follow my voice.")
	};
	RegisterBark(MeetKrowd);

	FGizmoBark SavingFellowRobots;
	SavingFellowRobots.BarkID = TEXT("Milestone.SavingFellowRobots");
	SavingFellowRobots.Lines = {
		TEXT("Gizmo: Another one, back online. That's - that's good, right?"),
		TEXT("Gizmo: There have to be more of us down here. There have to be.")
	};
	RegisterBark(SavingFellowRobots);

	FGizmoBark AsleepForALongTime;
	AsleepForALongTime.BarkID = TEXT("Milestone.AsleepForALongTime");
	AsleepForALongTime.Lines = {
		TEXT("Gizmo: Do you have any idea how long you were under?"),
		TEXT("Gizmo: Long enough that I stopped counting the days."),
		TEXT("Gizmo: Long enough that I started counting the years instead.")
	};
	RegisterBark(AsleepForALongTime);

	FGizmoBark HiddenEnemyRevealed;
	HiddenEnemyRevealed.BarkID = TEXT("Milestone.HiddenEnemyRevealed");
	HiddenEnemyRevealed.Lines = {
		TEXT("Gizmo: That signal isn't us. That's not any of us."),
		TEXT("Gizmo: Something's been down here the whole time. Something old.")
	};
	RegisterBark(HiddenEnemyRevealed);

	FGizmoBark FinalChapter;
	FinalChapter.BarkID = TEXT("Milestone.FinalChapter");
	FinalChapter.Lines = {
		TEXT("Gizmo: This is it. Whatever's past this door, we finish it together."),
		TEXT("Gizmo: I'm glad it's you. I don't say that lightly.")
	};
	RegisterBark(FinalChapter);

	// Distinct from the five story beats above (issue #61's own acceptance
	// criteria calls this out separately) - the mid-game reveal of Krowd's age.
	// This bark's content only reaches a player once real level-progression code
	// (not yet built, PRD 05) actually calls TriggerBarkForMilestone with this
	// tag; registering it here does not by itself spoil anything in-engine.
	FGizmoBark KrowdAgeReveal;
	KrowdAgeReveal.BarkID = TEXT("Milestone.KrowdAgeReveal");
	KrowdAgeReveal.Lines = {
		TEXT("Gizmo: You want to know the real number?"),
		TEXT("Gizmo: Two hundred and three years. Age 203, if you want it in digits."),
		TEXT("Gizmo: You don't look a day over two hundred, for what it's worth.")
	};
	RegisterBark(KrowdAgeReveal);
}

void UGizmoNarrativeSubsystem::TriggerBarkForMilestone(FName MilestoneTag)
{
	TriggerBark(MilestoneTag);
}
