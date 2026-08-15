// Confirms APlaceholderTargetZoneActor (issue #72) carries a visible world-space
// beacon - a mesh + point light pair - satisfying PRD 13 REQ-6's "target-zone
// indicators are world-space UI, not screen-space HUD" requirement, and that the
// beacon's colour is not one of MISSION.md Hard Invariant 3's five reserved
// gameplay-information colours.
//
// Uses FAutomationEditorCommonUtils::CreateNewMap() + World->SpawnActor(), mirroring
// KrowdKontrolRoomEnemyBudgetControllerTest.cpp (issue #82), rather than a bare
// NewObject() as in KrowdKontrolPlaceholderCubeActorTest.cpp: the issue's acceptance
// criteria specifically requires proving the beacon is "present and visible in a test
// map", not just that the constructor wires components correctly.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "PlaceholderTargetZoneActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolPlaceholderTargetZoneActorTest,
	"KrowdKontrol.Unit.PlaceholderTargetZoneActorHasVisibleBeacon",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolPlaceholderTargetZoneActorTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	APlaceholderTargetZoneActor* Actor = World->SpawnActor<APlaceholderTargetZoneActor>();
	if (!TestNotNull(TEXT("APlaceholderTargetZoneActor should spawn into the test World"), Actor))
	{
		return false;
	}

	UStaticMeshComponent* Mesh = Actor->BeaconMeshComponent;
	if (!TestNotNull(TEXT("PlaceholderTargetZoneActor should have a BeaconMeshComponent"), Mesh))
	{
		return false;
	}

	UStaticMesh* StaticMesh = Mesh->GetStaticMesh();
	if (!TestNotNull(TEXT("BeaconMeshComponent should have a static mesh assigned"), StaticMesh))
	{
		return false;
	}
	TestTrue(TEXT("BeaconMeshComponent should be visible"), Mesh->IsVisible());
	TestEqual(TEXT("BeaconMeshComponent should be the actor's root component"),
		Actor->GetRootComponent(), static_cast<USceneComponent*>(Mesh));

	UPointLightComponent* Light = Actor->BeaconLightComponent;
	if (!TestNotNull(TEXT("PlaceholderTargetZoneActor should have a BeaconLightComponent"), Light))
	{
		return false;
	}
	TestTrue(TEXT("BeaconLightComponent should be visible"), Light->IsVisible());
	TestTrue(TEXT("BeaconLightComponent should have nonzero intensity"), Light->Intensity > 0.0f);
	TestEqual(TEXT("Beacon colour should be the chosen non-reserved green"),
		Light->GetLightColor(), FLinearColor(0.2f, 1.0f, 0.3f));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
