// Confirms ARoomActor::SealRoomPerimeter() (issue #243, PRD Room Encounter Flow
// REQ-1): a wall side with no connecting ADoorConnectorActor gets real blocking
// collision (QueryOnly, Block ECC_WorldDynamic - the channel the real player pawn
// actually presents, issue #218's regression), while a side with a connecting door
// gets a matching-width gap flanked by invisible blocking UBoxComponents, leaving the
// doorway itself open for ADoorConnectorActor's own GateBlockingComponent to gate.
//
// Uses the same CreateNewMap() + World->InitializeActorsForPlay(FURL()) +
// World->SetBegunPlay(true) scaffold as KrowdKontrolRoomActorDoorGatingTest.cpp, so
// SpawnActor() auto-dispatches BeginPlay() (and therefore SealRoomPerimeter())
// immediately. A room spawned before its connecting door exists in the World seals
// itself with no doors found (documented edge case - see RoomActor.cpp) - each case
// below spawns the room first, then its door(s), then calls SealRoomPerimeter() again
// directly (public, idempotent) to pick up the door(s), mirroring
// EnsureBankingZonesWired()'s own "safe to call more than once" contract.
//
// Flank components are asserted via AActor::GetComponents<UBoxComponent>() rather than
// a private accessor - ARoomActor never attaches a UBoxComponent for any reason other
// than SealRoomPerimeter()'s own wall-gap flanks, so this is an exact proxy for
// WallGapFlankComponents without needing a friend-class grant.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "RoomActor.h"
#include "DoorConnectorActor.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolRoomActorPerimeterSealingTest,
	"KrowdKontrol.Unit.RoomActorPerimeterSealing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace
{
	void CheckWallSolid(FAutomationTestBase& Test, const TCHAR* WallName, UStaticMeshComponent* Wall)
	{
		Test.TestEqual(FString::Printf(TEXT("%s should be blocking (QueryOnly) with no connecting door"), WallName),
			Wall->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);
		Test.TestEqual(FString::Printf(TEXT("%s should Block ECC_WorldDynamic - the channel the real player pawn presents"), WallName),
			Wall->GetCollisionResponseToChannel(ECC_WorldDynamic), ECR_Block);
	}

	// Asserts a gapped side's two flanks leave a walkable span matching
	// +/-ConnectorFloorWidth/2, centered on the room - shared by the East-door and
	// North-door cases below, which differ only in which axis (Y vs X) the gap runs
	// along.
	void CheckFlankGapSpan(FAutomationTestBase& Test, const TCHAR* SideName, float ExtentA, float RelativeA,
		float ExtentB, float RelativeB, float ConnectorFloorWidth)
	{
		const bool bAIsNegativeSide = RelativeA < RelativeB;
		const float GapMin = bAIsNegativeSide ? (RelativeA + ExtentA) : (RelativeB + ExtentB);
		const float GapMax = bAIsNegativeSide ? (RelativeB - ExtentB) : (RelativeA - ExtentA);
		Test.TestTrue(FString::Printf(TEXT("%s flank gap's negative edge should match -ConnectorFloorWidth/2"), SideName),
			FMath::IsNearlyEqual(GapMin, -ConnectorFloorWidth * 0.5f, 0.1f));
		Test.TestTrue(FString::Printf(TEXT("%s flank gap's positive edge should match +ConnectorFloorWidth/2"), SideName),
			FMath::IsNearlyEqual(GapMax, ConnectorFloorWidth * 0.5f, 0.1f));
	}
}

