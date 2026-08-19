#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RoomDifficultyTier.h"
#include "RoomPoolShufflerComponent.generated.h"

class ARoomActor;
class ADoorConnectorActor;
class UAbilityUnlockComponent;

// Implements the P1 room-pool shuffler (PRD 05 REQ-4/REQ-6, issue #51): given a pool
// of hand-authored ARoomActor instances (each optionally tagged with a
// URoomMetadataComponent) and a target ERoomDifficultyTier, ShuffleRooms() filters the
// pool down to rooms whose DifficultyTier exactly matches (rooms with no metadata
// component are excluded - they can't be sequenced without tags), shuffles the
// filtered subset with a seeded FRandomStream (deterministic per seed - same seed
// always reproduces the same order, different seeds are expected to differ, the PRD's
// own success metric), and spawns a linear chain of ADoorConnectorActor instances
// connecting each consecutive pair - the same topology shape the hand-authored Alpha
// levels already use. ShuffleRooms() also enforces REQ-5 (ability-gating, issue #53):
// any room whose URoomMetadataComponent::RequiredAbility names an ability the passed
// UnlockState doesn't report as unlocked is excluded from the filtered pool.
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class KROWDKONTROL_API URoomPoolShufflerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URoomPoolShufflerComponent();

	// Filters RoomPool to rooms tagged TargetTier whose RequiredAbility (if any) is
	// unlocked per UnlockState, shuffles the result deterministically per Seed, and
	// spawns an ADoorConnectorActor chaining each consecutive pair. Returns the
	// ordered, filtered sequence. Repeated calls on the same component instance reset
	// any doors spawned by a previous call. UnlockState mirrors the pointer-consumer
	// pattern UAbilityCooldownTrayWidget::BindAbilityUnlockComponent() already
	// established for the same class; a null UnlockState is treated as "nothing
	// unlocked" (every ability-gated room excluded, fail closed), not as "skip the
	// ability check."
	UFUNCTION(BlueprintCallable, Category = "Room Pool Shuffler")
	TArray<ARoomActor*> ShuffleRooms(const TArray<ARoomActor*>& RoomPool, ERoomDifficultyTier TargetTier, int32 Seed, const UAbilityUnlockComponent* UnlockState);

	const TArray<TObjectPtr<ADoorConnectorActor>>& GetSpawnedDoors() const { return SpawnedDoors; }

private:
	UPROPERTY()
	TArray<TObjectPtr<ADoorConnectorActor>> SpawnedDoors;
};
