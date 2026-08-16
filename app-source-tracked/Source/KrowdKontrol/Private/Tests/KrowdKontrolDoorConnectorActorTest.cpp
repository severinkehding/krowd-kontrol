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

	Door->RoomB = RoomOne;
	TestFalse(TEXT("A door with the same room assigned to both slots should not connect valid rooms"),
		Door->ConnectsValidRooms());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
