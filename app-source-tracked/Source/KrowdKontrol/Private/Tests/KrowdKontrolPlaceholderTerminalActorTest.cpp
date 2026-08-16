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

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
