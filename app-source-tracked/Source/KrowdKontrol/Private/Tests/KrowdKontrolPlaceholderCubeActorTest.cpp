// Proves the project's first placeholder-first gameplay class (issue #2) is wired
// correctly: constructing APlaceholderCubeActor produces a mesh component pointed at
// the engine's default cube mesh, not a null or wrong asset reference. This is the
// KrowdKontrol.Unit.* group's first member — see harness/harness.config.json's
// "unit" field and harness/README.md's "What to fill in first", item 1.
//
// Uses NewObject rather than spawning into a UWorld: this is a unit test of the
// constructor's asset-wiring logic, not a world-integration test, and NewObject keeps
// it free of any new module dependency (no UnrealEd / AutomationEditorCommon needed).
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as
// KrowdKontrolSmokeTest.cpp.

#include "Misc/AutomationTest.h"
#include "PlaceholderCubeActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolPlaceholderCubeActorTest,
	"KrowdKontrol.Unit.PlaceholderCubeActorHasCubeMesh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolPlaceholderCubeActorTest::RunTest(const FString& Parameters)
{
	APlaceholderCubeActor* Actor = NewObject<APlaceholderCubeActor>();
	if (!TestNotNull(TEXT("APlaceholderCubeActor should construct"), Actor))
	{
		return false;
	}

	UStaticMeshComponent* Mesh = Actor->MeshComponent;
	if (!TestNotNull(TEXT("PlaceholderCubeActor should have a MeshComponent"), Mesh))
	{
		return false;
	}

	UStaticMesh* StaticMesh = Mesh->GetStaticMesh();
	if (!TestNotNull(TEXT("MeshComponent should have a static mesh assigned"), StaticMesh))
	{
		return false;
	}

	TestEqual(TEXT("MeshComponent should use the engine's default cube mesh"),
		StaticMesh->GetPathName(), FString(TEXT("/Engine/BasicShapes/Cube.Cube")));

	TestEqual(TEXT("MeshComponent should be the actor's root component"),
		Actor->GetRootComponent(), static_cast<USceneComponent*>(Mesh));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
