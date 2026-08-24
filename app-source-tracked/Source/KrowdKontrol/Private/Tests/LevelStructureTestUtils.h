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
#include "TargetZone.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"

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
	// assignment is unambiguous. Delegates to ARoomActor::FindNearestRoom (issue #218)
	// rather than duplicating the comparison, so this test utility's expectations and
	// ARoomActor::BeginPlay's own owned-enemy auto-discovery can never drift apart.
	inline ARoomActor* FindNearestRoom(const AActor* Enemy, const TArray<ARoomActor*>& Rooms)
	{
		return ARoomActor::FindNearestRoom(Enemy, Rooms);
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

	// Asserts every room has a floor mesh with a valid UStaticMesh set (REQ-3 - no
	// void anywhere along the playable path) and that collision is split correctly:
	// the floor keeps blocking collision, and walls stay non-blocking here specifically
	// because this asserts pre-BeginPlay construction-time state - the only context this
	// helper is ever called from (KrowdKontrolRoomActorTest.cpp, which never dispatches
	// BeginPlay). At real play time, ARoomActor::SealRoomPerimeter() (issue #243) enables
	// blocking collision on any wall side with no connecting door, and gaps the sides
	// that have one - see KrowdKontrolRoomActorPerimeterSealingTest.cpp for that coverage.
	inline void CheckRoomsHaveFloorGeometry(FAutomationTestBase& Test, const TArray<ARoomActor*>& Rooms)
	{
		for (ARoomActor* Room : Rooms)
		{
			Test.TestNotNull(TEXT("Room should have a floor mesh component (REQ-3)"), Room->FloorMeshComponent.Get());
			if (Room->FloorMeshComponent)
			{
				UStaticMesh* FloorStaticMesh = Room->FloorMeshComponent->GetStaticMesh();
				Test.TestNotNull(TEXT("Room's floor mesh component should have a static mesh set (REQ-3)"), FloorStaticMesh);
				Test.TestTrue(TEXT("Room's floor should keep blocking collision (REQ-3)"),
					Room->FloorMeshComponent->GetCollisionEnabled() != ECollisionEnabled::NoCollision);
			}
			for (UStaticMeshComponent* Wall : { Room->WallNorthMeshComponent.Get(), Room->WallSouthMeshComponent.Get(),
				Room->WallEastMeshComponent.Get(), Room->WallWestMeshComponent.Get() })
			{
				if (Wall)
				{
					Test.TestEqual(TEXT("Room walls must not block connector paths (REQ-3)"),
						Wall->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
				}
			}
		}
	}

	// Asserts every door that connects two valid, distinct rooms has a visible
	// connector floor after RecomputeConnectorGeometry() is (re)called - safe/
	// idempotent to call from a test, and necessary here because
	// FAutomationEditorCommonUtils::LoadMap does not start play, so BeginPlay (and
	// its call to RecomputeConnectorGeometry()) never fires under this test path.
	inline void CheckDoorsHaveConnectorGeometry(FAutomationTestBase& Test, const TArray<ADoorConnectorActor*>& Doors)
	{
		for (ADoorConnectorActor* Door : Doors)
		{
			if (!Door->ConnectsValidRooms())
			{
				continue;
			}
			Door->RecomputeConnectorGeometry();
			Test.TestTrue(TEXT("Door's connector floor mesh should be visible once it connects two valid rooms (REQ-3)"),
				Door->ConnectorFloorMeshComponent->IsVisible());
			Test.TestTrue(TEXT("Door's connector floor mesh should keep blocking collision (REQ-3)"),
				Door->ConnectorFloorMeshComponent->GetCollisionEnabled() != ECollisionEnabled::NoCollision);
		}
	}

	// Asserts every door that connects two valid, distinct rooms has a visible
	// marker mesh (issue #191) after RecomputeConnectorGeometry() is (re)called -
	// same idempotency/BeginPlay-timing rationale as CheckDoorsHaveConnectorGeometry
	// above (FAutomationEditorCommonUtils::LoadMap does not start play).
	inline void CheckDoorsHaveVisibleMarker(FAutomationTestBase& Test, const TArray<ADoorConnectorActor*>& Doors)
	{
		for (ADoorConnectorActor* Door : Doors)
		{
			if (!Door->ConnectsValidRooms())
			{
				continue;
			}
			Door->RecomputeConnectorGeometry();
			Test.TestTrue(TEXT("Door's marker mesh should be visible once it connects two valid rooms (issue #191)"),
				Door->DoorMarkerMeshComponent->IsVisible());
			Test.TestTrue(TEXT("Door's marker light should be visible once it connects two valid rooms (issue #191)"),
				Door->DoorMarkerLightComponent->IsVisible());
			Test.TestTrue(TEXT("Door's marker mesh should have no collision so it never blocks the connector path (issue #191)"),
				Door->DoorMarkerMeshComponent->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
		}
	}

	// Shared by CheckAdjacentRoomSpacingCompressed and CheckEnemyDensityRamp below -
	// both need rooms in chain order (by X) to compare adjacent/entrance rooms.
	inline TArray<ARoomActor*> SortRoomsByX(const TArray<ARoomActor*>& Rooms)
	{
		TArray<ARoomActor*> SortedRooms = Rooms;
		SortedRooms.Sort([](const ARoomActor& A, const ARoomActor& B) { return A.GetActorLocation().X < B.GetActorLocation().X; });
		return SortedRooms;
	}

	// Asserts adjacent rooms (sorted by X) sit closer together than the pre-#189
	// 3000cm baseline, without hardcoding the exact new spacing value - catches a
	// regression back to the original spacing while still tolerating a future minor
	// retune (issue #189).
	inline void CheckAdjacentRoomSpacingCompressed(FAutomationTestBase& Test, const TArray<ARoomActor*>& Rooms)
	{
		TArray<ARoomActor*> SortedRooms = SortRoomsByX(Rooms);

		for (int32 Index = 1; Index < SortedRooms.Num(); ++Index)
		{
			const float Distance = SortedRooms[Index]->GetActorLocation().X - SortedRooms[Index - 1]->GetActorLocation().X;
			Test.TestTrue(TEXT("Adjacent room spacing should be compressed below the pre-#189 3000cm baseline"),
				Distance < 3000.f);
		}
	}

	// Asserts the entrance room (lowest X) has 1-2 enemies and that per-room enemy
	// counts (sorted by room X) strictly increase at every step - proves an actual
	// density ramp, not a flat line (issue #189). A single-room level has no adjacent
	// pair to ramp across, so it's vacuously fine - similar in spirit to
	// CheckAllRoomsReachableViaDoors's zero-room guard above, though this guard also
	// skips the single-room case (Num() <= 1, not just Num() == 0).
	inline void CheckEnemyDensityRamp(
		FAutomationTestBase& Test,
		const TArray<ARoomActor*>& Rooms,
		const TMap<ARoomActor*, int32>& EnemyCountByRoom)
	{
		TArray<ARoomActor*> SortedRooms = SortRoomsByX(Rooms);

		if (SortedRooms.Num() <= 1)
		{
			return;
		}

		const int32 FirstCount = EnemyCountByRoom.FindRef(SortedRooms[0]);
		Test.TestTrue(TEXT("Entrance room's enemy count should be 1-2 (issue #189)"),
			FirstCount >= 1 && FirstCount <= 2);

		int32 PreviousCount = FirstCount;
		for (int32 Index = 1; Index < SortedRooms.Num(); ++Index)
		{
			const int32 CurrentCount = EnemyCountByRoom.FindRef(SortedRooms[Index]);
			Test.TestTrue(TEXT("Enemy density should strictly increase room-to-room, proving a real ramp (issue #189)"),
				CurrentCount > PreviousCount);
			PreviousCount = CurrentCount;
		}
	}

	// Issue #211 pass-1 review follow-up: CheckRoomTargetZonesAndDensity above only
	// proves each real room's TargetZones marker array is non-empty - it says nothing
	// about whether ARoomActor::EnsureBankingZonesWired() (the actual self-heal this
	// issue adds) has anything real to act on for these shipped levels, since
	// FAutomationEditorCommonUtils::LoadMap never fires BeginPlay. Calls
	// EnsureBankingZonesWired() directly (public, idempotent - safe here for the same
	// reason KrowdKontrolRoomActorBankingWiringTest.cpp calls it a second time) against
	// every already-placed marker in the loaded level, then asserts each one now has an
	// attached, non-default-coloured ATargetZone - proof the self-heal mechanism
	// produces a real result against this level's actual marker data, not just against
	// the synthetic room KrowdKontrolRoomActorBankingWiringTest.cpp builds from scratch.
	inline void CheckRoomBankingZonesSelfHeal(FAutomationTestBase& Test, const TArray<ARoomActor*>& Rooms)
	{
		for (ARoomActor* Room : Rooms)
		{
			Room->EnsureBankingZonesWired();

			for (const FRoomTargetZone& Zone : Room->GetTargetZones())
			{
				if (!Zone.MarkerActor)
				{
					continue;
				}

				TArray<AActor*> Attached;
				Zone.MarkerActor->GetAttachedActors(Attached);
				ATargetZone* BankingZone = nullptr;
				for (AActor* AttachedActor : Attached)
				{
					if (ATargetZone* Candidate = Cast<ATargetZone>(AttachedActor))
					{
						BankingZone = Candidate;
						break;
					}
				}

				if (Test.TestNotNull(TEXT("Every real target-zone marker should have a self-healed ATargetZone attached after EnsureBankingZonesWired() (issue #211)"), BankingZone))
				{
					Test.TestNotEqual(TEXT("The self-healed ATargetZone's ZoneColourTag should resolve to a real colour, not the NAME_None safe default (issue #211)"),
						BankingZone->ZoneColourTag, NAME_None);
				}
			}
		}
	}
}
