// Issue #243 validation-pass-1 follow-up: the issue's acceptance criteria include an
// unconditional "manually walk the full perimeter of every room in PIE and confirm no
// route exists into a room other than through its gated door" step. No PIE
// pawn-input-simulation primitive exists in this factory's holdout/harness
// environment, so that step can't literally be performed - but the same claim (a real
// player-sized probe cannot slip through a sealed wall, flank, or corridor guard rail,
// and CAN pass through an intentionally open doorway/corridor) can be checked with a
// real physics query instead of a one-off manual observation.
//
// KrowdKontrolRoomActorPerimeterSealingTest.cpp and KrowdKontrolDoorConnectorActorTest.cpp
// already assert the *component* properties (CollisionEnabled, response-to-channel,
// box extent/position) that are supposed to add up to a sealed perimeter. This test
// goes one level further and asks the physics engine directly, at each probe point: is
// there any primitive here at all (World::OverlapMultiByObjectType against
// AllObjects), and if so, is one of the hits actually configured as a WorldStatic
// blocking volume (UPrimitiveComponent::GetCollisionObjectType() == ECC_WorldStatic
// with collision enabled) - the exact recipe
// ARoomActor::ConfigureWorldDynamicBlockingCollision() applies to every wall/flank/
// guard-rail volume (RoomActor.cpp, issue #243 Finding 3), so a wrong extent, a missed
// flank, or a mis-placed guard rail shows up here as a probe that should be blocked
// sailing through clean, not just as a stale collision flag.
//
// The query explicitly sets FCollisionQueryParams::bTraceComplex = false. Confirmed
// empirically: without it (the default is bTraceComplex = true), this headless
// -nullrhi Automation run fails to find the room walls' own UStaticMeshComponent hits
// at all (their box simple collision is present and correctly configured - only the
// complex/per-poly path silently comes back empty here, with no such gap for the
// procedural flank/guard-rail UBoxComponents, which have no complex representation to
// prefer). Forcing simple-only matches how these volumes are actually meant to be
// queried (RoomActor.cpp's own recipe never relies on complex collision) and removes
// that environment-specific gap.
//
// Uses the same CreateNewMap() + InitializeActorsForPlay(FURL()) + SetBegunPlay(true)
// scaffold as the perimeter-sealing test, so SpawnActor()/FinishSpawning() dispatch
// BeginPlay (and therefore SealRoomPerimeter()/RecomputeConnectorGeometry())
// immediately - no PIE session is required for the physics scene these queries hit.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "RoomActor.h"
#include "DoorConnectorActor.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "CollisionShape.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "WorldCollision.h"
#include "Engine/OverlapResult.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolRoomPerimeterPhysicsProbeTest,
	"KrowdKontrol.Unit.RoomPerimeterPhysicsProbe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace
{
	// Real physics query - a small sphere rather than a point test so the probe
	// behaves like a player-sized obstruction check, not an infinitesimal ray that
	// could graze past a valid volume on floating-point luck. Queries AllObjects and
	// filters the hit list in test code (see file header) for a component actually
	// configured as a WorldStatic blocking volume.
	bool IsPointPhysicallyBlocked(UWorld* World, const FVector& Point)
	{
		FCollisionQueryParams QueryParams;
		QueryParams.bTraceComplex = false;

		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByObjectType(
			Overlaps, Point, FQuat::Identity,
			FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllObjects),
			FCollisionShape::MakeSphere(5.f), QueryParams);

		for (const FOverlapResult& Overlap : Overlaps)
		{
			UPrimitiveComponent* Component = Overlap.GetComponent();
			if (Component &&
				Component->GetCollisionObjectType() == ECC_WorldStatic &&
				Component->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
			{
				return true;
			}
		}
		return false;
	}

	void ProbeExpectBlocked(FAutomationTestBase& Test, UWorld* World, const TCHAR* Description, const FVector& Point)
	{
		Test.TestTrue(FString::Printf(TEXT("%s should be physically blocked (WorldStatic overlap probe)"), Description),
			IsPointPhysicallyBlocked(World, Point));
	}

	void ProbeExpectOpen(FAutomationTestBase& Test, UWorld* World, const TCHAR* Description, const FVector& Point)
	{
		Test.TestFalse(FString::Printf(TEXT("%s should stay walkable (WorldStatic overlap probe)"), Description),
			IsPointPhysicallyBlocked(World, Point));
	}
}

