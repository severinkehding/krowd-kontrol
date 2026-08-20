// Confirms L_Level01 (issue #42, PRD 05 REQ-1/REQ-2/REQ-3) - the smallest of the 5
// hand-authored Alpha levels - loads without errors and matches its design target:
// exactly 3 rooms, 2 doors each connecting two distinct rooms, and every room
// carrying at least one target zone matching each enemy type placed in it.
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
#include "EnemyBase.h"
#include "LevelLightingRigActor.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/LevelStructureTestUtils.h"
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

	TArray<ALevelLightingRigActor*> LightingRigs;
	for (TActorIterator<ALevelLightingRigActor> It(World); It; ++It)
	{
		LightingRigs.Add(*It);
	}
	TestEqual(TEXT("L_Level01 should have exactly one ALevelLightingRigActor placed (issue #186, PRD REQ-2)"),
		LightingRigs.Num(), 1);

	KrowdKontrolLevelTestUtils::CheckAllRoomsReachableViaDoors(*this, Rooms, Doors);
	KrowdKontrolLevelTestUtils::CheckRoomsHaveFloorGeometry(*this, Rooms);
	KrowdKontrolLevelTestUtils::CheckDoorsHaveConnectorGeometry(*this, Doors);

	TMap<ARoomActor*, TSet<EEnemyType>> EnemyTypesByRoom;
	TMap<ARoomActor*, int32> EnemyCountByRoom;
	for (TActorIterator<AEnemyBase> It(World); It; ++It)
	{
		AEnemyBase* Enemy = *It;
		ARoomActor* NearestRoom = KrowdKontrolLevelTestUtils::FindNearestRoom(Enemy, Rooms);
		if (!NearestRoom)
		{
			continue;
		}
		EnemyCountByRoom.FindOrAdd(NearestRoom, 0)++;
		if (TOptional<EEnemyType> EnemyType = KrowdKontrolLevelTestUtils::GetPlacedEnemyType(Enemy))
		{
			EnemyTypesByRoom.FindOrAdd(NearestRoom).Add(EnemyType.GetValue());
		}
	}

	// Lightweight per-room density check (PRD 05's "static placeholder-density
	// enemies") - only asserts every room has *some* enemy presence, not a specific
	// count, since a real per-level density target needs Levels 2-5 to exist for
	// comparison and is out of this test's reach.
	KrowdKontrolLevelTestUtils::CheckRoomTargetZonesAndDensity(*this, Rooms, EnemyTypesByRoom, EnemyCountByRoom);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
