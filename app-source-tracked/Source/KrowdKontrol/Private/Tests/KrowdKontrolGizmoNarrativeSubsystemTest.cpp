// Confirms UGizmoNarrativeSubsystem (issue #57, PRD 07) is the reusable trigger point
// for one-sided Gizmo remote-call barks: a known bark ID broadcasts its lines exactly
// once, a second trigger of the same ID is a silent no-op, and triggering an unknown
// ID logs a warning and no-ops rather than crashing.
//
// No UWorld/CreateNewMap() needed - RegisterBark/TriggerBark/HasBarkFired never call
// GetWorld() or GetGameInstance(), so the subsystem is constructed directly via
// NewObject<>(), matching KrowdKontrolAbilityDataTest.cpp's "no engine-object
// dependency" precedent. UGameInstanceSubsystem is UCLASS(Within = GameInstance),
// so NewObject<> still needs a UGameInstance Outer (a bare NewObject<>() defaults to
// the transient package and fails Outer-class validation) - a plain NewObject<UGameInstance>()
// satisfies that without needing a real engine-started game instance.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "GizmoNarrativeSubsystem.h"
#include "GizmoBarkTestListener.h"
#include "Engine/GameInstance.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolGizmoNarrativeSubsystemTest,
	"KrowdKontrol.Unit.GizmoNarrativeSubsystem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolGizmoNarrativeSubsystemTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstanceOuter = NewObject<UGameInstance>();
	UGizmoNarrativeSubsystem* Subsystem = NewObject<UGizmoNarrativeSubsystem>(GameInstanceOuter);
	if (!TestNotNull(TEXT("UGizmoNarrativeSubsystem should construct"), Subsystem))
	{
		return false;
	}

	UGizmoBarkTestListener* Listener = NewObject<UGizmoBarkTestListener>();
	Subsystem->OnBarkTriggered.AddDynamic(Listener, &UGizmoBarkTestListener::HandleBarkTriggered);

	const FName KnownID = TEXT("TestBark.FirstContact");
	FGizmoBark Bark;
	Bark.BarkID = KnownID;
	Bark.Lines = { TEXT("Line one."), TEXT("Line two.") };
	Subsystem->RegisterBark(Bark);

	TestFalse(TEXT("HasBarkFired should be false for a registered-but-not-yet-triggered bark"),
		Subsystem->HasBarkFired(KnownID));

	// A second, distinct bark registered alongside the first - stays untouched by
	// everything the first bark's ID does below.
	const FName SecondID = TEXT("TestBark.SecondContact");
	FGizmoBark SecondBark;
	SecondBark.BarkID = SecondID;
	SecondBark.Lines = { TEXT("Should never fire in this test.") };
	Subsystem->RegisterBark(SecondBark);

	// (1) Triggering a known, unfired bark ID broadcasts once with the registered text
	// and marks it as fired.
	Subsystem->TriggerBark(KnownID);
	TestEqual(TEXT("CallCount should be 1 after the first trigger"), Listener->CallCount, 1);
	TestEqual(TEXT("LastBarkID should be the triggered bark's ID"), Listener->LastBarkID, KnownID);
	TestEqual(TEXT("LastLines should match the registered lines"), Listener->LastLines, Bark.Lines);
	TestTrue(TEXT("HasBarkFired should be true after triggering"), Subsystem->HasBarkFired(KnownID));
	TestFalse(TEXT("A second, untriggered bark must not be affected by triggering the first"),
		Subsystem->HasBarkFired(SecondID));

	// (2) Triggering the same bark ID a second time must not re-broadcast.
	Subsystem->TriggerBark(KnownID);
	TestEqual(TEXT("CallCount should still be 1 after a second trigger of the same ID"), Listener->CallCount, 1);

	// (3) Triggering an unregistered bark ID logs a warning, does not crash, and does
	// not broadcast.
	const FName UnknownID = TEXT("TestBark.NeverRegistered");
	AddExpectedError(TEXT("unknown bark ID"), EAutomationExpectedErrorFlags::Contains, 1);
	Subsystem->TriggerBark(UnknownID);
	TestEqual(TEXT("CallCount should still be 1 after triggering an unknown ID"), Listener->CallCount, 1);
	TestFalse(TEXT("HasBarkFired should be false for an unknown ID"), Subsystem->HasBarkFired(UnknownID));

	// (4) Regression: a listener that re-enters TriggerBark on the same ID from inside
	// the broadcast must not re-fire. This pins down TriggerBark's flip-before-broadcast
	// ordering (bHasBeenTriggered = true precedes OnBarkTriggered.Broadcast) - the
	// property that makes same-ID re-entrancy safe instead of unbounded recursion.
	const FName ReentrantID = TEXT("TestBark.Reentrant");
	FGizmoBark ReentrantBark;
	ReentrantBark.BarkID = ReentrantID;
	ReentrantBark.Lines = { TEXT("Only fires once even if re-entered.") };
	Subsystem->RegisterBark(ReentrantBark);

	UGizmoBarkTestListener* ReentrantListener = NewObject<UGizmoBarkTestListener>();
	ReentrantListener->SubsystemToReenter = Subsystem;
	ReentrantListener->ReentrantBarkID = ReentrantID;
	Subsystem->OnBarkTriggered.AddDynamic(ReentrantListener, &UGizmoBarkTestListener::HandleBarkTriggered);

	Subsystem->TriggerBark(ReentrantID);
	TestEqual(TEXT("Re-entrant TriggerBark on the same ID during broadcast must not re-fire"),
		ReentrantListener->CallCount, 1);

	// (5) TriggerBarkForMilestone (issue #61): NewObject<>() construction never runs
	// the engine's subsystem-collection Initialize() lifecycle, so this test drives
	// RegisterPlaceholderMilestoneBarks() directly, the same way real engine-booted
	// instances get it from Initialize().
	Subsystem->RegisterPlaceholderMilestoneBarks();

	// Detach the earlier sections' listeners so their bindings can't silently observe
	// milestone broadcasts - ReentrantListener in particular would otherwise re-invoke
	// TriggerBark(ReentrantID) on every trigger below.
	Subsystem->OnBarkTriggered.RemoveDynamic(Listener, &UGizmoBarkTestListener::HandleBarkTriggered);
	Subsystem->OnBarkTriggered.RemoveDynamic(ReentrantListener, &UGizmoBarkTestListener::HandleBarkTriggered);

	// A fresh listener keeps this section's CallCount self-contained, since Listener
	// above already has a non-zero CallCount from the earlier TestBark.* assertions.
	UGizmoBarkTestListener* MilestoneListener = NewObject<UGizmoBarkTestListener>();
	Subsystem->OnBarkTriggered.AddDynamic(MilestoneListener, &UGizmoBarkTestListener::HandleBarkTriggered);

	Subsystem->TriggerBarkForMilestone(TEXT("Milestone.MeetKrowd"));
	TestEqual(TEXT("CallCount should be 1 after the first milestone trigger"), MilestoneListener->CallCount, 1);
	TestEqual(TEXT("LastBarkID should be the triggered milestone tag"), MilestoneListener->LastBarkID, FName(TEXT("Milestone.MeetKrowd")));
	TestTrue(TEXT("LastLines should be non-empty for a registered milestone bark"), MilestoneListener->LastLines.Num() > 0);

	// Re-triggering the same milestone tag must not re-fire - proves
	// TriggerBarkForMilestone itself inherits the once-only guarantee.
	Subsystem->TriggerBarkForMilestone(TEXT("Milestone.MeetKrowd"));
	TestEqual(TEXT("CallCount should still be 1 after a second trigger of the same milestone tag"), MilestoneListener->CallCount, 1);

	// The remaining four story-beat tags each fire independently and increment CallCount.
	Subsystem->TriggerBarkForMilestone(TEXT("Milestone.SavingFellowRobots"));
	TestEqual(TEXT("CallCount should be 2 after triggering SavingFellowRobots"), MilestoneListener->CallCount, 2);
	TestEqual(TEXT("LastBarkID should be SavingFellowRobots"), MilestoneListener->LastBarkID, FName(TEXT("Milestone.SavingFellowRobots")));

	Subsystem->TriggerBarkForMilestone(TEXT("Milestone.AsleepForALongTime"));
	TestEqual(TEXT("CallCount should be 3 after triggering AsleepForALongTime"), MilestoneListener->CallCount, 3);
	TestEqual(TEXT("LastBarkID should be AsleepForALongTime"), MilestoneListener->LastBarkID, FName(TEXT("Milestone.AsleepForALongTime")));

	Subsystem->TriggerBarkForMilestone(TEXT("Milestone.HiddenEnemyRevealed"));
	TestEqual(TEXT("CallCount should be 4 after triggering HiddenEnemyRevealed"), MilestoneListener->CallCount, 4);
	TestEqual(TEXT("LastBarkID should be HiddenEnemyRevealed"), MilestoneListener->LastBarkID, FName(TEXT("Milestone.HiddenEnemyRevealed")));

	Subsystem->TriggerBarkForMilestone(TEXT("Milestone.FinalChapter"));
	TestEqual(TEXT("CallCount should be 5 after triggering FinalChapter"), MilestoneListener->CallCount, 5);
	TestEqual(TEXT("LastBarkID should be FinalChapter"), MilestoneListener->LastBarkID, FName(TEXT("Milestone.FinalChapter")));

	// The age-reveal beat is a distinct, separately-triggerable entry from the five
	// story beats, per the issue's explicit callout.
	Subsystem->TriggerBarkForMilestone(TEXT("Milestone.KrowdAgeReveal"));
	TestEqual(TEXT("CallCount should be 6 after triggering KrowdAgeReveal"), MilestoneListener->CallCount, 6);
	TestEqual(TEXT("LastBarkID should be KrowdAgeReveal"), MilestoneListener->LastBarkID, FName(TEXT("Milestone.KrowdAgeReveal")));

	bool bAgeRevealMentions203 = false;
	for (const FString& Line : MilestoneListener->LastLines)
	{
		if (Line.Contains(TEXT("203")))
		{
			bAgeRevealMentions203 = true;
			break;
		}
	}
	TestTrue(TEXT("KrowdAgeReveal's Lines should contain the age-203 reveal"), bAgeRevealMentions203);

	// TriggerBarkForMilestone with an unregistered tag inherits TriggerBark's
	// unknown-ID no-op-with-warning guarantee unmodified - pinned here directly rather
	// than relying solely on section (3)'s transitive coverage via TriggerBark itself.
	const FName UnknownMilestoneTag = TEXT("Milestone.NeverRegistered");
	AddExpectedError(TEXT("unknown bark ID"), EAutomationExpectedErrorFlags::Contains, 1);
	Subsystem->TriggerBarkForMilestone(UnknownMilestoneTag);
	TestEqual(TEXT("CallCount should be unaffected by an unknown milestone tag"), MilestoneListener->CallCount, 6);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
