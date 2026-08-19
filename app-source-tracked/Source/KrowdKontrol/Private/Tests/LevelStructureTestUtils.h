#pragma once

#include "CoreMinimal.h"
#include "RoomActor.h"
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
}
