#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RoomEnemyBudgetController.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRoomCleared);

// Keeps a room's active enemy count pinned near MaxConcurrentDensity as enemies are
// banked, spawning replacements from TotalRoomBudget until the budget is exhausted
// and the active pool is empty (PRD 01 loop steps 6-7; REQ-6 - enemy count, not
// damage, is the difficulty lever). See issue #82. Placeholder-actor-first per
// MISSION.md: no real enemy class exists yet, so EnemyClassToSpawn is whatever
// placeholder actor the room's designer assigns (APlaceholderCubeActor for now).
//
// Attach to a room/level-management actor. Does not wire itself to any banking
// trigger (e.g. a future ATargetZone::OnActorBanked) - callers invoke
// NotifyEnemyBanked() directly. See issue #82's Notes.
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class KROWDKONTROL_API URoomEnemyBudgetController : public UActorComponent
{
	GENERATED_BODY()

public:
	URoomEnemyBudgetController();

	// Total enemies this room will ever spawn across its lifetime, including the
	// initial fill at room start.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Room Budget")
	int32 TotalRoomBudget = 0;

	// Cap on simultaneously active enemies - spawning never exceeds this. Must be at
	// least 1: a value of 0 permanently soft-locks the room (nothing ever spawns, so
	// OnRoomCleared never fires either).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Room Budget", meta = (ClampMin = "1"))
	int32 MaxConcurrentDensity = 0;

	// Placeholder-actor-first (MISSION.md): no real enemy class exists yet.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Room Budget")
	TSubclassOf<AActor> EnemyClassToSpawn;

	// Fires exactly once, when TotalRoomBudget and the active count have both
	// reached zero.
	UPROPERTY(BlueprintAssignable, Category = "Room Budget")
	FOnRoomCleared OnRoomCleared;

	// Fills the room up to MaxConcurrentDensity (bounded by TotalRoomBudget). Called
	// automatically from BeginPlay(); exposed publicly (and made idempotent) so
	// callers - including the Automation Framework test - can trigger the initial
	// spawn wave deterministically without needing to drive the engine's full actor
	// BeginPlay lifecycle.
	UFUNCTION(BlueprintCallable, Category = "Room Budget")
	void InitializeRoom();

	// Called by future banking code (e.g. a room's ATargetZone::OnActorBanked) when
	// an active enemy is banked. Decrements the active count and spawns a
	// replacement while budget remains and density allows it.
	UFUNCTION(BlueprintCallable, Category = "Room Budget")
	void NotifyEnemyBanked();

	int32 GetRemainingBudget() const { return RemainingBudget; }
	int32 GetActiveEnemyCount() const { return ActiveEnemyCount; }

protected:
	virtual void BeginPlay() override;

private:
	void SpawnEnemy();
	void CheckForRoomCleared();

	UPROPERTY()
	int32 RemainingBudget = 0;

	UPROPERTY()
	int32 ActiveEnemyCount = 0;

	bool bHasInitializedRoom = false;
	bool bRoomClearedFired = false;
};
