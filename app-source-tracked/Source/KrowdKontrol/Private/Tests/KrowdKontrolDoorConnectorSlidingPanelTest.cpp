// Pins ADoorConnectorActor's sliding door leaves (operator scene brief,
// 2026-09-02): the leaves cache a valid door plane from the connector
// geometry, rest closed at the door centre, slide apart when an enemy is
// within DoorProximityRadius of an OPEN gate, glide back when it leaves, and
// never open while the gate is locked. Drives TickDoorPanels() directly (the
// friend seam) so the gate-state polling in Tick() can't flip bIsGateOpen
// mid-assertion.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the
// other KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "DoorConnectorActor.h"
#include "RoomActor.h"
#include "RunnerEnemy.h"
#include "Components/StaticMeshComponent.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolDoorConnectorSlidingPanelTest,
	"KrowdKontrol.Unit.DoorConnectorSlidingPanel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolDoorConnectorSlidingPanelTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	ARoomActor* RoomA = World->SpawnActor<ARoomActor>(FVector::ZeroVector, FRotator::ZeroRotator);
	ARoomActor* RoomB = World->SpawnActor<ARoomActor>(FVector(2400.f, 0.f, 0.f), FRotator::ZeroRotator);
	ADoorConnectorActor* Door = World->SpawnActor<ADoorConnectorActor>(FVector(1200.f, 0.f, 0.f), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("ADoorConnectorActor should spawn into the test World"), Door))
	{
		return false;
	}
	Door->RoomA = RoomA;
	Door->RoomB = RoomB;
	Door->RecomputeConnectorGeometry();

	TestTrue(TEXT("Valid rooms should produce a cached door plane"), Door->bDoorPlaneValid);
	TestTrue(TEXT("The leaves should be visible on a valid connector"),
		Door->DoorPanelLeftComponent->IsVisible());
	TestEqual(TEXT("Closed leaves rest together at the door centre"),
		Door->DoorPanelLeftComponent->GetComponentLocation(), FVector(1200.f, 0.f, 0.f), 1.f);

	// (a) locked gate: even a nearby enemy must not open the leaves.
	ARunnerEnemy* Enemy = World->SpawnActor<ARunnerEnemy>(FVector(1200.f, 100.f, 0.f), FRotator::ZeroRotator);
	Door->bIsGateOpen = false;
	Door->TickDoorPanels(1.0f);
	TestEqual(TEXT("A locked gate keeps the leaves shut regardless of proximity"),
		Door->DoorPanelSlide01, 0.0f, KINDA_SMALL_NUMBER);

	// (b) open gate + nearby enemy: the leaves slide apart over time.
	Door->bIsGateOpen = true;
	Door->TickDoorPanels(0.25f);
	const float PartialSlide = Door->DoorPanelSlide01;
	TestTrue(TEXT("An open gate with someone at the door starts sliding open"), PartialSlide > 0.0f);
	TestTrue(TEXT("A quarter second is not enough for a full slide (it animates, not snaps)"), PartialSlide < 1.0f);
	Door->TickDoorPanels(2.0f);
	TestEqual(TEXT("The leaves should reach fully open"), Door->DoorPanelSlide01, 1.0f, KINDA_SMALL_NUMBER);
	TestTrue(TEXT("The left leaf should have slid laterally away from centre"),
		FMath::Abs(Door->DoorPanelLeftComponent->GetComponentLocation().Y) > Door->DoorSlideDistance * 0.9f);

	// (c) everyone leaves: the leaves glide back shut.
	Enemy->SetActorLocation(FVector(1200.f, 5000.f, 0.f));
	Door->TickDoorPanels(2.0f);
	TestEqual(TEXT("With nobody near, the leaves should glide fully shut again"),
		Door->DoorPanelSlide01, 0.0f, KINDA_SMALL_NUMBER);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
