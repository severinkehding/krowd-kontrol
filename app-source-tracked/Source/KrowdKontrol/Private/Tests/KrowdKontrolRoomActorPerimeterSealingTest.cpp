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
		const float ExtentA = EastFlanks[0]->GetUnscaledBoxExtent().Y;
		const float ExtentB = EastFlanks[1]->GetUnscaledBoxExtent().Y;
		const float RelativeYA = EastFlanks[0]->GetRelativeLocation().Y;
		const float RelativeYB = EastFlanks[1]->GetRelativeLocation().Y;
		const bool bAIsNegativeSide = RelativeYA < RelativeYB;
		const float GapMin = bAIsNegativeSide ? (RelativeYA + ExtentA) : (RelativeYB + ExtentB);
		const float GapMax = bAIsNegativeSide ? (RelativeYB - ExtentB) : (RelativeYA - ExtentA);
		TestTrue(TEXT("Flank gap's negative edge should match -ConnectorFloorWidth/2"),
			FMath::IsNearlyEqual(GapMin, -EastDoor->ConnectorFloorWidth * 0.5f, 0.1f));
		TestTrue(TEXT("Flank gap's positive edge should match +ConnectorFloorWidth/2"),
			FMath::IsNearlyEqual(GapMax, EastDoor->ConnectorFloorWidth * 0.5f, 0.1f));
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

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
