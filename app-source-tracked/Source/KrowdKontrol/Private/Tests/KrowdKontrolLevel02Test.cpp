// Confirms L_Level02 (issue #43, PRD 05 REQ-1/REQ-2/REQ-3) - the next Alpha level
// after L_Level01 (issue #42) - loads without errors and is measurably harder than
// L_Level01: strictly more rooms and strictly more enemies, computed dynamically
// against the live L_Level01 asset rather than a hardcoded magic number, so the
// comparison can't silently drift if L_Level01 is ever revised.
//
// Uses FAutomationEditorCommonUtils::LoadMap (not CreateNewMap) because this is
// regression coverage for the shipped level assets themselves, not the ARoomActor/
// ADoorConnectorActor classes in isolation (those are covered by
// KrowdKontrolRoomActorTest.cpp / KrowdKontrolDoorConnectorActorTest.cpp) - mirrors
// KrowdKontrolLevel01Test.cpp's own approach.
//
// Room/door counts are asserted via TActorIterator rather than by name, since
// TActorIterator iteration order is not guaranteed to match spawn order - a re-save
// of either level that shuffles actor GUIDs must not break this test.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "RoomActor.h"
#include "DoorConnectorActor.h"
#include "EnemyBase.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/LevelStructureTestUtils.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Editor.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolLevel02StructureTest,
	"KrowdKontrol.Unit.Level02Structure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolLevel02StructureTest::RunTest(const FString& Parameters)
{
	// Load L_Level01 first to gather its room/enemy counts as the live baseline the
	// difficulty ramp (REQ-3) must exceed - computed here, not hardcoded, so this test
	// can't silently drift if L_Level01 is ever revised.
	FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/Maps/L_Level01"));

	UWorld* Level01World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("L_Level01 should load into a valid World"), Level01World))
	{
		return false;
	}

	int32 Level01RoomCount = 0;
	for (TActorIterator<ARoomActor> It(Level01World); It; ++It)
	{
		++Level01RoomCount;
	}

	int32 Level01EnemyCount = 0;
	for (TActorIterator<AEnemyBase> It(Level01World); It; ++It)
	{
		++Level01EnemyCount;
	}

	FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/Maps/L_Level02"));

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("L_Level02 should load into a valid World"), World))
	{
		return false;
	}

	TArray<ARoomActor*> Rooms;
	for (TActorIterator<ARoomActor> It(World); It; ++It)
	{
		Rooms.Add(*It);
	}
	TestTrue(TEXT("L_Level02's room count should be strictly greater than L_Level01's (REQ-3 difficulty ramp)"),
		Rooms.Num() > Level01RoomCount);
	// Exact-count check is intentionally in addition to (not instead of) the dynamic
	// comparison above: L_Level02 is a hand-authored level with a fixed design target
	// per PRD 05, not a procedurally generated one, so "4" here isn't a magic number
	// at risk of drifting relative to some other live value - it's this specific
	// level's own spec. The dynamic check guards the REQ-3 difficulty ramp against
	// L_Level01 changing; this check guards L_Level02 itself against an unintended
	// room being added/removed.
	TestEqual(TEXT("L_Level02 room count should match its design target of 4"), Rooms.Num(), 4);

	TArray<ADoorConnectorActor*> Doors;
	for (TActorIterator<ADoorConnectorActor> It(World); It; ++It)
	{
		Doors.Add(*It);
	}
	TestEqual(TEXT("L_Level02 should have 3 doors connecting its 4 rooms in a chain"), Doors.Num(), 3);

	KrowdKontrolLevelTestUtils::CheckAllRoomsReachableViaDoors(*this, Rooms, Doors);
	KrowdKontrolLevelTestUtils::CheckRoomsHaveFloorGeometry(*this, Rooms);
	KrowdKontrolLevelTestUtils::CheckDoorsHaveConnectorGeometry(*this, Doors);

	TMap<ARoomActor*, TSet<EEnemyType>> EnemyTypesByRoom;
	TMap<ARoomActor*, int32> EnemyCountByRoom;
	const int32 TotalLevel02EnemyCount = KrowdKontrolLevelTestUtils::CollectEnemyRoomAssignments(
		World, Rooms, EnemyTypesByRoom, EnemyCountByRoom);

	// Lightweight per-room density check (PRD 05's "static placeholder-density
	// enemies") - only asserts every room has *some* enemy presence, not a specific
	// count, mirroring KrowdKontrolLevel01Test.cpp's own approach.
	KrowdKontrolLevelTestUtils::CheckRoomTargetZonesAndDensity(*this, Rooms, EnemyTypesByRoom, EnemyCountByRoom);

	TestTrue(TEXT("L_Level02's total enemy count should be strictly greater than L_Level01's (REQ-3 difficulty ramp)"),
		TotalLevel02EnemyCount > Level01EnemyCount);
	// See the room-count comment above: same rationale applies to this exact-count
	// check alongside the dynamic difficulty-ramp comparison.
	TestEqual(TEXT("L_Level02 total enemy count should match its design target of 8"), TotalLevel02EnemyCount, 8);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