bool FKrowdKontrolRoomPerimeterPhysicsProbeTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}
	World->InitializeActorsForPlay(FURL());
	World->SetBegunPlay(true);

	// Room with an East door - same topology KrowdKontrolRoomActorPerimeterSealingTest's
	// case (2) uses, so its already-verified component-level expectations (NoCollision
	// East wall mesh, two QueryOnly flanks, open GateBlockingComponent for a room with
	// no owned enemies) are the ground truth these physics probes are cross-checking.
	ARoomActor* Room = World->SpawnActor<ARoomActor>(ARoomActor::StaticClass(), FTransform(FVector(0.f, 0.f, 0.f)));
	if (!TestNotNull(TEXT("ARoomActor should spawn into the test World"), Room))
	{
		return false;
	}

	ARoomActor* Neighbor = World->SpawnActor<ARoomActor>(ARoomActor::StaticClass(), FTransform(FVector(3000.f, 0.f, 0.f)));
	if (!TestNotNull(TEXT("Neighbor ARoomActor should spawn into the test World"), Neighbor))
	{
		return false;
	}

	ADoorConnectorActor* Door = World->SpawnActorDeferred<ADoorConnectorActor>(ADoorConnectorActor::StaticClass(), FTransform::Identity);
	if (!TestNotNull(TEXT("ADoorConnectorActor should spawn into the test World"), Door))
	{
		return false;
	}
	Door->RoomA = Room;
	Door->RoomB = Neighbor;
	Door->FinishSpawning(FTransform::Identity);

	// The door now exists - re-seal (public, idempotent) so Room's East side picks it
	// up, mirroring the perimeter-sealing test's own two-phase spawn order.
	Room->SealRoomPerimeter();

	const float WallProbeHeight = Room->RoomWallHeight * 0.5f;
	const float GapHalfWidth = Door->ConnectorFloorWidth * 0.5f;

	// Walls with no connecting door: a probe at each solid wall's own centre must be
	// physically blocked.
	ProbeExpectBlocked(*this, World, TEXT("North wall centre"),
		FVector(0.f, Room->RoomFloorExtent.Y, WallProbeHeight));
	ProbeExpectBlocked(*this, World, TEXT("South wall centre"),
		FVector(0.f, -Room->RoomFloorExtent.Y, WallProbeHeight));
	ProbeExpectBlocked(*this, World, TEXT("West wall centre"),
		FVector(-Room->RoomFloorExtent.X, 0.f, WallProbeHeight));

	// Corner span near the North/East corner: still inside the North wall's full-width
	// mesh (it spans the room's entire X extent, closing the corner) even though the
	// East side itself is gapped for the door.
	ProbeExpectBlocked(*this, World, TEXT("North/East corner span"),
		FVector(Room->RoomFloorExtent.X - 10.f, Room->RoomFloorExtent.Y, WallProbeHeight));

	// East wall's gap flanks: solid just outside the door's own gap span, on both
	// sides.
	ProbeExpectBlocked(*this, World, TEXT("East wall flank (positive side)"),
		FVector(Room->RoomFloorExtent.X, GapHalfWidth + 50.f, WallProbeHeight));
	ProbeExpectBlocked(*this, World, TEXT("East wall flank (negative side)"),
		FVector(Room->RoomFloorExtent.X, -(GapHalfWidth + 50.f), WallProbeHeight));

	// The door's own gap centre: open, since a room with no owned enemies is
	// vacuously cleared and GateBlockingComponent starts ungated (ADoorConnectorActor::
	// bIsGateOpen defaults true, reconciled by RefreshGateState()).
	ProbeExpectOpen(*this, World, TEXT("Door gap centre"),
		FVector(Room->RoomFloorExtent.X, 0.f, WallProbeHeight));

	// Corridor guard rails: solid at each rail's own centre, matching
	// KrowdKontrolDoorConnectorActorTest's geometry assertions for the same components.
	ProbeExpectBlocked(*this, World, TEXT("Corridor guard rail A centre"),
		Door->CorridorGuardRailAComponent->GetComponentLocation());
	ProbeExpectBlocked(*this, World, TEXT("Corridor guard rail B centre"),
		Door->CorridorGuardRailBComponent->GetComponentLocation());

	// Corridor centreline, between the two guard rails: open, so the corridor itself
	// stays walkable between the two rooms.
	const FVector CorridorMidpoint = (Door->CorridorGuardRailAComponent->GetComponentLocation() +
		Door->CorridorGuardRailBComponent->GetComponentLocation()) * 0.5f;
	ProbeExpectOpen(*this, World, TEXT("Corridor centreline"), CorridorMidpoint);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
