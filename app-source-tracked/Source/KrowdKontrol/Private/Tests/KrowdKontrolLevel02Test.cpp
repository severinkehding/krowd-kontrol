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
#include "RunnerEnemy.h"
#include "TrooperEnemy.h"
#include "BomberEnemy.h"
#include "SniperEnemy.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Editor.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// Maps a placed enemy actor to the locked EEnemyType its concrete class represents
	// (RU-NNR=Runner, TR-UPR=Trooper, B0-0MR=Bomber, SN-1PR=Sniper - MISSION.md Hard
	// Invariant 5), so REQ-2's "one target zone per enemy type present in that room"
	// can be checked against what's actually placed, not just a non-zero target-zone
	// count.
	TOptional<EEnemyType> GetPlacedEnemyType(const AEnemyBase* Enemy)
	{
		if (Enemy->IsA<ARunnerEnemy>())
		{
			return EEnemyType::RU_NNR;
		}
		if (Enemy->IsA<ATrooperEnemy>())
		{
			return EEnemyType::TR_UPR;
		}
		if (Enemy->IsA<ABomberEnemy>())
		{
			return EEnemyType::B0_0MR;
		}
		if (Enemy->IsA<ASniperEnemy>())
		{
			return EEnemyType::SN_1PR;
		}
		return TOptional<EEnemyType>();
	}

	// Level02's enemies are statically placed within each room's spatial footprint -
	// unlike target zones (attached to their room via AddTargetZone()), no explicit
	// room/enemy link exists for static placeholder-density enemies, so nearest-room-
	// by-distance is how "which room is this enemy in" is determined. Rooms in a
	// hand-authored linear chain are spaced far enough apart that closest-room
	// assignment is unambiguous.
	ARoomActor* FindNearestRoom(const AActor* Enemy, const TArray<ARoomActor*>& Rooms)
	{
		ARoomActor* Nearest = nullptr;
		float NearestDistSq = TNumericLimits<float>::Max();
		for (ARoomActor* Room : Rooms)
		{
			const float DistSq = FVector::DistSquared(Enemy->GetActorLocation(), Room->GetActorLocation());
			if (DistSq < NearestDistSq)
			{
				NearestDistSq = DistSq;
				Nearest = Room;
			}
		}
		return Nearest;
	}
}

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
	TestEqual(TEXT("L_Level02 room count should match its design target of 4"), Rooms.Num(), 4);

	TArray<ADoorConnectorActor*> Doors;
	for (TActorIterator<ADoorConnectorActor> It(World); It; ++It)
	{
		Doors.Add(*It);
	}
	TestEqual(TEXT("L_Level02 should have 3 doors connecting its 4 rooms in a chain"), Doors.Num(), 3);

	// Count/individual-validity checks above don't rule out doors leaving a room
	// unreachable - walk the door graph to confirm every room is actually reachable,
	// not just that door count/validity look right.
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

	TMap<ARoomActor*, TSet<EEnemyType>> EnemyTypesByRoom;
	TMap<ARoomActor*, int32> EnemyCountByRoom;
	int32 TotalLevel02EnemyCount = 0;
	for (TActorIterator<AEnemyBase> It(World); It; ++It)
	{
		AEnemyBase* Enemy = *It;
		++TotalLevel02EnemyCount;
		ARoomActor* NearestRoom = FindNearestRoom(Enemy, Rooms);
		if (!NearestRoom)
		{
			continue;
		}
		EnemyCountByRoom.FindOrAdd(NearestRoom, 0)++;
		if (TOptional<EEnemyType> EnemyType = GetPlacedEnemyType(Enemy))
		{
			EnemyTypesByRoom.FindOrAdd(NearestRoom).Add(EnemyType.GetValue());
		}
	}

	for (ARoomActor* Room : Rooms)
	{
		TestTrue(TEXT("Every room should have at least one target zone (REQ-2)"), Room->GetTargetZones().Num() >= 1);

		// Lightweight per-room density check (PRD 05's "static placeholder-density
		// enemies") - only asserts every room has *some* enemy presence, not a specific
		// count, mirroring KrowdKontrolLevel01Test.cpp's own approach.
		TestTrue(TEXT("Every room should have at least one enemy placeholder placed in it (placeholder density)"),
			EnemyCountByRoom.FindRef(Room) >= 1);

		const TSet<EEnemyType>* PlacedTypes = EnemyTypesByRoom.Find(Room);
		if (!PlacedTypes)
		{
			continue;
		}
		for (EEnemyType PlacedType : *PlacedTypes)
		{
			const bool bHasMatchingTargetZone = Room->GetTargetZones().ContainsByPredicate(
				[PlacedType](const FRoomTargetZone& Zone) { return Zone.EnemyType == PlacedType; });
			TestTrue(
				FString::Printf(TEXT("Room should have a target zone matching each enemy type placed in it (REQ-2) - missing for %s"),
					*UEnum::GetDisplayValueAsText(PlacedType).ToString()),
				bHasMatchingTargetZone);
		}
	}

	TestTrue(TEXT("L_Level02's total enemy count should be strictly greater than L_Level01's (REQ-3 difficulty ramp)"),
		TotalLevel02EnemyCount > Level01EnemyCount);
	TestEqual(TEXT("L_Level02 total enemy count should match its design target of 8"), TotalLevel02EnemyCount, 8);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
