#include "RoomPoolShufflerComponent.h"
#include "RoomActor.h"
#include "RoomMetadataComponent.h"
#include "DoorConnectorActor.h"
#include "AbilityUnlockComponent.h"
#include "Engine/World.h"
#include "Math/RandomStream.h"

URoomPoolShufflerComponent::URoomPoolShufflerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

TArray<ARoomActor*> URoomPoolShufflerComponent::ShuffleRooms(const TArray<ARoomActor*>& RoomPool, ERoomDifficultyTier TargetTier, int32 Seed, const UAbilityUnlockComponent* UnlockState)
{
	for (ADoorConnectorActor* Door : SpawnedDoors)
	{
		if (Door)
		{
			Door->Destroy();
		}
	}
	SpawnedDoors.Reset();

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("URoomPoolShufflerComponent::ShuffleRooms: no World available, cannot spawn door connectors."));
		return TArray<ARoomActor*>();
	}

	if (!UnlockState)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("URoomPoolShufflerComponent::ShuffleRooms: null UnlockState, treating every ability-gated room as locked."));
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
			// ERoomAbilityGate shares EAbilitySlot's Stun/Sleep/Root/Fear/Snare order,
			// offset by one slot for None (RoomAbilityGate.h) - never convert None.
			if (Metadata->RequiredAbility != ERoomAbilityGate::None)
			{
				const EAbilitySlot RequiredSlot = static_cast<EAbilitySlot>(static_cast<uint8>(Metadata->RequiredAbility) - 1);
				if (!UnlockState || !UnlockState->IsAbilityUnlocked(RequiredSlot))
				{
					continue;
				}
			}
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