bool FKrowdKontrolRoomActorPerimeterSealingTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}
	World->InitializeActorsForPlay(FURL());
	World->SetBegunPlay(true);

	// (1): a room with no connecting door - all 4 walls end up solid, zero flanks.
	ARoomActor* IsolatedRoom = World->SpawnActor<ARoomActor>(ARoomActor::StaticClass(), FTransform(FVector(0.f, 0.f, 0.f)));
	if (!TestNotNull(TEXT("Isolated ARoomActor should spawn into the test World"), IsolatedRoom))
	{
		return false;
	}

	TArray<UBoxComponent*> IsolatedFlanks;
	IsolatedRoom->GetComponents<UBoxComponent>(IsolatedFlanks);
	TestEqual(TEXT("A room with no connecting door should have zero flank components"), IsolatedFlanks.Num(), 0);
	CheckWallSolid(*this, TEXT("North wall"), IsolatedRoom->WallNorthMeshComponent);
	CheckWallSolid(*this, TEXT("South wall"), IsolatedRoom->WallSouthMeshComponent);
	CheckWallSolid(*this, TEXT("East wall"), IsolatedRoom->WallEastMeshComponent);
	CheckWallSolid(*this, TEXT("West wall"), IsolatedRoom->WallWestMeshComponent);

	// (2): a room with one door on its East side (OtherRoom at +X).
	ARoomActor* EastDoorRoom = World->SpawnActor<ARoomActor>(ARoomActor::StaticClass(), FTransform(FVector(10000.f, 0.f, 0.f)));
	if (!TestNotNull(TEXT("East-door ARoomActor should spawn into the test World"), EastDoorRoom))
	{
		return false;
	}
	// Spawned with no door yet in the World - self-seals fully solid, same as (1).
	TArray<UBoxComponent*> PreDoorFlanks;
	EastDoorRoom->GetComponents<UBoxComponent>(PreDoorFlanks);
	TestEqual(TEXT("Before its door exists, the East-door room should also have zero flanks"), PreDoorFlanks.Num(), 0);

	ARoomActor* EastNeighborRoom = World->SpawnActor<ARoomActor>(ARoomActor::StaticClass(), FTransform(FVector(13000.f, 0.f, 0.f)));
	if (!TestNotNull(TEXT("East neighbor ARoomActor should spawn into the test World"), EastNeighborRoom))
	{
		return false;
	}

	ADoorConnectorActor* EastDoor = World->SpawnActorDeferred<ADoorConnectorActor>(ADoorConnectorActor::StaticClass(), FTransform::Identity);
	if (!TestNotNull(TEXT("East ADoorConnectorActor should spawn into the test World"), EastDoor))
	{
		return false;
	}
	EastDoor->RoomA = EastDoorRoom;
	EastDoor->RoomB = EastNeighborRoom;
	EastDoor->FinishSpawning(FTransform::Identity);

	// The door now exists - re-seal (public, idempotent) so EastDoorRoom picks it up.
	EastDoorRoom->SealRoomPerimeter();

	TestEqual(TEXT("East wall mesh should stay NoCollision (visual-only) once its side is gapped"),
		EastDoorRoom->WallEastMeshComponent->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	CheckWallSolid(*this, TEXT("North wall (East-door room)"), EastDoorRoom->WallNorthMeshComponent);
	CheckWallSolid(*this, TEXT("South wall (East-door room)"), EastDoorRoom->WallSouthMeshComponent);
	CheckWallSolid(*this, TEXT("West wall (East-door room)"), EastDoorRoom->WallWestMeshComponent);

	TArray<UBoxComponent*> EastFlanks;
	EastDoorRoom->GetComponents<UBoxComponent>(EastFlanks);
	if (TestEqual(TEXT("A room with one door should have exactly two flank components"), EastFlanks.Num(), 2))
	{
		for (UBoxComponent* Flank : EastFlanks)
		{
			TestEqual(TEXT("Each flank should be blocking (QueryOnly)"), Flank->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);
			TestEqual(TEXT("Each flank should Block ECC_WorldDynamic"),
				Flank->GetCollisionResponseToChannel(ECC_WorldDynamic), ECR_Block);
		}

		// The two flanks' combined Y-span should leave a walkable gap matching the
		// door's ConnectorFloorWidth, centered at Y=0 (relative to EastDoorRoom).
		CheckFlankGapSpan(*this, TEXT("East-door"),
			EastFlanks[0]->GetUnscaledBoxExtent().Y, EastFlanks[0]->GetRelativeLocation().Y,
			EastFlanks[1]->GetUnscaledBoxExtent().Y, EastFlanks[1]->GetRelativeLocation().Y,
			EastDoor->ConnectorFloorWidth);
	}

	// (3): a mid-chain room with doors on both East and West.
	ARoomActor* MidRoom = World->SpawnActor<ARoomActor>(ARoomActor::StaticClass(), FTransform(FVector(20000.f, 0.f, 0.f)));
	ARoomActor* MidWestNeighbor = World->SpawnActor<ARoomActor>(ARoomActor::StaticClass(), FTransform(FVector(17000.f, 0.f, 0.f)));
	ARoomActor* MidEastNeighbor = World->SpawnActor<ARoomActor>(ARoomActor::StaticClass(), FTransform(FVector(23000.f, 0.f, 0.f)));
	if (!TestNotNull(TEXT("Mid-chain ARoomActor should spawn into the test World"), MidRoom) ||
		!TestNotNull(TEXT("Mid-chain West neighbor ARoomActor should spawn into the test World"), MidWestNeighbor) ||
		!TestNotNull(TEXT("Mid-chain East neighbor ARoomActor should spawn into the test World"), MidEastNeighbor))
	{
		return false;
	}

	ADoorConnectorActor* MidWestDoor = World->SpawnActorDeferred<ADoorConnectorActor>(ADoorConnectorActor::StaticClass(), FTransform::Identity);
	ADoorConnectorActor* MidEastDoor = World->SpawnActorDeferred<ADoorConnectorActor>(ADoorConnectorActor::StaticClass(), FTransform::Identity);
	if (!TestNotNull(TEXT("Mid-chain West ADoorConnectorActor should spawn into the test World"), MidWestDoor) ||
		!TestNotNull(TEXT("Mid-chain East ADoorConnectorActor should spawn into the test World"), MidEastDoor))
	{
		return false;
	}
	MidWestDoor->RoomA = MidRoom;
	MidWestDoor->RoomB = MidWestNeighbor;
	MidWestDoor->FinishSpawning(FTransform::Identity);
	MidEastDoor->RoomA = MidRoom;
	MidEastDoor->RoomB = MidEastNeighbor;
	MidEastDoor->FinishSpawning(FTransform::Identity);

	MidRoom->SealRoomPerimeter();

	TestEqual(TEXT("Mid-chain room's East wall mesh should stay NoCollision"),
		MidRoom->WallEastMeshComponent->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	TestEqual(TEXT("Mid-chain room's West wall mesh should stay NoCollision"),
		MidRoom->WallWestMeshComponent->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	CheckWallSolid(*this, TEXT("North wall (mid-chain room)"), MidRoom->WallNorthMeshComponent);
	CheckWallSolid(*this, TEXT("South wall (mid-chain room)"), MidRoom->WallSouthMeshComponent);

	TArray<UBoxComponent*> MidFlanks;
	MidRoom->GetComponents<UBoxComponent>(MidFlanks);
	TestEqual(TEXT("A mid-chain room with two doors should have four flank components (two per gapped side)"),
		MidFlanks.Num(), 4);

	// (4): calling SealRoomPerimeter() twice must not leak flank components - count
	// stays the same, and the previous flanks are actually destroyed (no longer
	// attached/discoverable), not merely duplicated alongside new ones.
	TArray<UBoxComponent*> FlanksBeforeSecondCall;
	EastDoorRoom->GetComponents<UBoxComponent>(FlanksBeforeSecondCall);

	EastDoorRoom->SealRoomPerimeter();

	TArray<UBoxComponent*> FlanksAfterSecondCall;
	EastDoorRoom->GetComponents<UBoxComponent>(FlanksAfterSecondCall);
	TestEqual(TEXT("Calling SealRoomPerimeter() twice should not change the flank component count"),
		FlanksAfterSecondCall.Num(), FlanksBeforeSecondCall.Num());
	for (UBoxComponent* OldFlank : FlanksBeforeSecondCall)
	{
		TestFalse(TEXT("A flank from the first SealRoomPerimeter() call should not still be attached after a second call"),
			FlanksAfterSecondCall.Contains(OldFlank));
	}

	// (5): a door wide enough to reach/exceed the wall's tangent extent - the flank
	// should gracefully skip (BuildWallSideFlanks's KINDA_SMALL_NUMBER guard) rather than
	// produce a negative/zero-extent box, matching the investigation doc's own named
	// accepted risk ("that side ends up fully open rather than crashing"). For an
	// East/West door, BuildWallSideFlanks's WallSpanHalfExtent local is Room->RoomFloorExtent.Y
	// - not .X - so it's Y that must be narrowed to push the default
	// ConnectorFloorWidth (300, GapHalfWidth=150) past the wall's own half-extent.
	ARoomActor* NarrowRoom = World->SpawnActor<ARoomActor>(ARoomActor::StaticClass(), FTransform(FVector(30000.f, 0.f, 0.f)));
	if (!TestNotNull(TEXT("Narrow ARoomActor should spawn into the test World"), NarrowRoom))
	{
		return false;
	}
	NarrowRoom->RoomFloorExtent = FVector2D(1000.f, 100.f);

	ARoomActor* NarrowNeighbor = World->SpawnActor<ARoomActor>(ARoomActor::StaticClass(), FTransform(FVector(33000.f, 0.f, 0.f)));
	if (!TestNotNull(TEXT("Narrow neighbor ARoomActor should spawn into the test World"), NarrowNeighbor))
	{
		return false;
	}

	ADoorConnectorActor* WideDoor = World->SpawnActorDeferred<ADoorConnectorActor>(ADoorConnectorActor::StaticClass(), FTransform::Identity);
	if (!TestNotNull(TEXT("Wide ADoorConnectorActor should spawn into the test World"), WideDoor))
	{
		return false;
	}
	WideDoor->RoomA = NarrowRoom;
	WideDoor->RoomB = NarrowNeighbor;
	WideDoor->FinishSpawning(FTransform::Identity); // ConnectorFloorWidth=300 -> GapHalfWidth=150 > NarrowRoom's RoomFloorExtent.Y=100

	NarrowRoom->SealRoomPerimeter();

	TArray<UBoxComponent*> NarrowFlanks;
	NarrowRoom->GetComponents<UBoxComponent>(NarrowFlanks);
	TestEqual(TEXT("A gap wider than the wall's tangent extent should skip both flanks, not crash or produce a degenerate box"),
		NarrowFlanks.Num(), 0);
	TestEqual(TEXT("East wall mesh should still go NoCollision (visual-only) once its side is gapped, even when both flanks are skipped"),
		NarrowRoom->WallEastMeshComponent->GetCollisionEnabled(), ECollisionEnabled::NoCollision);

	// (6): a room with a door on its North side (neighbor at +Y) - exercises the
	// North/South branch of Side selection and BuildWallSideFlanks's axis-swapped
	// geometry, never reached by any of the East/West cases above.
	ARoomActor* NorthDoorRoom = World->SpawnActor<ARoomActor>(ARoomActor::StaticClass(), FTransform(FVector(40000.f, 0.f, 0.f)));
	if (!TestNotNull(TEXT("North-door ARoomActor should spawn into the test World"), NorthDoorRoom))
	{
		return false;
	}
	ARoomActor* NorthNeighbor = World->SpawnActor<ARoomActor>(ARoomActor::StaticClass(), FTransform(FVector(40000.f, 3000.f, 0.f)));
	if (!TestNotNull(TEXT("North neighbor ARoomActor should spawn into the test World"), NorthNeighbor))
	{
		return false;
	}

	ADoorConnectorActor* NorthDoor = World->SpawnActorDeferred<ADoorConnectorActor>(ADoorConnectorActor::StaticClass(), FTransform::Identity);
	if (!TestNotNull(TEXT("North ADoorConnectorActor should spawn into the test World"), NorthDoor))
	{
		return false;
	}
	NorthDoor->RoomA = NorthDoorRoom;
	NorthDoor->RoomB = NorthNeighbor;
	NorthDoor->FinishSpawning(FTransform::Identity);

	NorthDoorRoom->SealRoomPerimeter();

	TestEqual(TEXT("North wall mesh should stay NoCollision (visual-only) once its side is gapped"),
		NorthDoorRoom->WallNorthMeshComponent->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	CheckWallSolid(*this, TEXT("South wall (North-door room)"), NorthDoorRoom->WallSouthMeshComponent);
	CheckWallSolid(*this, TEXT("East wall (North-door room)"), NorthDoorRoom->WallEastMeshComponent);
	CheckWallSolid(*this, TEXT("West wall (North-door room)"), NorthDoorRoom->WallWestMeshComponent);

	TArray<UBoxComponent*> NorthFlanks;
	NorthDoorRoom->GetComponents<UBoxComponent>(NorthFlanks);
	if (TestEqual(TEXT("A room with one North door should have exactly two flank components"), NorthFlanks.Num(), 2))
	{
		for (UBoxComponent* Flank : NorthFlanks)
		{
			TestEqual(TEXT("Each North-side flank should be blocking (QueryOnly)"), Flank->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);
			TestEqual(TEXT("Each North-side flank should Block ECC_WorldDynamic"),
				Flank->GetCollisionResponseToChannel(ECC_WorldDynamic), ECR_Block);
		}

		// The two flanks' combined X-span should leave a walkable gap matching the
		// door's ConnectorFloorWidth, centered at X=0 (relative to NorthDoorRoom) -
		// the axis-swapped mirror of the East-door case's Y-span assertion above.
		CheckFlankGapSpan(*this, TEXT("North-door"),
			NorthFlanks[0]->GetUnscaledBoxExtent().X, NorthFlanks[0]->GetRelativeLocation().X,
			NorthFlanks[1]->GetUnscaledBoxExtent().X, NorthFlanks[1]->GetRelativeLocation().X,
			NorthDoor->ConnectorFloorWidth);
	}

	// (7): a room with two doors on the same (East) side - exercises
	// BuildWallSideFlanks() being called once per door on a single side, rather than
	// once per side, so both doors' flank pairs must accumulate (not overwrite one
	// another) in WallGapFlankComponents.
	ARoomActor* MultiDoorRoom = World->SpawnActor<ARoomActor>(ARoomActor::StaticClass(), FTransform(FVector(50000.f, 0.f, 0.f)));
	if (!TestNotNull(TEXT("Multi-door ARoomActor should spawn into the test World"), MultiDoorRoom))
	{
		return false;
	}
	ARoomActor* MultiDoorNeighborA = World->SpawnActor<ARoomActor>(ARoomActor::StaticClass(), FTransform(FVector(53000.f, 800.f, 0.f)));
	ARoomActor* MultiDoorNeighborB = World->SpawnActor<ARoomActor>(ARoomActor::StaticClass(), FTransform(FVector(53000.f, -800.f, 0.f)));
	if (!TestNotNull(TEXT("Multi-door East neighbor A ARoomActor should spawn into the test World"), MultiDoorNeighborA) ||
		!TestNotNull(TEXT("Multi-door East neighbor B ARoomActor should spawn into the test World"), MultiDoorNeighborB))
	{
		return false;
	}

	ADoorConnectorActor* MultiDoorA = World->SpawnActorDeferred<ADoorConnectorActor>(ADoorConnectorActor::StaticClass(), FTransform::Identity);
	ADoorConnectorActor* MultiDoorB = World->SpawnActorDeferred<ADoorConnectorActor>(ADoorConnectorActor::StaticClass(), FTransform::Identity);
	if (!TestNotNull(TEXT("Multi-door East door A should spawn into the test World"), MultiDoorA) ||
		!TestNotNull(TEXT("Multi-door East door B should spawn into the test World"), MultiDoorB))
	{
		return false;
	}
	MultiDoorA->RoomA = MultiDoorRoom;
	MultiDoorA->RoomB = MultiDoorNeighborA;
	MultiDoorA->FinishSpawning(FTransform::Identity);
	MultiDoorB->RoomA = MultiDoorRoom;
	MultiDoorB->RoomB = MultiDoorNeighborB;
	MultiDoorB->FinishSpawning(FTransform::Identity);

	MultiDoorRoom->SealRoomPerimeter();

	TestEqual(TEXT("Multi-door room's East wall mesh should stay NoCollision with two East doors"),
		MultiDoorRoom->WallEastMeshComponent->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	CheckWallSolid(*this, TEXT("North wall (multi-door room)"), MultiDoorRoom->WallNorthMeshComponent);
	CheckWallSolid(*this, TEXT("South wall (multi-door room)"), MultiDoorRoom->WallSouthMeshComponent);
	CheckWallSolid(*this, TEXT("West wall (multi-door room)"), MultiDoorRoom->WallWestMeshComponent);

	TArray<UBoxComponent*> MultiDoorFlanks;
	MultiDoorRoom->GetComponents<UBoxComponent>(MultiDoorFlanks);
	if (TestEqual(TEXT("Two non-overlapping doors on the same side should produce three flank components (N+1 solid segments - issue #243, PR #305 pass-1 rejection fix, replacing the old per-door 2*N-flank algorithm)"),
		MultiDoorFlanks.Num(), 3))
	{
		for (UBoxComponent* Flank : MultiDoorFlanks)
		{
			TestEqual(TEXT("Each multi-door flank should be blocking (QueryOnly)"), Flank->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);
			TestEqual(TEXT("Each multi-door flank should Block ECC_WorldDynamic"),
				Flank->GetCollisionResponseToChannel(ECC_WorldDynamic), ECR_Block);
		}
	}

	// Regression (PR #305 pass-1 rejection): each door's own gap must stay walkable -
	// the bug this test exists to catch made Door A's flank fully cover Door B's gap and
	// vice versa, so a naive flank-count/channel check (above) passed at 4/4 anyway.
	// Gap centers use the corrected crossing-point formula (issue #243 Finding 2a):
	// CrossingParam = RoomFloorExtent.X / Delta.X = 1000/3000 = 1/3, GapCenterOffset =
	// CrossingParam * Delta.Y = (1/3) * (+-800) = +-266.667 - not the old room-centres
	// midpoint formula's +-400.
	auto IsPointBlockedByAnyFlank = [&MultiDoorFlanks](const FVector& LocalPoint) -> bool
	{
		for (UBoxComponent* Flank : MultiDoorFlanks)
		{
			const FVector LocalToFlank = Flank->GetRelativeLocation();
			const FVector Extent = Flank->GetUnscaledBoxExtent();
			if (FMath::Abs(LocalPoint.Y - LocalToFlank.Y) < Extent.Y &&
				FMath::Abs(LocalPoint.X - LocalToFlank.X) < Extent.X)
			{
				return true;
			}
		}
		return false;
	};
	TestFalse(TEXT("Door A's own gap center should not be blocked by any flank (PR #305 pass-1 regression)"),
		IsPointBlockedByAnyFlank(FVector(MultiDoorRoom->RoomFloorExtent.X, 800.f / 3.f, 0.f)));
	TestFalse(TEXT("Door B's own gap center should not be blocked by any flank (PR #305 pass-1 regression)"),
		IsPointBlockedByAnyFlank(FVector(MultiDoorRoom->RoomFloorExtent.X, -800.f / 3.f, 0.f)));

	// (7b): two doors on the same (East) side whose gaps overlap - exercises
	// BuildWallSideFlanks's merge branch (RoomActor.cpp's MergedGaps loop), never
	// reached by case (7)'s non-overlapping doors (issue #243 test-coverage Finding 2).
	// CrossingParam = RoomFloorExtent.X / Delta.X = 1000/3000 = 1/3, so neighbors at
	// Y=+-100 give gap centers +-33.333 (GapCenterOffset = 1/3 * +-100); with
	// GapHalfWidth=150 the two gaps are [-116.667,183.333] and [-183.333,116.667] -
	// overlapping by 233.333uu - which must merge into a single [-183.333,183.333] open
	// span, leaving exactly 2 flanks (not 3) and both original gap centers walkable.
	ARoomActor* OverlapDoorRoom = World->SpawnActor<ARoomActor>(ARoomActor::StaticClass(), FTransform(FVector(70000.f, 0.f, 0.f)));
	if (!TestNotNull(TEXT("Overlap ARoomActor should spawn into the test World"), OverlapDoorRoom))
	{
		return false;
	}
	ARoomActor* OverlapNeighborA = World->SpawnActor<ARoomActor>(ARoomActor::StaticClass(), FTransform(FVector(73000.f, 100.f, 0.f)));
	ARoomActor* OverlapNeighborB = World->SpawnActor<ARoomActor>(ARoomActor::StaticClass(), FTransform(FVector(73000.f, -100.f, 0.f)));
	if (!TestNotNull(TEXT("Overlap East neighbor A ARoomActor should spawn into the test World"), OverlapNeighborA) ||
		!TestNotNull(TEXT("Overlap East neighbor B ARoomActor should spawn into the test World"), OverlapNeighborB))
	{
		return false;
	}

	ADoorConnectorActor* OverlapDoorA = World->SpawnActorDeferred<ADoorConnectorActor>(ADoorConnectorActor::StaticClass(), FTransform::Identity);
	ADoorConnectorActor* OverlapDoorB = World->SpawnActorDeferred<ADoorConnectorActor>(ADoorConnectorActor::StaticClass(), FTransform::Identity);
	if (!TestNotNull(TEXT("Overlap East door A should spawn into the test World"), OverlapDoorA) ||
		!TestNotNull(TEXT("Overlap East door B should spawn into the test World"), OverlapDoorB))
	{
		return false;
	}
	OverlapDoorA->RoomA = OverlapDoorRoom;
	OverlapDoorA->RoomB = OverlapNeighborA;
	OverlapDoorA->FinishSpawning(FTransform::Identity);
	OverlapDoorB->RoomA = OverlapDoorRoom;
	OverlapDoorB->RoomB = OverlapNeighborB;
	OverlapDoorB->FinishSpawning(FTransform::Identity);

	OverlapDoorRoom->SealRoomPerimeter();

	TArray<UBoxComponent*> OverlapFlanks;
	OverlapDoorRoom->GetComponents<UBoxComponent>(OverlapFlanks);
	if (TestEqual(TEXT("Two overlapping same-side gaps should merge into one open span, producing only 2 flanks (not 3) - issue #243 BuildWallSideFlanks merge branch"),
		OverlapFlanks.Num(), 2))
	{
		auto IsOverlapPointBlockedByAnyFlank = [&OverlapFlanks](const FVector& LocalPoint) -> bool
		{
			for (UBoxComponent* Flank : OverlapFlanks)
			{
				const FVector LocalToFlank = Flank->GetRelativeLocation();
				const FVector Extent = Flank->GetUnscaledBoxExtent();
				if (FMath::Abs(LocalPoint.Y - LocalToFlank.Y) < Extent.Y &&
					FMath::Abs(LocalPoint.X - LocalToFlank.X) < Extent.X)
				{
					return true;
				}
			}
			return false;
		};
		TestFalse(TEXT("Door A's own gap center should stay walkable after the overlap merge"),
			IsOverlapPointBlockedByAnyFlank(FVector(OverlapDoorRoom->RoomFloorExtent.X, 100.f / 3.f, 0.f)));
		TestFalse(TEXT("Door B's own gap center should stay walkable after the overlap merge"),
			IsOverlapPointBlockedByAnyFlank(FVector(OverlapDoorRoom->RoomFloorExtent.X, -100.f / 3.f, 0.f)));
	}

	// (8): a diagonal/asymmetric-extent room pair - proves Finding 2a's crossing-point
	// fix actually changes the computed gap center for a non-collinear pair. Every case
	// above pairs a collinear neighbor with a room whose RoomFloorExtent on the
	// wall-normal axis equals half of Delta on that axis - exactly the condition under
	// which the old room-centres-midpoint formula and the corrected crossing-point
	// formula are numerically identical, so none of them can distinguish the two. This
	// case uses a non-square RoomFloorExtent and an off-axis neighbor specifically so
	// they diverge.
	ARoomActor* DiagonalRoom = World->SpawnActor<ARoomActor>(ARoomActor::StaticClass(), FTransform(FVector(60000.f, 0.f, 0.f)));
	if (!TestNotNull(TEXT("Diagonal ARoomActor should spawn into the test World"), DiagonalRoom))
	{
		return false;
	}
	DiagonalRoom->RoomFloorExtent = FVector2D(1200.f, 600.f);

	ARoomActor* DiagonalNeighbor = World->SpawnActor<ARoomActor>(ARoomActor::StaticClass(), FTransform(FVector(63000.f, 900.f, 0.f)));
	if (!TestNotNull(TEXT("Diagonal neighbor ARoomActor should spawn into the test World"), DiagonalNeighbor))
	{
		return false;
	}

	ADoorConnectorActor* DiagonalDoor = World->SpawnActorDeferred<ADoorConnectorActor>(ADoorConnectorActor::StaticClass(), FTransform::Identity);
	if (!TestNotNull(TEXT("Diagonal ADoorConnectorActor should spawn into the test World"), DiagonalDoor))
	{
		return false;
	}
	DiagonalDoor->RoomA = DiagonalRoom;
	DiagonalDoor->RoomB = DiagonalNeighbor;
	DiagonalDoor->FinishSpawning(FTransform::Identity);

	DiagonalRoom->SealRoomPerimeter();

	TArray<UBoxComponent*> DiagonalFlanks;
	DiagonalRoom->GetComponents<UBoxComponent>(DiagonalFlanks);
	if (TestEqual(TEXT("Diagonal room's East door should still produce exactly two flank components"), DiagonalFlanks.Num(), 2))
	{
		// Whichever flank sits on the negative-Y side (its own +Y edge borders the
		// gap's start) vs the positive-Y side (its own -Y edge borders the gap's end).
		const bool bFlank0IsNegativeSide = DiagonalFlanks[0]->GetRelativeLocation().Y < DiagonalFlanks[1]->GetRelativeLocation().Y;
		UBoxComponent* NegFlank = bFlank0IsNegativeSide ? DiagonalFlanks[0] : DiagonalFlanks[1];
		UBoxComponent* PosFlank = bFlank0IsNegativeSide ? DiagonalFlanks[1] : DiagonalFlanks[0];
		const float GapStart = NegFlank->GetRelativeLocation().Y + NegFlank->GetUnscaledBoxExtent().Y;
		const float GapEnd = PosFlank->GetRelativeLocation().Y - PosFlank->GetUnscaledBoxExtent().Y;
		const float GapCenter = (GapStart + GapEnd) * 0.5f;

		// Corrected crossing-point formula (Finding 2a): CrossingParam =
		// RoomFloorExtent.X / Delta.X = 1200/3000 = 0.4, GapCenterOffset =
		// CrossingParam * Delta.Y = 0.4 * 900 = 360 - not the old room-centres-midpoint
		// formula's Delta.Y * 0.5 = 450.
		TestTrue(TEXT("Diagonal room's gap center should match the corrected crossing-point formula (issue #243 Finding 2a), not the old room-centres midpoint"),
			FMath::IsNearlyEqual(GapCenter, 360.f, 0.5f));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
