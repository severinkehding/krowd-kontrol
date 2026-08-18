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
	TestEqual(TEXT("BeaconMeshComponent should use the engine's cylinder mesh"),
		StaticMesh->GetPathName(), FString(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
	TestTrue(TEXT("BeaconMeshComponent should be visible"), Mesh->IsVisible());
	TestEqual(TEXT("BeaconMeshComponent should be the actor's root component"),
		Actor->GetRootComponent(), static_cast<USceneComponent*>(Mesh));
	TestEqual(TEXT("BeaconMeshComponent should be flattened into a floor-marker disc"),
		Mesh->GetRelativeScale3D(), FVector(1.5f, 1.5f, 0.05f));

	UPointLightComponent* Light = Actor->BeaconLightComponent;
	if (!TestNotNull(TEXT("PlaceholderTargetZoneActor should have a BeaconLightComponent"), Light))
	{
		return false;
	}
	TestTrue(TEXT("BeaconLightComponent should be visible"), Light->IsVisible());
	TestEqual(TEXT("BeaconLightComponent should be attached to BeaconMeshComponent"),
		Light->GetAttachParent(), static_cast<USceneComponent*>(Mesh));
	TestEqual(TEXT("BeaconLightComponent should use the planned beacon intensity"),
		Light->Intensity, 3000.0f);
	TestEqual(TEXT("BeaconLightComponent should use the planned attenuation radius"),
		Light->AttenuationRadius, 300.0f);
	// Colour goes through an 8-bit FColor round-trip inside ULightComponentBase, so an
	// exact TestEqual would depend on incidental quantization rather than a designed
	// guarantee - use a tolerance instead.
	TestTrue(TEXT("Beacon colour should be the chosen non-reserved green"),
		Light->GetLightColor().Equals(FLinearColor(0.2f, 1.0f, 0.3f), 0.01f));

	// IntensifyBeacon() (issue #29) raises the light from its baseline to its
	// intensified intensity - mirrors ASniperEnemy's EyeGlow baseline/intensified
	// assertion pair (KrowdKontrolSniperEnemyTest.cpp).
	TestEqual(TEXT("BeaconLightComponent should start at BeaconBaselineIntensity"),
		Light->Intensity, Actor->BeaconBaselineIntensity);
	Actor->IntensifyBeacon();
	TestEqual(TEXT("IntensifyBeacon should raise the beacon to its intensified intensity"),
		Light->Intensity, Actor->BeaconIntensifiedIntensity);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
