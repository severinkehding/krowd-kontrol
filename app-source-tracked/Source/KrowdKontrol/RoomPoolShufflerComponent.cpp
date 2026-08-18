#include "RoomPoolShufflerComponent.h"
#include "RoomActor.h"
#include "RoomMetadataComponent.h"
#include "DoorConnectorActor.h"
#include "Engine/World.h"
#include "Math/RandomStream.h"

URoomPoolShufflerComponent::URoomPoolShufflerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

TArray<ARoomActor*> URoomPoolShufflerComponent::ShuffleRooms(const TArray<ARoomActor*>& RoomPool, ERoomDifficultyTier TargetTier, int32 Seed)
{
	SpawnedDoors.Reset();

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("URoomPoolShufflerComponent::ShuffleRooms: no World available, cannot spawn door connectors."));
		return TArray<ARoomActor*>();
	}

	TArray<ARoomActor*> Filtered;
	for (ARoomActor* Room : RoomPool)
	{
		if (!Room)
		{
			continue;
		}

		URoomMetadataComponent* Metadata = Room->FindComponentByClass<URoomMetadataComponent>();
		if (Metadata && Metadata->DifficultyTier == TargetTier)
		{
			Filtered.Add(Room);
		}
	}

	FRandomStream RandomStream(Seed);
	for (int32 Index = Filtered.Num() - 1; Index > 0; --Index)
	{
		int32 SwapIndex = RandomStream.RandRange(0, Index);
		Filtered.Swap(Index, SwapIndex);
	}

	for (int32 Index = 0; Index < Filtered.Num() - 1; ++Index)
	{
		ADoorConnectorActor* Door = World->SpawnActor<ADoorConnectorActor>();
		if (Door)
		{
			Door->RoomA = Filtered[Index];
			Door->RoomB = Filtered[Index + 1];
			SpawnedDoors.Add(Door);
		}
	}

	return Filtered;
}
