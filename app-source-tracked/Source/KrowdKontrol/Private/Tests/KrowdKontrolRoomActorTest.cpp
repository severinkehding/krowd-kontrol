// Confirms ARoomActor (issue #39, PRD 05 REQ-1/REQ-2) can be placed and supports an
// arbitrary number of tagged target-zone marker children: AddTargetZone() spawns a
// marker actor, attaches it as a genuine child of the room (not just tracked in the
// array), and records its EEnemyType tag - the mechanism hand-authored Level 1/2/3
// rooms will be built from.
//
// Needs a real UWorld to spawn into (AddTargetZone() calls GetWorld()->SpawnActor),
// same rationale as KrowdKontrolRoomEnemyBudgetControllerTest.cpp - FAutomationEditorCommonUtils::
// CreateNewMap() gives a real editor UWorld without needing PIE.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "RoomActor.h"
#include "EnemyType.h"
#include "PlaceholderCubeActor.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/LevelStructureTestUtils.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolRoomActorTest,
	"KrowdKontrol.Unit.RoomActor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolRoomActorTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	ARoomActor* Room = World->SpawnActor<ARoomActor>();
	if (!TestNotNull(TEXT("ARoomActor should spawn into the test World"), Room))
	{
		return false;
	}

	KrowdKontrolLevelTestUtils::CheckRoomsHaveFloorGeometry(*this, { Room });

	TestNotNull(TEXT("Room should have a north wall mesh component"), Room->WallNorthMeshComponent.Get());
	UStaticMesh* WallNorthStaticMesh = Room->WallNorthMeshComponent->GetStaticMesh();
	TestNotNull(TEXT("North wall mesh component should have a static mesh set"), WallNorthStaticMesh);
	TestNotNull(TEXT("Room should have a south wall mesh component"), Room->WallSouthMeshComponent.Get());
	UStaticMesh* WallSouthStaticMesh = Room->WallSouthMeshComponent->GetStaticMesh();
	TestNotNull(TEXT("South wall mesh component should have a static mesh set"), WallSouthStaticMesh);
	TestNotNull(TEXT("Room should have an east wall mesh component"), Room->WallEastMeshComponent.Get());
	UStaticMesh* WallEastStaticMesh = Room->WallEastMeshComponent->GetStaticMesh();
	TestNotNull(TEXT("East wall mesh component should have a static mesh set"), WallEastStaticMesh);
	TestNotNull(TEXT("Room should have a west wall mesh component"), Room->WallWestMeshComponent.Get());
	UStaticMesh* WallWestStaticMesh = Room->WallWestMeshComponent->GetStaticMesh();
	TestNotNull(TEXT("West wall mesh component should have a static mesh set"), WallWestStaticMesh);

	const FVector ExpectedFloorScale(
		Room->RoomFloorExtent.X * 2.f / 100.f, Room->RoomFloorExtent.Y * 2.f / 100.f, Room->RoomFloorThickness / 100.f);
	TestTrue(TEXT("Floor mesh's relative scale should be driven by RoomFloorExtent/RoomFloorThickness"),
		Room->FloorMeshComponent->GetRelativeScale3D().Equals(ExpectedFloorScale, 0.01f));

	const FVector ExpectedNorthWallScale(
		Room->RoomFloorExtent.X * 2.f / 100.f, Room->RoomWallThickness / 100.f, Room->RoomWallHeight / 100.f);
	TestTrue(TEXT("North wall's relative scale should be driven by RoomFloorExtent/RoomWallThickness/RoomWallHeight"),
		Room->WallNorthMeshComponent->GetRelativeScale3D().Equals(ExpectedNorthWallScale, 0.01f));
	const FVector ExpectedNorthWallLocation(0.f, Room->RoomFloorExtent.Y, Room->RoomWallHeight * 0.5f);
	TestTrue(TEXT("North wall's relative location should sit at +Y room extent, half wall height up"),
		Room->WallNorthMeshComponent->GetRelativeLocation().Equals(ExpectedNorthWallLocation, 0.01f));

	AActor* MarkerActor = Room->AddTargetZone(EEnemyType::RU_NNR);
	if (!TestNotNull(TEXT("AddTargetZone should spawn and return a marker actor"), MarkerActor))
	{
		return false;
	}

	TestEqual(TEXT("Room should have one target zone after one AddTargetZone call"),
		Room->GetTargetZones().Num(), 1);
	TestEqual(TEXT("Recorded target zone's enemy type should match the requested tag"),
		static_cast<uint8>(Room->GetTargetZones()[0].EnemyType), static_cast<uint8>(EEnemyType::RU_NNR));
	TestEqual(TEXT("Recorded target zone's marker actor should be the one AddTargetZone returned"),
		Room->GetTargetZones()[0].MarkerActor.Get(), MarkerActor);
	TestTrue(TEXT("Marker actor should genuinely be attached to the room, not just tracked in the array"),
		MarkerActor->IsAttachedTo(Room));

	AActor* SecondMarkerActor = Room->AddTargetZone(EEnemyType::TR_UPR);
	if (!TestNotNull(TEXT("A second AddTargetZone call should also spawn a marker actor"), SecondMarkerActor))
	{
		return false;
	}

	TestEqual(TEXT("Room should support an arbitrary number of independently tagged target zones"),
		Room->GetTargetZones().Num(), 2);
	TestEqual(TEXT("Second target zone's enemy type should match its own requested tag"),
		static_cast<uint8>(Room->GetTargetZones()[1].EnemyType), static_cast<uint8>(EEnemyType::TR_UPR));

	AActor* CustomMarkerActor = Room->AddTargetZone(EEnemyType::B0_0MR, APlaceholderCubeActor::StaticClass());
	if (!TestNotNull(TEXT("AddTargetZone should spawn a marker actor when given an explicit MarkerClass"), CustomMarkerActor))
	{
		return false;
	}
	TestTrue(TEXT("An explicit MarkerClass should be honored instead of the default placeholder"),
		CustomMarkerActor->IsA(APlaceholderCubeActor::StaticClass()));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
