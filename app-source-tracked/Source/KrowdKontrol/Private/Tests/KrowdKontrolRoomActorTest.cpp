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
#include "Engine/World.h"

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
