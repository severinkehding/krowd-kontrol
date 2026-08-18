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

	// Level01's enemies are statically placed within each room's spatial footprint -
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

	TMap<ARoomActor*, TSet<EEnemyType>> EnemyTypesByRoom;
	TMap<ARoomActor*, int32> EnemyCountByRoom;
	for (TActorIterator<AEnemyBase> It(World); It; ++It)
	{
		AEnemyBase* Enemy = *It;
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
		// count, since a real per-level density target needs Levels 2-5 to exist for
		// comparison and is out of this test's reach.
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

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
