// Confirms URoomPoolShufflerComponent (issue #51, PRD 05 REQ-4/REQ-6) filters a room
// pool down to those tagged with a matching URoomMetadataComponent::DifficultyTier
// (excluding both wrong-tier and untagged rooms), sequences the result into a
// connected chain of spawned ADoorConnectorActor instances matching the returned
// order, reproduces the same order for the same seed, and produces a different order
// (same room set) for a different seed - the issue's core acceptance criterion and
// PRD 05's own success metric. Also confirms ability-gating (issue #53, PRD 05 REQ-5):
// a room whose RequiredAbility is not yet unlocked is excluded from shuffle output,
// and becomes eligible once it is.
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
#include "AbilityUnlockComponent.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "EngineUtils.h"

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

	// Editor-authored TArray properties commonly have unset (null) slots; the filter
	// loop guards against this explicitly, so exercise it here.
	Pool.Add(nullptr);

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

	UAbilityUnlockComponent* DefaultUnlockState = NewObject<UAbilityUnlockComponent>();
	if (!TestNotNull(TEXT("UAbilityUnlockComponent should construct"), DefaultUnlockState))
	{
		return false;
	}

	// (a) Filtering: only the 6 Easy-tagged rooms should come back.
	TArray<ARoomActor*> SequenceA = Shuffler->ShuffleRooms(Pool, ERoomDifficultyTier::Easy, /*Seed=*/1, DefaultUnlockState);
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
	TArray<ARoomActor*> SequenceRepeat = Shuffler->ShuffleRooms(Pool, ERoomDifficultyTier::Easy, /*Seed=*/1, DefaultUnlockState);
	TestEqual(TEXT("Repeat shuffle with the same seed should return the same number of rooms"), SequenceRepeat.Num(), SequenceA.Num());
	for (int32 Index = 0; Index < SequenceA.Num(); ++Index)
	{
		TestEqual(TEXT("Repeat shuffle with the same seed should reproduce the same order"), SequenceRepeat[Index], SequenceA[Index]);
	}

	// (d) Different seeds differ (the issue's core acceptance criterion), while still
	// containing the exact same set of rooms.
	TArray<ARoomActor*> SequenceB = Shuffler->ShuffleRooms(Pool, ERoomDifficultyTier::Easy, /*Seed=*/987654321, DefaultUnlockState);
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

	// (e) Repeated calls must not leak the previous call's door actors: after (c)'s
	// repeat ShuffleRooms() call above, the world should contain exactly as many live
	// ADoorConnectorActor instances as the most recent call tracks, not the sum of
	// every call made so far.
	int32 LiveDoorCount = 0;
	for (TActorIterator<ADoorConnectorActor> It(World); It; ++It)
	{
		++LiveDoorCount;
	}
	TestEqual(TEXT("World should not accumulate orphaned doors across repeated ShuffleRooms() calls"),
		LiveDoorCount, Shuffler->GetSpawnedDoors().Num());

	// (f) Zero-match tier: Medium is declared but never tagged on any room in Pool.
	TArray<ARoomActor*> SequenceMedium = Shuffler->ShuffleRooms(Pool, ERoomDifficultyTier::Medium, /*Seed=*/1, DefaultUnlockState);
	TestEqual(TEXT("Zero-match tier should return an empty sequence"), SequenceMedium.Num(), 0);
	TestEqual(TEXT("Zero-match tier should spawn no doors"), Shuffler->GetSpawnedDoors().Num(), 0);

	// (g) Single-match tier: exactly one Hard-tagged room in a fresh small pool.
	ARoomActor* SoloRoom = World->SpawnActor<ARoomActor>();
	if (!TestNotNull(TEXT("Solo room should spawn into the test World"), SoloRoom))
	{
		return false;
	}
	URoomMetadataComponent* SoloMetadata = NewObject<URoomMetadataComponent>(SoloRoom);
	SoloMetadata->RegisterComponent();
	SoloMetadata->DifficultyTier = ERoomDifficultyTier::Hard;
	TArray<ARoomActor*> SoloPool = { SoloRoom };

	TArray<ARoomActor*> SequenceSolo = Shuffler->ShuffleRooms(SoloPool, ERoomDifficultyTier::Hard, /*Seed=*/1, DefaultUnlockState);
	TestEqual(TEXT("Single-match tier should return that one room"), SequenceSolo.Num(), 1);
	TestEqual(TEXT("Single-match tier should spawn no doors"), Shuffler->GetSpawnedDoors().Num(), 0);

	// (h) Ability-gating (issue #53, PRD 05 REQ-5): a Root-gated room must be
	// excluded from shuffle output until the player has unlocked Root, and become
	// eligible once it is.
	ARoomActor* PlainRoom = World->SpawnActor<ARoomActor>();
	if (!TestNotNull(TEXT("Plain Easy room should spawn into the test World"), PlainRoom))
	{
		return false;
	}
	URoomMetadataComponent* PlainMetadata = NewObject<URoomMetadataComponent>(PlainRoom);
	PlainMetadata->RegisterComponent();
	PlainMetadata->DifficultyTier = ERoomDifficultyTier::Easy;

	ARoomActor* RootGatedRoom = World->SpawnActor<ARoomActor>();
	if (!TestNotNull(TEXT("Root-gated Easy room should spawn into the test World"), RootGatedRoom))
	{
		return false;
	}
	URoomMetadataComponent* RootGatedMetadata = NewObject<URoomMetadataComponent>(RootGatedRoom);
	RootGatedMetadata->RegisterComponent();
	RootGatedMetadata->DifficultyTier = ERoomDifficultyTier::Easy;
	RootGatedMetadata->RequiredAbility = ERoomAbilityGate::Root;

	TArray<ARoomActor*> GatingPool = { PlainRoom, RootGatedRoom };

	UAbilityUnlockComponent* GatingUnlockState = NewObject<UAbilityUnlockComponent>();
	if (!TestNotNull(TEXT("Gating-test UAbilityUnlockComponent should construct"), GatingUnlockState))
	{
		return false;
	}
	TestFalse(TEXT("Root should not be unlocked on a fresh UAbilityUnlockComponent"),
		GatingUnlockState->IsAbilityUnlocked(EAbilitySlot::Root));

	// (h-a) Root locked: the Root-gated room must be excluded.
	TArray<ARoomActor*> SequenceRootLocked = Shuffler->ShuffleRooms(GatingPool, ERoomDifficultyTier::Easy, /*Seed=*/1, GatingUnlockState);
	TestEqual(TEXT("Root-gated room should be excluded while Root is locked"), SequenceRootLocked.Num(), 1);
	if (SequenceRootLocked.Num() == 1)
	{
		TestEqual(TEXT("Only the plain room should come back while Root is locked"), SequenceRootLocked[0], PlainRoom);
	}

	// (h-b) Root unlocked (NotifyLevelReached(3) - AbilityUnlockComponent.cpp's
	// LevelToAbilityMap maps level 3 to Root): the Root-gated room becomes eligible.
	GatingUnlockState->NotifyLevelReached(3);
	TestTrue(TEXT("Root should be unlocked after NotifyLevelReached(3)"),
		GatingUnlockState->IsAbilityUnlocked(EAbilitySlot::Root));

	TArray<ARoomActor*> SequenceRootUnlocked = Shuffler->ShuffleRooms(GatingPool, ERoomDifficultyTier::Easy, /*Seed=*/1, GatingUnlockState);
	TestEqual(TEXT("Both rooms should come back once Root is unlocked"), SequenceRootUnlocked.Num(), 2);
	TSet<ARoomActor*> SetRootUnlocked(SequenceRootUnlocked);
	TestTrue(TEXT("Root-gated room should be present once Root is unlocked"), SetRootUnlocked.Contains(RootGatedRoom));
	TestTrue(TEXT("Plain room should still be present once Root is unlocked"), SetRootUnlocked.Contains(PlainRoom));

	// (h-c) Null UnlockState fails closed: the Root-gated room stays excluded even
	// though nothing here claims Root is locked - a caller that forgets to pass a
	// real unlock state must never accidentally admit a gated room.
	TArray<ARoomActor*> SequenceNullUnlockState = Shuffler->ShuffleRooms(GatingPool, ERoomDifficultyTier::Easy, /*Seed=*/1, nullptr);
	TestEqual(TEXT("Null UnlockState should exclude the Root-gated room"), SequenceNullUnlockState.Num(), 1);
	if (SequenceNullUnlockState.Num() == 1)
	{
		TestEqual(TEXT("Only the plain room should come back with a null UnlockState"), SequenceNullUnlockState[0], PlainRoom);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
