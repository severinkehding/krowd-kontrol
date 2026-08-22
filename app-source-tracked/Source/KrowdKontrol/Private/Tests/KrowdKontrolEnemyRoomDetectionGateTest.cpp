// Confirms issue #244: AEnemyBase::TickCheckDetection's Idle->Alert transition is
// gated on the player being resolved (via ARoomActor::FindNearestRoom's existing
// nearest-room-by-distance rule) to the enemy's own OwningRoom, not on proximity
// alone. Covers both explicit ACs (gated while the player is outside the owning
// room but still within DetectionRangeUnits; ungated once the player resolves to
// the owning room) plus the no-owning-room legacy fallback. Bare CreateNewMap() is
// sufficient - this test drives TickCheckDetection directly via the friend grant,
// never relying on a dynamic-multicast-delegate broadcast between spawned actors
// (unlike KrowdKontrolRoomActorDoorGatingTest.cpp, which needs
// InitializeActorsForPlay()/SetBegunPlay() for exactly that reason).
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "RoomActor.h"
#include "EnemyBaseTestActor.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolEnemyRoomDetectionGateTest,
	"KrowdKontrol.Unit.EnemyRoomDetectionGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolEnemyRoomDetectionGateTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	// Rooms spaced 3000cm apart, matching the hand-authored-level convention
	// documented at RoomActor.h:104-106.
	ARoomActor* OwnRoom = World->SpawnActor<ARoomActor>(ARoomActor::StaticClass(), FTransform(FVector(0.f, 0.f, 0.f)));
	ARoomActor* OtherRoom = World->SpawnActor<ARoomActor>(ARoomActor::StaticClass(), FTransform(FVector(3000.f, 0.f, 0.f)));
	if (!TestNotNull(TEXT("OwnRoom should spawn into the test World"), OwnRoom) ||
		!TestNotNull(TEXT("OtherRoom should spawn into the test World"), OtherRoom))
	{
		return false;
	}

	AEnemyBaseTestActor* GatedEnemy = World->SpawnActor<AEnemyBaseTestActor>(AEnemyBaseTestActor::StaticClass(), FTransform(FVector(1400.f, 0.f, 0.f)));
	if (!TestNotNull(TEXT("GatedEnemy should spawn into the test World"), GatedEnemy))
	{
		return false;
	}

	OwnRoom->AddOwnedEnemy(GatedEnemy);
	TestEqual(TEXT("AddOwnedEnemy should set the enemy's OwningRoom back-reference"),
		GatedEnemy->GetOwningRoom(), OwnRoom);

	// (1) Player outside the owned room, within DetectionRangeUnits: FVector(1600,0,0)
	// resolves nearest to OtherRoom (|1600-3000|=1400 < |1600-0|=1600) but is only 200
	// units from GatedEnemy (well inside the default 1500 DetectionRangeUnits) - the
	// issue's core regression case.
	GatedEnemy->TickCheckDetection(FVector(1600.f, 0.f, 0.f));
	TestEqual(TEXT("Idle->Alert should stay gated while the player's nearest room differs from OwningRoom, even within DetectionRangeUnits"),
		static_cast<uint8>(GatedEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Idle));

	// (2) Player inside the owned room: FVector(1300,0,0) resolves nearest to OwnRoom
	// (|1300-0|=1300 < |1300-3000|=1700), 100 units from GatedEnemy.
	GatedEnemy->TickCheckDetection(FVector(1300.f, 0.f, 0.f));
	TestEqual(TEXT("Idle->Alert should proceed once the player's nearest room equals OwningRoom"),
		static_cast<uint8>(GatedEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));

	// (3) No owning room -> unscoped legacy fallback: never passed to AddOwnedEnemy(),
	// so OwningRoom stays null and proximity alone should still advance Idle->Alert.
	AEnemyBaseTestActor* UnownedEnemy = World->SpawnActor<AEnemyBaseTestActor>(AEnemyBaseTestActor::StaticClass(), FTransform(FVector(3100.f, 0.f, 0.f)));
	if (!TestNotNull(TEXT("UnownedEnemy should spawn into the test World"), UnownedEnemy))
	{
		return false;
	}
	TestNull(TEXT("An enemy never added via AddOwnedEnemy() should have a null OwningRoom"), UnownedEnemy->GetOwningRoom());

	UnownedEnemy->TickCheckDetection(FVector(3200.f, 0.f, 0.f));
	TestEqual(TEXT("An enemy with no OwningRoom should keep unscoped proximity-only Idle->Alert behaviour"),
		static_cast<uint8>(UnownedEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));

	// (4) Already-Alert gated enemy should still reach Attack purely on distance, even
	// with the player outside OwningRoom - proves the room gate is confined to
	// Idle->Alert per the issue's "already-Alert enemies are unaffected" AC. Spawned at
	// the same (1400,0,0) as GatedEnemy above so the same case-(2) player location
	// (1300,0,0) resolves nearest to OwnRoom and advances it to Alert. AEnemyBaseTestActor
	// doesn't override GetAttackRangeUnits() (base default 0.0f - see
	// KrowdKontrolEnemyBaseTest.cpp's boundary case), so triggering Alert->Attack needs
	// Distance == 0 exactly; the enemy is relocated to OtherRoom's territory first so
	// that zero-distance point still resolves away from OwningRoom, isolating "is the
	// player in OwningRoom" from "is Alert->Attack gated on it" (it isn't).
	AEnemyBaseTestActor* AlertGatedEnemy = World->SpawnActor<AEnemyBaseTestActor>(AEnemyBaseTestActor::StaticClass(), FTransform(FVector(1400.f, 0.f, 0.f)));
	if (!TestNotNull(TEXT("AlertGatedEnemy should spawn into the test World"), AlertGatedEnemy))
	{
		return false;
	}
	OwnRoom->AddOwnedEnemy(AlertGatedEnemy);
	AlertGatedEnemy->TickCheckDetection(FVector(1300.f, 0.f, 0.f)); // inside OwnRoom -> Idle->Alert
	TestEqual(TEXT("Setup: AlertGatedEnemy should be Alert before the regression check"),
		static_cast<uint8>(AlertGatedEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));

	// Relocate the enemy itself into OtherRoom's territory: FindNearestRoom((2900,0,0))
	// resolves to OtherRoom (|2900-3000|=100 < |2900-0|=2900), while OwningRoom (set via
	// AddOwnedEnemy above) stays OwnRoom - i.e. the player, standing where the enemy now
	// is, is genuinely outside OwningRoom by the same rule case (1) uses.
	AlertGatedEnemy->SetActorLocation(FVector(2900.f, 0.f, 0.f));
	AlertGatedEnemy->TickCheckDetection(FVector(2900.f, 0.f, 0.f)); // Distance 0 -> within GetAttackRangeUnits()
	TestEqual(TEXT("Alert->Attack should proceed on distance alone even when the player has left OwningRoom"),
		static_cast<uint8>(AlertGatedEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Attack));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
