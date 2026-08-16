// Proves APlaceholderTerminalActor (issue #62, PRD 07 REQ-4) is the reusable
// "optional, non-gating environmental storytelling" placeholder: interacting with it
// reveals its foreshadowing log exactly once, and it exposes no other mutable state a
// progression system could depend on.
//
// No UWorld/CreateNewMap() needed - Interact() never calls GetWorld() or
// GetGameInstance() (unlike routing through UGizmoNarrativeSubsystem, which this
// actor deliberately does not do - see this issue's investigation artifact), so a
// bare NewObject<>() actor is sufficient, matching
// KrowdKontrolPlaceholderCubeActorTest.cpp's precedent.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "PlaceholderTerminalActor.h"
#include "GizmoBarkTestListener.h"
#include "ReentrantTerminalListener.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolPlaceholderTerminalActorTest,
	"KrowdKontrol.Unit.PlaceholderTerminalActor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolPlaceholderTerminalActorTest::RunTest(const FString& Parameters)
{
	APlaceholderTerminalActor* Actor = NewObject<APlaceholderTerminalActor>();
	if (!TestNotNull(TEXT("APlaceholderTerminalActor should construct"), Actor))
	{
		return false;
	}

	UStaticMeshComponent* Mesh = Actor->MeshComponent;
	if (!TestNotNull(TEXT("PlaceholderTerminalActor should have a MeshComponent"), Mesh))
	{
		return false;
	}

	UStaticMesh* StaticMesh = Mesh->GetStaticMesh();
	if (!TestNotNull(TEXT("MeshComponent should have a static mesh assigned"), StaticMesh))
	{
		return false;
	}

	TestEqual(TEXT("MeshComponent should use the engine's cylinder mesh"),
		StaticMesh->GetPathName(), FString(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));

	TestEqual(TEXT("MeshComponent should be the actor's root component"),
		Actor->GetRootComponent(), static_cast<USceneComponent*>(Mesh));

	TestFalse(TEXT("HasBeenInteracted should be false before any Interact() call"),
		Actor->HasBeenInteracted());

	Actor->TerminalLog.BarkID = TEXT("Terminal.Test");
	Actor->TerminalLog.Lines = { TEXT("Foreshadowing line one."), TEXT("Foreshadowing line two.") };

	UGizmoBarkTestListener* Listener = NewObject<UGizmoBarkTestListener>();
	Actor->OnTerminalLogRevealed.AddDynamic(Listener, &UGizmoBarkTestListener::HandleBarkTriggered);

	// (1) First Interact() broadcasts once with TerminalLog's content and flips
	// HasBeenInteracted().
	Actor->Interact();
	TestEqual(TEXT("CallCount should be 1 after Interact()"), Listener->CallCount, 1);
	TestEqual(TEXT("LastBarkID should match TerminalLog.BarkID"), Listener->LastBarkID, Actor->TerminalLog.BarkID);
	TestEqual(TEXT("LastLines should match TerminalLog.Lines"), Listener->LastLines, Actor->TerminalLog.Lines);
	TestTrue(TEXT("HasBeenInteracted should be true after Interact()"), Actor->HasBeenInteracted());

	// (2) Fires exactly once: a second Interact() call must not re-broadcast.
	Actor->Interact();
	TestEqual(TEXT("CallCount should still be 1 - no replay"), Listener->CallCount, 1);

	// (3) No progression-gating side effects: APlaceholderTerminalActor's only public
	// mutable surface is TerminalLog and OnTerminalLogRevealed, both already asserted
	// above, and Interact() calls no other API - this makes "never required to
	// progress" a structural fact of the class (nothing else exists to gate on)
	// rather than something requiring a separate runtime assertion against a
	// progression system that doesn't exist yet.

	// (4) Regression: a listener that re-enters Interact() on the same actor from
	// inside the broadcast must not re-fire. Pins down Interact()'s
	// flip-before-broadcast ordering (bHasBeenTriggered = true precedes
	// OnTerminalLogRevealed.Broadcast) - the property that makes same-actor
	// re-entrancy safe instead of unbounded recursion. Mirrors
	// KrowdKontrolGizmoNarrativeSubsystemTest.cpp's case (4) for TriggerBark.
	APlaceholderTerminalActor* ReentrantActor = NewObject<APlaceholderTerminalActor>();
	ReentrantActor->TerminalLog.BarkID = TEXT("Terminal.Reentrant");
	ReentrantActor->TerminalLog.Lines = { TEXT("Only fires once even if re-entered.") };

	UReentrantTerminalListener* ReentrantListener = NewObject<UReentrantTerminalListener>();
	ReentrantListener->ActorToReenter = ReentrantActor;
	ReentrantActor->OnTerminalLogRevealed.AddDynamic(ReentrantListener, &UReentrantTerminalListener::HandleBarkTriggered);

	ReentrantActor->Interact();
	TestEqual(TEXT("Re-entrant Interact() during broadcast must not re-fire"),
		ReentrantListener->CallCount, 1);

	// (5) An editor-authored TerminalLog.bHasBeenTriggered = true (settable in the
	// details panel since TerminalLog is EditAnywhere) must make Interact() a silent
	// no-op, the same as if it had already fired - no separate "pre-triggered" code
	// path exists, so this documents that the existing no-replay guard also covers
	// authoring-time state, not just runtime state.
	APlaceholderTerminalActor* PreTriggeredActor = NewObject<APlaceholderTerminalActor>();
	PreTriggeredActor->TerminalLog.BarkID = TEXT("Terminal.PreTriggered");
	PreTriggeredActor->TerminalLog.Lines = { TEXT("Should never broadcast.") };
	PreTriggeredActor->TerminalLog.bHasBeenTriggered = true;

	UGizmoBarkTestListener* PreTriggeredListener = NewObject<UGizmoBarkTestListener>();
	PreTriggeredActor->OnTerminalLogRevealed.AddDynamic(PreTriggeredListener, &UGizmoBarkTestListener::HandleBarkTriggered);

	PreTriggeredActor->Interact();
	TestEqual(TEXT("Interact() must not broadcast when bHasBeenTriggered was pre-set"),
		PreTriggeredListener->CallCount, 0);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
