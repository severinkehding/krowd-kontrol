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
	KrowdKontrolLevelTestUtils::CheckDoorsHaveVisibleMarker(*this, Doors);
	// CheckRoomsHaveFloorGeometry / CheckDoorsHaveConnectorGeometry removed — belongs to
	// issue #187, not present in this PR's own diff or in main.

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

	// Issue #189: room spacing compression and enemy density ramp, both independently
	// falsifiable rather than resting on changelog prose (D-009).
	KrowdKontrolLevelTestUtils::CheckAdjacentRoomSpacingCompressed(*this, Rooms);
	KrowdKontrolLevelTestUtils::CheckEnemyDensityRamp(*this, Rooms, EnemyCountByRoom);

	// Issue #189, pass-1 review follow-up: the two relative checks above only prove
	// "compressed" and "increasing", not this PR's actual claimed values (2400cm hops,
	// 1/2/3 split) - asserting the literal numbers here makes this tracked test file
	// itself a stronger, independently-checkable proxy for the claimed .umap state,
	// since the underlying binary edit is invisible to the diff (D-009).
	TArray<ARoomActor*> SortedRooms = KrowdKontrolLevelTestUtils::SortRoomsByX(Rooms);
	if (TestEqual(TEXT("L_Level01 should have exactly 3 rooms for the exact-position/count check below"), SortedRooms.Num(), 3))
	{
		TestEqual(TEXT("Room 1 (entrance) should stay fixed at X=0 (issue #189)"), SortedRooms[0]->GetActorLocation().X, 0.0);
		TestEqual(TEXT("Room 2 should sit at X=2400 (issue #189)"), SortedRooms[1]->GetActorLocation().X, 2400.0);
		TestEqual(TEXT("Room 3 should sit at X=4800 (issue #189)"), SortedRooms[2]->GetActorLocation().X, 4800.0);

		TestEqual(TEXT("Room 1 should have exactly 1 enemy (issue #189)"), EnemyCountByRoom.FindRef(SortedRooms[0]), 1);
		TestEqual(TEXT("Room 2 should have exactly 2 enemies (issue #189)"), EnemyCountByRoom.FindRef(SortedRooms[1]), 2);
		TestEqual(TEXT("Room 3 should have exactly 3 enemies (issue #189)"), EnemyCountByRoom.FindRef(SortedRooms[2]), 3);
	}

	// Issue #218: real-level regression coverage for room/door gating. LoadMap does not
	// start play, so DispatchBeginPlay() (the public, legal route - see
	// KrowdKontrolCrowdMasteryBeginPlayWiringTest.cpp for the same pattern; a direct
	// BeginPlay() call would not compile, it's protected) is required to exercise
	// ADoorConnectorActor's GatingRoom auto-derivation and ARoomActor's owned-enemy
	// auto-discovery against this shipped level's actual placed actors - the exact gap
	// the E2E holdout caught in attempt 1 (PR #228), where correct C++ logic shipped
	// with GatingRoom=None and empty OwnedEnemies on every real door/room.
	for (ADoorConnectorActor* Door : Doors)
	{
		Door->DispatchBeginPlay();
		TestNotNull(TEXT("Every real door should resolve a GatingRoom automatically (issue #218)"),
			Door->GatingRoom.Get());
	}
	for (ARoomActor* Room : Rooms)
	{
		Room->DispatchBeginPlay();
		TestTrue(TEXT("Every real room should auto-discover at least one owned enemy (issue #218)"),
			Room->GetOwnedEnemies().Num() >= 1);
		TestEqual(TEXT("Auto-discovered OwnedEnemies count should match nearest-room-by-distance (issue #218)"),
			Room->GetOwnedEnemies().Num(), EnemyCountByRoom.FindRef(Room));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
