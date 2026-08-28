// Confirms ADoorConnectorActor (issue #39, PRD 05 REQ-1/REQ-2) can be placed and
// ConnectsValidRooms() correctly reflects whether it references two distinct,
// valid ARoomActor instances - false by default (no rooms assigned), true once two
// distinct rooms are assigned, and false again if the same room is assigned to both
// slots (a door can't connect a room to itself).
//
// Uses the same CreateNewMap()/SpawnActor scaffold as KrowdKontrolRoomActorTest.cpp
// for consistency; rooms are spawned with no target zones since this test only
// exercises ADoorConnectorActor's reference logic.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "DoorConnectorActor.h"
#include "RoomActor.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/BoxComponent.h"
#include "ReservedGameplayColours.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolDoorConnectorActorTest,
	"KrowdKontrol.Unit.DoorConnectorActor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolDoorConnectorActorTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	ARoomActor* RoomOne = World->SpawnActor<ARoomActor>();
	if (!TestNotNull(TEXT("First ARoomActor should spawn into the test World"), RoomOne))
	{
		return false;
	}

	ARoomActor* RoomTwo = World->SpawnActor<ARoomActor>();
	if (!TestNotNull(TEXT("Second ARoomActor should spawn into the test World"), RoomTwo))
	{
		return false;
	}
	// SpawnActor with no explicit FTransform places both rooms at the world origin -
	// left as-is here (rather than immediately repositioning RoomTwo) so the block
	// below can exercise the zero-length-span guard against two distinct, valid rooms
	// before RoomTwo is moved apart for the non-degenerate case.

	ADoorConnectorActor* Door = World->SpawnActor<ADoorConnectorActor>();
	if (!TestNotNull(TEXT("ADoorConnectorActor should spawn into the test World"), Door))
	{
		return false;
	}

	TestFalse(TEXT("A freshly-spawned door with no rooms assigned should not connect valid rooms"),
		Door->ConnectsValidRooms());
	TestFalse(TEXT("Door marker mesh should start hidden before any rooms are assigned"),
		Door->DoorMarkerMeshComponent->IsVisible());
	TestFalse(TEXT("Door marker light should start hidden before any rooms are assigned"),
		Door->DoorMarkerLightComponent->IsVisible());
	TestEqual(TEXT("Corridor guard rail A should be NoCollision before the door connects valid rooms (issue #243)"),
		Door->CorridorGuardRailAComponent->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	TestEqual(TEXT("Corridor guard rail B should be NoCollision before the door connects valid rooms (issue #243)"),
		Door->CorridorGuardRailBComponent->GetCollisionEnabled(), ECollisionEnabled::NoCollision);

	Door->RoomA = RoomOne;
	TestFalse(TEXT("A door with only RoomA assigned should not connect valid rooms"),
		Door->ConnectsValidRooms());

	Door->RoomB = RoomTwo;
	TestTrue(TEXT("A door referencing two distinct rooms should connect valid rooms"),
		Door->ConnectsValidRooms());

	Door->RecomputeConnectorGeometry();
	TestFalse(TEXT("Connector floor mesh should stay hidden when both rooms share a location"),
		Door->ConnectorFloorMeshComponent->IsVisible());
	const FVector DegenerateScale = Door->ConnectorFloorMeshComponent->GetComponentScale();
	TestFalse(TEXT("Connector scale should not be NaN for a zero-length span"),
		FMath::IsNaN(DegenerateScale.X) || FMath::IsNaN(DegenerateScale.Y) || FMath::IsNaN(DegenerateScale.Z));
	TestFalse(TEXT("Door marker mesh should stay hidden when both rooms share a location"),
		Door->DoorMarkerMeshComponent->IsVisible());
	TestFalse(TEXT("Door marker light should stay hidden when both rooms share a location"),
		Door->DoorMarkerLightComponent->IsVisible());

	// Now give RoomTwo a distinct location so RecomputeConnectorGeometry() below has a
	// genuine, non-degenerate span to compute.
	RoomTwo->SetActorLocation(FVector(3000.f, 0.f, 0.f));

	Door->RecomputeConnectorGeometry();
	TestTrue(TEXT("Connector floor mesh should be visible once the door connects two valid rooms"),
		Door->ConnectorFloorMeshComponent->IsVisible());
	const float ExpectedScaleX = (RoomTwo->GetActorLocation() - RoomOne->GetActorLocation()).Size() / 100.f;
	TestTrue(TEXT("Connector floor mesh's X scale should span the distance between the two rooms"),
		FMath::IsNearlyEqual(Door->ConnectorFloorMeshComponent->GetComponentScale().X, ExpectedScaleX, 0.01f));
	TestTrue(TEXT("Connector floor mesh's Y scale should be driven by ConnectorFloorWidth"),
		FMath::IsNearlyEqual(Door->ConnectorFloorMeshComponent->GetComponentScale().Y, Door->ConnectorFloorWidth / 100.f, 0.01f));
	TestTrue(TEXT("Connector floor mesh's Z scale should be driven by ConnectorFloorThickness"),
		FMath::IsNearlyEqual(Door->ConnectorFloorMeshComponent->GetComponentScale().Z, Door->ConnectorFloorThickness / 100.f, 0.01f));

	TestEqual(TEXT("Corridor guard rail A should be QueryOnly once the door connects two valid rooms (issue #243)"),
		Door->CorridorGuardRailAComponent->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);
	TestEqual(TEXT("Corridor guard rail A should Block ECC_WorldDynamic - the channel the real player pawn presents (issue #243)"),
		Door->CorridorGuardRailAComponent->GetCollisionResponseToChannel(ECC_WorldDynamic), ECR_Block);
	TestEqual(TEXT("Corridor guard rail B should be QueryOnly once the door connects two valid rooms (issue #243)"),
		Door->CorridorGuardRailBComponent->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);
	TestEqual(TEXT("Corridor guard rail B should Block ECC_WorldDynamic - the channel the real player pawn presents (issue #243)"),
		Door->CorridorGuardRailBComponent->GetCollisionResponseToChannel(ECC_WorldDynamic), ECR_Block);
	TestEqual(TEXT("GateBlockingComponent should be WorldStatic-typed so a fleeing enemy's "
		"WorldDynamic-narrowed response (issue #211) can still be blocked by a closed gate (issue #243)"),
		Door->GateBlockingComponent->GetCollisionObjectType(), ECC_WorldStatic);
	const FVector GuardRailMidpoint =
		(Door->CorridorGuardRailAComponent->GetComponentLocation() + Door->CorridorGuardRailBComponent->GetComponentLocation()) * 0.5f;
	const FVector ExpectedConnectorMidpoint = (RoomOne->GetActorLocation() + RoomTwo->GetActorLocation()) * 0.5f;
	TestTrue(TEXT("Corridor guard rails should be symmetric around the connector's midpoint (issue #243)"),
		GuardRailMidpoint.Equals(ExpectedConnectorMidpoint, 0.1f));
	// Regression (2026-08-26 operator playtest): rails must span only the corridor gap
	// between the two room perimeters (1000cm in this setup), never the full
	// centre-to-centre span - a full-span rail runs from room centre to room centre,
	// carving an impassable channel through both room interiors.
	const float ExpectedRailHalfLength =
		((RoomTwo->GetActorLocation() - RoomOne->GetActorLocation()).Size()
			- RoomOne->RoomFloorExtent.X - RoomTwo->RoomFloorExtent.X) * 0.5f;
	TestTrue(TEXT("Corridor guard rail A should span only the gap between room perimeters, not the room interiors (2026-08-26 playtest)"),
		FMath::IsNearlyEqual(Door->CorridorGuardRailAComponent->GetUnscaledBoxExtent().X, ExpectedRailHalfLength, 0.1f));
	TestTrue(TEXT("Corridor guard rail B should span only the gap between room perimeters, not the room interiors (2026-08-26 playtest)"),
		FMath::IsNearlyEqual(Door->CorridorGuardRailBComponent->GetUnscaledBoxExtent().X, ExpectedRailHalfLength, 0.1f));

	TestTrue(TEXT("Door marker mesh should be visible once the door connects two valid rooms"),
		Door->DoorMarkerMeshComponent->IsVisible());
	TestEqual(TEXT("Door marker mesh should have no collision so it never blocks the connector path"),
		Door->DoorMarkerMeshComponent->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	const FVector ExpectedMarkerLocation =
		(RoomOne->GetActorLocation() + RoomTwo->GetActorLocation()) * 0.5f + FVector(0.f, 0.f, Door->DoorMarkerHeight);
	TestTrue(TEXT("Door marker mesh should sit above the connector midpoint by DoorMarkerHeight"),
		Door->DoorMarkerMeshComponent->GetComponentLocation().Equals(ExpectedMarkerLocation, 0.1f));

	UPointLightComponent* MarkerLight = Door->DoorMarkerLightComponent;
	if (!TestNotNull(TEXT("ADoorConnectorActor should have a DoorMarkerLightComponent"), MarkerLight))
	{
		return false;
	}
	TestEqual(TEXT("DoorMarkerLightComponent should be attached to DoorMarkerMeshComponent"),
		MarkerLight->GetAttachParent(), static_cast<USceneComponent*>(Door->DoorMarkerMeshComponent));
	TestTrue(TEXT("DoorMarkerLightComponent should be visible once the door connects two valid rooms"),
		MarkerLight->IsVisible());
	TestFalse(TEXT("DoorMarkerLightComponent colour should not collide with a reserved gameplay-information colour"),
		ReservedGameplayColours::GetAll().ContainsByPredicate(
			[MarkerLight](const FLinearColor& Reserved) { return Reserved.Equals(MarkerLight->GetLightColor(), 0.01f); }));

	Door->RoomB = RoomOne;
	TestFalse(TEXT("A door with the same room assigned to both slots should not connect valid rooms"),
		Door->ConnectsValidRooms());

	Door->RecomputeConnectorGeometry();
	TestFalse(TEXT("Connector floor mesh should be hidden again once the door no longer connects valid rooms"),
		Door->ConnectorFloorMeshComponent->IsVisible());
	TestFalse(TEXT("Door marker mesh should be hidden again once the door no longer connects valid rooms"),
		Door->DoorMarkerMeshComponent->IsVisible());
	TestFalse(TEXT("Door marker light should be hidden again once the door no longer connects valid rooms"),
		Door->DoorMarkerLightComponent->IsVisible());
	TestEqual(TEXT("Corridor guard rail A should revert to NoCollision once the door no longer connects valid rooms (issue #243)"),
		Door->CorridorGuardRailAComponent->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	TestEqual(TEXT("Corridor guard rail B should revert to NoCollision once the door no longer connects valid rooms (issue #243)"),
		Door->CorridorGuardRailBComponent->GetCollisionEnabled(), ECollisionEnabled::NoCollision);

	// Non-axis-aligned RoomA/RoomB pair - proves Finding 2b's ComputeAxisExitDistance
	// fix. The axis-aligned pair above (Delta.Y == 0) can't distinguish the old
	// support-function formula from the corrected ray-exit-distance formula - they're
	// numerically identical for that case. Giving RoomB a diagonal offset makes them
	// diverge.
	ARoomActor* DiagonalRoomA = World->SpawnActor<ARoomActor>();
	ARoomActor* DiagonalRoomB = World->SpawnActor<ARoomActor>();
	if (!TestNotNull(TEXT("Diagonal RoomA should spawn into the test World"), DiagonalRoomA) ||
		!TestNotNull(TEXT("Diagonal RoomB should spawn into the test World"), DiagonalRoomB))
	{
		return false;
	}
	DiagonalRoomB->SetActorLocation(FVector(3000.f, 1000.f, 0.f));

	ADoorConnectorActor* DiagonalDoor = World->SpawnActor<ADoorConnectorActor>();
	if (!TestNotNull(TEXT("Diagonal ADoorConnectorActor should spawn into the test World"), DiagonalDoor))
	{
		return false;
	}
	DiagonalDoor->RoomA = DiagonalRoomA;
	DiagonalDoor->RoomB = DiagonalRoomB;
	DiagonalDoor->RecomputeConnectorGeometry();

	const FVector DiagonalDelta = DiagonalRoomB->GetActorLocation() - DiagonalRoomA->GetActorLocation();
	const float DiagonalLength = DiagonalDelta.Size();
	const FVector2D DiagonalDirection2D(DiagonalDelta.X / DiagonalLength, DiagonalDelta.Y / DiagonalLength);
	const float ExpectedExitDistance = ARoomActor::ComputeAxisExitDistance(DiagonalRoomA->RoomFloorExtent, DiagonalDirection2D);
	const float ExpectedDiagonalGuardRailHalfLength = (DiagonalLength - 2.f * ExpectedExitDistance) * 0.5f;

	// Sanity check that this case's geometry actually distinguishes the two formulas -
	// otherwise the assertions below could pass even against the old (wrong) code.
	const float OldSupportFunctionExtent =
		FMath::Abs(DiagonalDirection2D.X) * DiagonalRoomA->RoomFloorExtent.X +
		FMath::Abs(DiagonalDirection2D.Y) * DiagonalRoomA->RoomFloorExtent.Y;
	TestTrue(TEXT("This diagonal case's corrected exit distance should differ from the old support-function value, or it can't distinguish the two formulas"),
		!FMath::IsNearlyEqual(ExpectedExitDistance, OldSupportFunctionExtent, 1.f));

	TestTrue(TEXT("Diagonal corridor guard rail A should match ComputeAxisExitDistance's half-length, not the old support-function overshoot (issue #243 Finding 2b)"),
		FMath::IsNearlyEqual(DiagonalDoor->CorridorGuardRailAComponent->GetUnscaledBoxExtent().X, ExpectedDiagonalGuardRailHalfLength, 0.5f));
	TestTrue(TEXT("Diagonal corridor guard rail B should match ComputeAxisExitDistance's half-length, not the old support-function overshoot (issue #243 Finding 2b)"),
		FMath::IsNearlyEqual(DiagonalDoor->CorridorGuardRailBComponent->GetUnscaledBoxExtent().X, ExpectedDiagonalGuardRailHalfLength, 0.5f));

	// Standalone, independently hand-computed case for ComputeAxisExitDistance (issue
	// #243 code-review Finding 3) - decoupled from any ADoorConnectorActor, so a bug in
	// the primitive itself (not just in how RecomputeConnectorGeometry() calls it) would
	// be caught. HalfExtent=(300,200), Direction=(0.6,0.8) (a 3-4-5 triangle direction):
	// ExitX = 300/0.6 = 500, ExitY = 200/0.8 = 250 - the ray exits through the box's
	// top/bottom edge first, so the correct answer is the smaller of the two, 250.
	const float StandaloneExitDistance = ARoomActor::ComputeAxisExitDistance(FVector2D(300.f, 200.f), FVector2D(0.6f, 0.8f));
	TestTrue(TEXT("ComputeAxisExitDistance should return the ray-exit distance for an independently hand-computed 3-4-5 triangle direction (issue #243 code-review Finding 3)"),
		FMath::IsNearlyEqual(StandaloneExitDistance, 250.f, 0.01f));

	// Adjacent/overlapping RoomA/RoomB pair - GuardRailHalfLength <= 0 (room extents of
	// 1000uu each side overshoot the 500uu gap between origins), so guard rails must
	// disable rather than block inside a room (DoorConnectorActor.cpp's own
	// degenerate-skip comment, mirroring BuildWallSideFlanks's KINDA_SMALL_NUMBER guard
	// in RoomActor.cpp - issue #243 test-coverage Finding 3).
	ARoomActor* AdjacentRoomA = World->SpawnActor<ARoomActor>();
	ARoomActor* AdjacentRoomB = World->SpawnActor<ARoomActor>();
	if (!TestNotNull(TEXT("Adjacent RoomA should spawn into the test World"), AdjacentRoomA) ||
		!TestNotNull(TEXT("Adjacent RoomB should spawn into the test World"), AdjacentRoomB))
	{
		return false;
	}
	AdjacentRoomB->SetActorLocation(FVector(500.f, 0.f, 0.f));

	ADoorConnectorActor* AdjacentDoor = World->SpawnActor<ADoorConnectorActor>();
	if (!TestNotNull(TEXT("Adjacent ADoorConnectorActor should spawn into the test World"), AdjacentDoor))
	{
		return false;
	}
	AdjacentDoor->RoomA = AdjacentRoomA;
	AdjacentDoor->RoomB = AdjacentRoomB;
	AdjacentDoor->RecomputeConnectorGeometry();

	TestEqual(TEXT("Guard rail A should disable (not block) when rooms are too close to leave a corridor gap (issue #243)"),
		AdjacentDoor->CorridorGuardRailAComponent->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	TestEqual(TEXT("Guard rail B should disable (not block) when rooms are too close to leave a corridor gap (issue #243)"),
		AdjacentDoor->CorridorGuardRailBComponent->GetCollisionEnabled(), ECollisionEnabled::NoCollision);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
