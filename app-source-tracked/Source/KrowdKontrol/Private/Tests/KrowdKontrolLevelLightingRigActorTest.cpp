// Confirms ALevelLightingRigActor (issue #186, PRD "Level Playability & Presentation"
// REQ-2) is correctly wired - a plain USceneComponent root with an attached
// DirectionalLightComponent + SkyLightComponent, both tuned to their designed
// placeholder intensity/colour - and that neither light's colour collides with any of
// MISSION.md Hard Invariant 3's five reserved gameplay-information colours.
//
// Uses FAutomationEditorCommonUtils::CreateNewMap() + World->SpawnActor(), mirroring
// KrowdKontrolPlaceholderTargetZoneActorTest.cpp - this test proves the class's own
// wiring/colour-lock in isolation, independent of any shipped map (that coverage lives
// in KrowdKontrolLevel01Test.cpp / KrowdKontrolLevel02Test.cpp instead).
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "LevelLightingRigActor.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "ReservedGameplayColours.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolLevelLightingRigActorTest,
	"KrowdKontrol.Unit.LevelLightingRigActorHasDimReadableLighting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolLevelLightingRigActorTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	ALevelLightingRigActor* Actor = World->SpawnActor<ALevelLightingRigActor>();
	if (!TestNotNull(TEXT("ALevelLightingRigActor should spawn into the test World"), Actor))
	{
		return false;
	}

	USceneComponent* Root = Actor->RigRootComponent;
	if (!TestNotNull(TEXT("ALevelLightingRigActor should have a RigRootComponent"), Root))
	{
		return false;
	}
	TestEqual(TEXT("RigRootComponent should be the actor's root component"),
		Actor->GetRootComponent(), Root);

	UDirectionalLightComponent* Directional = Actor->DirectionalLightComponent;
	if (!TestNotNull(TEXT("ALevelLightingRigActor should have a DirectionalLightComponent"), Directional))
	{
		return false;
	}
	TestEqual(TEXT("DirectionalLightComponent should be attached to RigRootComponent"),
		Directional->GetAttachParent(), Root);
	TestEqual(TEXT("DirectionalLightComponent should use the planned baseline intensity"),
		Directional->Intensity, 1.5f);
	// Colour goes through an 8-bit FColor round-trip inside ULightComponentBase, so an
	// exact TestEqual would depend on incidental quantization rather than a designed
	// guarantee - use a tolerance instead, mirroring KrowdKontrolPlaceholderTargetZoneActorTest.cpp.
	TestTrue(TEXT("DirectionalLightComponent colour should be the chosen desaturated cool-neutral"),
		Directional->GetLightColor().Equals(FLinearColor(0.55f, 0.6f, 0.68f, 1.0f), 0.01f));

	USkyLightComponent* Sky = Actor->SkyLightComponent;
	if (!TestNotNull(TEXT("ALevelLightingRigActor should have a SkyLightComponent"), Sky))
	{
		return false;
	}
	TestEqual(TEXT("SkyLightComponent should be attached to RigRootComponent"),
		Sky->GetAttachParent(), Root);
	TestEqual(TEXT("SkyLightComponent should use the planned baseline intensity"),
		Sky->Intensity, 0.4f);
	TestTrue(TEXT("SkyLightComponent colour should be the chosen desaturated cool-neutral"),
		Sky->GetLightColor().Equals(FLinearColor(0.55f, 0.6f, 0.68f, 1.0f), 0.01f));

	// Colour-lock regression guard (issue #186 AC): neither light may collide with any
	// of the 5 reserved gameplay-information colours - mirrors
	// KrowdKontrolBomberEnemyTest.cpp's EliteTrimLightComponent assertion.
	TestFalse(TEXT("DirectionalLightComponent colour should not collide with a reserved gameplay-information colour"),
		ReservedGameplayColours::GetAll().ContainsByPredicate(
			[Directional](const FLinearColor& Reserved) { return Reserved.Equals(Directional->GetLightColor(), 0.01f); }));
	TestFalse(TEXT("SkyLightComponent colour should not collide with a reserved gameplay-information colour"),
		ReservedGameplayColours::GetAll().ContainsByPredicate(
			[Sky](const FLinearColor& Reserved) { return Reserved.Equals(Sky->GetLightColor(), 0.01f); }));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
