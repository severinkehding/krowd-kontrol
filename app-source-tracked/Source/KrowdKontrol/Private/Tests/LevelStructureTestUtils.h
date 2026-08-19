#pragma once

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "RoomActor.h"
#include "DoorConnectorActor.h"
#include "EnemyBase.h"
#include "RunnerEnemy.h"
#include "TrooperEnemy.h"
#include "BomberEnemy.h"
#include "SniperEnemy.h"

// Shared helpers for the KrowdKontrolLevel0*Test.cpp structural regression tests
// (KrowdKontrolLevel01Test.cpp, KrowdKontrolLevel02Test.cpp, and future Level03-05
// tests per MISSION.md's 5-level Alpha roster) - extracted after code review flagged
// the two functions below as byte-identical, verbatim-duplicated per level test file.
namespace KrowdKontrolLevelTestUtils
{
	// Maps a placed enemy actor to the locked EEnemyType its concrete class represents
	// (RU-NNR=Runner, TR-UPR=Trooper, B0-0MR=Bomber, SN-1PR=Sniper - MISSION.md Hard
	// Invariant 5), so REQ-2's "one target zone per enemy type present in that room"
	// can be checked against what's actually placed, not just a non-zero target-zone
	// count.
	inline TOptional<EEnemyType> GetPlacedEnemyType(const AEnemyBase* Enemy)
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

	// Each level's enemies are statically placed within each room's spatial footprint -
	// unlike target zones (attached to their room via AddTargetZone()), no explicit
	// room/enemy link exists for static placeholder-density enemies, so nearest-room-
	// by-distance is how "which room is this enemy in" is determined. Rooms in a
	// hand-authored linear chain are spaced far enough apart that closest-room
	// assignment is unambiguous.
	inline ARoomActor* FindNearestRoom(const AActor* Enemy, const TArray<ARoomActor*>& Rooms)
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

	// Count/individual-validity checks on rooms and doors don't rule out doors leaving
	// a room unreachable (e.g. both doors wiring the same pair of rooms) - walks the
	// door adjacency graph (BFS) to confirm every room is actually reachable from the
	// first one, not just that door count/validity look right.
	inline void CheckAllRoomsReachableViaDoors(FAutomationTestBase& Test, const TArray<ARoomActor*>& Rooms, const TArray<ADoorConnectorActor*>& Doors)
	{
		TMap<ARoomActor*, TArray<ARoomActor*>> Adjacency;
		for (ADoorConnectorActor* Door : Doors)
		{
			Test.TestTrue(TEXT("Each door should connect two valid, distinct rooms"), Door->ConnectsValidRooms());
			if (Door->ConnectsValidRooms())
			{
				Adjacency.FindOrAdd(Door->RoomA).Add(Door->RoomB);
				Adjacency.FindOrAdd(Door->RoomB).Add(Door->RoomA);
			}
		}

		if (Rooms.Num() == 0)
		{
			return;
		}

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
		Test.TestEqual(TEXT("All rooms should be reachable via doors (no room isolated from the chain)"),
			Visited.Num(), Rooms.Num());
	}

	// Asserts every room has >=1 target zone and >=1 enemy placeholder (REQ-2's
	// placeholder-density check), and that every distinct enemy type placed in a room
	// (per EnemyTypesByRoom, keyed by nearest-room-by-distance) has a target zone of
	// the matching EEnemyType in that same room.
	inline void CheckRoomTargetZonesAndDensity(
		FAutomationTestBase& Test,
		const TArray<ARoomActor*>& Rooms,
		const TMap<ARoomActor*, TSet<EEnemyType>>& EnemyTypesByRoom,
		const TMap<ARoomActor*, int32>& EnemyCountByRoom)
	{
		for (ARoomActor* Room : Rooms)
		{
			Test.TestTrue(TEXT("Every room should have at least one target zone (REQ-2)"), Room->GetTargetZones().Num() >= 1);
			Test.TestTrue(TEXT("Every room should have at least one enemy placeholder placed in it (placeholder density)"),
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
				Test.TestTrue(
					FString::Printf(TEXT("Room should have a target zone matching each enemy type placed in it (REQ-2) - missing for %s"),
						*UEnum::GetDisplayValueAsText(PlacedType).ToString()),
					bHasMatchingTargetZone);
			}
		}
	}
}
