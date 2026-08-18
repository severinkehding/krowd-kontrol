// Confirms URoomPoolShufflerComponent (issue #51, PRD 05 REQ-4/REQ-6) filters a room
// pool down to those tagged with a matching URoomMetadataComponent::DifficultyTier
// (excluding both wrong-tier and untagged rooms), sequences the result into a
// connected chain of spawned ADoorConnectorActor instances matching the returned
// order, reproduces the same order for the same seed, and produces a different order
// (same room set) for a different seed - the issue's core acceptance criterion and
// PRD 05's own success metric.
//
// Needs a real UWorld to spawn into (ShuffleRooms() calls GetWorld()->SpawnActor for
// the door chain), so uses the same FAutomationEditorCommonUtils::CreateNewMap()
// scaffold as the other room/door tests in this suite.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "RoomPoolShufflerComponent.h"
#include "RoomActor.h"
#include "RoomMetadataComponent.h"
#include "DoorConnectorActor.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolRoomPoolShufflerComponentTest,
	"KrowdKontrol.Unit.RoomPoolShufflerComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolRoomPoolShufflerComponentTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	// 6 Easy-tagged rooms + 2 Hard-tagged rooms + 1 untagged room (no metadata
	// component at all) - lets the test assert the tier filter excludes both the
	// wrong-tier and the untagged case, not just that shuffling works.
	TArray<ARoomActor*> Pool;

	for (int32 Index = 0; Index < 6; ++Index)
	{
		ARoomActor* Room = World->SpawnActor<ARoomActor>();
		if (!TestNotNull(TEXT("Easy-tier room should spawn into the test World"), Room))
		{
			return false;
		}
		URoomMetadataComponent* Metadata = NewObject<URoomMetadataComponent>(Room);
		Metadata->RegisterComponent();
		Metadata->DifficultyTier = ERoomDifficultyTier::Easy;
		Pool.Add(Room);
	}

	for (int32 Index = 0; Index < 2; ++Index)
	{
		ARoomActor* Room = World->SpawnActor<ARoomActor>();
		if (!TestNotNull(TEXT("Hard-tier room should spawn into the test World"), Room))
		{
			return false;
		}
		URoomMetadataComponent* Metadata = NewObject<URoomMetadataComponent>(Room);
		Metadata->RegisterComponent();
		Metadata->DifficultyTier = ERoomDifficultyTier::Hard;
		Pool.Add(Room);
	}

	ARoomActor* UntaggedRoom = World->SpawnActor<ARoomActor>();
	if (!TestNotNull(TEXT("Untagged room should spawn into the test World"), UntaggedRoom))
	{
		return false;
	}
	Pool.Add(UntaggedRoom);

	AActor* OwnerActor = World->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("Owner actor should spawn into the test World"), OwnerActor))
	{
		return false;
	}

	URoomPoolShufflerComponent* Shuffler = NewObject<URoomPoolShufflerComponent>(OwnerActor);
	if (!TestNotNull(TEXT("URoomPoolShufflerComponent should construct"), Shuffler))
	{
		return false;
	}
	Shuffler->RegisterComponent();

	// (a) Filtering: only the 6 Easy-tagged rooms should come back.
	TArray<ARoomActor*> SequenceA = Shuffler->ShuffleRooms(Pool, ERoomDifficultyTier::Easy, /*Seed=*/1);
	TestEqual(TEXT("Shuffled sequence should contain exactly the 6 Easy-tagged rooms"), SequenceA.Num(), 6);
	for (ARoomActor* Room : SequenceA)
	{
		URoomMetadataComponent* Metadata = Room ? Room->FindComponentByClass<URoomMetadataComponent>() : nullptr;
		if (TestNotNull(TEXT("Every returned room should have metadata"), Metadata))
		{
			TestEqual(TEXT("Every returned room's DifficultyTier should be Easy"),
				static_cast<uint8>(Metadata->DifficultyTier), static_cast<uint8>(ERoomDifficultyTier::Easy));
		}
	}

	// (b) Connected layout: 5 doors chaining the 6-room sequence in returned order.
	const TArray<TObjectPtr<ADoorConnectorActor>>& Doors = Shuffler->GetSpawnedDoors();
	TestEqual(TEXT("Shuffler should spawn one fewer door than rooms in the sequence"), Doors.Num(), 5);
	for (int32 Index = 0; Index < Doors.Num(); ++Index)
	{
		ADoorConnectorActor* Door = Doors[Index];
		if (!TestNotNull(TEXT("Spawned door should be valid"), Door))
		{
			continue;
		}
		TestTrue(TEXT("Spawned door should connect two distinct valid rooms"), Door->ConnectsValidRooms());
		TestEqual(TEXT("Door RoomA should match the sequence at this index"), static_cast<ARoomActor*>(Door->RoomA), SequenceA[Index]);
		TestEqual(TEXT("Door RoomB should match the sequence at the next index"), static_cast<ARoomActor*>(Door->RoomB), SequenceA[Index + 1]);
	}

	// (c) Reproducibility: same seed reproduces the same order.
	TArray<ARoomActor*> SequenceRepeat = Shuffler->ShuffleRooms(Pool, ERoomDifficultyTier::Easy, /*Seed=*/1);
	TestEqual(TEXT("Repeat shuffle with the same seed should return the same number of rooms"), SequenceRepeat.Num(), SequenceA.Num());
	for (int32 Index = 0; Index < SequenceA.Num(); ++Index)
	{
		TestEqual(TEXT("Repeat shuffle with the same seed should reproduce the same order"), SequenceRepeat[Index], SequenceA[Index]);
	}

	// (d) Different seeds differ (the issue's core acceptance criterion), while still
	// containing the exact same set of rooms.
	TArray<ARoomActor*> SequenceB = Shuffler->ShuffleRooms(Pool, ERoomDifficultyTier::Easy, /*Seed=*/987654321);
	TestEqual(TEXT("Different-seed shuffle should still return all 6 Easy-tagged rooms"), SequenceB.Num(), SequenceA.Num());

	bool bAnyIndexDiffers = false;
	for (int32 Index = 0; Index < SequenceA.Num() && Index < SequenceB.Num(); ++Index)
	{
		if (SequenceA[Index] != SequenceB[Index])
		{
			bAnyIndexDiffers = true;
			break;
		}
	}
	TestTrue(TEXT("Different seeds should produce a different room ordering"), bAnyIndexDiffers);

	TSet<ARoomActor*> SetA(SequenceA);
	TSet<ARoomActor*> SetB(SequenceB);
	TestEqual(TEXT("Different-seed shuffles should contain the same set of rooms"), SetA.Num(), SetB.Num());
	for (ARoomActor* Room : SetA)
	{
		TestTrue(TEXT("Every room in the first sequence should also appear in the second"), SetB.Contains(Room));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
