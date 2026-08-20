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

	Door->RoomB = RoomOne;
	TestFalse(TEXT("A door with the same room assigned to both slots should not connect valid rooms"),
		Door->ConnectsValidRooms());

	Door->RecomputeConnectorGeometry();
	TestFalse(TEXT("Connector floor mesh should be hidden again once the door no longer connects valid rooms"),
		Door->ConnectorFloorMeshComponent->IsVisible());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
