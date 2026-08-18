// Confirms L_Level01 (issue #42, PRD 05 REQ-1/REQ-2/REQ-3) - the smallest of the 5
// hand-authored Alpha levels - loads without errors and matches its design target:
// exactly 3 rooms, 2 doors each connecting two distinct rooms, and every room
// carrying at least one target zone.
//
// Uses FAutomationEditorCommonUtils::LoadMap (not CreateNewMap) because this is
// regression coverage for the shipped level asset itself, not the ARoomActor/
// ADoorConnectorActor classes in isolation (those are covered by
// KrowdKontrolRoomActorTest.cpp / KrowdKontrolDoorConnectorActorTest.cpp) - mirrors
// KrowdKontrolFlatCamera3DPipelineSmokeTest.cpp's FKrowdKontrolFlatCamera3DPipelineLevelTest.
//
// Room/door counts are asserted via TActorIterator rather than by name, since
// TActorIterator iteration order is not guaranteed to match spawn order - a re-save
// of the level that shuffles actor GUIDs must not break this test.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "RoomActor.h"
#include "DoorConnectorActor.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Editor.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolLevel01StructureTest,
	"KrowdKontrol.Unit.Level01Structure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolLevel01StructureTest::RunTest(const FString& Parameters)
{
	FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/Maps/L_Level01"));

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("L_Level01 should load into a valid World"), World))
	{
		return false;
	}

	TArray<ARoomActor*> Rooms;
	for (TActorIterator<ARoomActor> It(World); It; ++It)
	{
		Rooms.Add(*It);
	}
	TestEqual(TEXT("L_Level01 room count should match its design target of 3 (the smallest of the 5 Alpha levels)"),
		Rooms.Num(), 3);

	TArray<ADoorConnectorActor*> Doors;
	for (TActorIterator<ADoorConnectorActor> It(World); It; ++It)
	{
		Doors.Add(*It);
	}
	TestEqual(TEXT("L_Level01 should have 2 doors connecting its 3 rooms in a chain"), Doors.Num(), 2);

	// Count/individual-validity checks above don't rule out both doors wiring the same
	// pair of rooms and leaving the third unreachable - walk the door graph to confirm
	// every room is actually reachable, not just that door count/validity look right.
	TMap<ARoomActor*, TArray<ARoomActor*>> Adjacency;
	for (ADoorConnectorActor* Door : Doors)
	{
		TestTrue(TEXT("Each door should connect two valid, distinct rooms"), Door->ConnectsValidRooms());
		if (Door->ConnectsValidRooms())
		{
			Adjacency.FindOrAdd(Door->RoomA).Add(Door->RoomB);
			Adjacency.FindOrAdd(Door->RoomB).Add(Door->RoomA);
		}
	}

	if (Rooms.Num() > 0)
	{
		TSet<ARoomActor*> Visited;
		TArray<ARoomActor*> Frontier = { Rooms[0] };
		Visited.Add(Rooms[0]);
		while (Frontier.Num() > 0)
		{
			ARoomActor* Current = Frontier.Pop();
			for (ARoomActor* Neighbor : Adjacency.FindRef(Current))
			{
				if (!Visited.Contains(Neighbor))
				{
					Visited.Add(Neighbor);
					Frontier.Add(Neighbor);
				}
			}
		}
		TestEqual(TEXT("All rooms should be reachable via doors (no room isolated from the chain)"),
			Visited.Num(), Rooms.Num());
	}

	for (ARoomActor* Room : Rooms)
	{
		TestTrue(TEXT("Every room should have at least one target zone (REQ-2)"), Room->GetTargetZones().Num() >= 1);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
