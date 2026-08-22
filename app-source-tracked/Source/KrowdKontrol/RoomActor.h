#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemyType.h"
#include "RoomActor.generated.h"

class APlaceholderTargetZoneActor;
class UStaticMeshComponent;
class AEnemyBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRoomClearedStateChanged);

// One tagged target-zone marker child of an ARoomActor: which EEnemyType it serves,
// and the spawned marker actor itself (see APlaceholderTargetZoneActor - no real
// ATargetZone class exists yet, per RoomEnemyBudgetController.h's reserved
// OnActorBanked integration point).
USTRUCT(BlueprintType)
struct FRoomTargetZone
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Target Zone")
	EEnemyType EnemyType = EEnemyType::RU_NNR;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Target Zone")
	TObjectPtr<AActor> MarkerActor;
};

// Placeable, hand-authoring-era building block for a level's room topology (PRD 05
// REQ-1/REQ-2, issue #39): holds an arbitrary number of tagged target-zone marker
// children, added via AddTargetZone(). Structural/topology only - no enemy AI,
// ability, or HUD logic. Room-pool/connector-shuffler generation (PRD 05
// REQ-4/REQ-5/REQ-6) is a separate future P1 issue, not built here.
UCLASS()
class KROWDKONTROL_API ARoomActor : public AActor
{
	GENERATED_BODY()

public:
	ARoomActor();

	// Spawns MarkerClass (or APlaceholderTargetZoneActor if unset), attaches it as a
	// child of this room, tags it with EnemyType, and records the pair. Returns the
	// spawned marker actor, or nullptr if spawning failed.
	UFUNCTION(BlueprintCallable, Category = "Room")
	AActor* AddTargetZone(EEnemyType EnemyType, TSubclassOf<AActor> MarkerClass = nullptr);

	const TArray<FRoomTargetZone>& GetTargetZones() const { return TargetZones; }

	// Enemies this room must clear before its gated door(s) open. Hand-placed for
	// enemies present at level start (same EditInstanceOnly, per-placed-instance shape
	// as ADoorConnectorActor::RoomA/RoomB); AddOwnedEnemy() extends this at runtime for
	// enemies added later (e.g. a wave spawn).
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Room|Enemies")
	TArray<TObjectPtr<AEnemyBase>> OwnedEnemies;

	// True once every currently-owned enemy has reached Banked (vacuously true for an
	// empty list - a room with nothing to clear never gates its own doors).
	UFUNCTION(BlueprintCallable, Category = "Room|Enemies")
	bool IsRoomCleared() const;

	// Adds Enemy to this room's ownership (no-op if already owned or null) and binds its
	// OnEnemyBanked so the room's cleared-state is re-evaluated when it banks. Broadcasts
	// OnRoomClearedStateChanged immediately - a room that was previously cleared becomes
	// un-cleared the moment a new enemy is added, re-gating any door bound to it. This is
	// the hook a future wave-spawner integration calls; this issue's own tests call it
	// directly to simulate that without depending on UWaveSpawnerComponent (unwired to
	// ARoomActor today).
	UFUNCTION(BlueprintCallable, Category = "Room|Enemies")
	void AddOwnedEnemy(AEnemyBase* Enemy);

	// Fires whenever IsRoomCleared() may have changed - after an owned enemy banks, or
	// after AddOwnedEnemy() adds a not-yet-banked enemy. ADoorConnectorActor::GatingRoom
	// binds to this rather than polling.
	UPROPERTY(BlueprintAssignable, Category = "Room|Enemies")
	FOnRoomClearedStateChanged OnRoomClearedStateChanged;

	// Half-extents (cm) of the room's greybox floor slab - full floor is 2x this.
	// Rooms in both hand-authored levels are spaced 3000cm apart along the chain axis
	// (docs/prd-level-playability-presentation.md:31-33), so the 1000cm default leaves
	// room for a connector strip without overlapping the next room's floor.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
	FVector2D RoomFloorExtent = FVector2D(1000.f, 1000.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
	float RoomFloorThickness = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
	float RoomWallHeight = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
	float RoomWallThickness = 20.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room")
	TObjectPtr<UStaticMeshComponent> FloorMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room")
	TObjectPtr<UStaticMeshComponent> WallNorthMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room")
	TObjectPtr<UStaticMeshComponent> WallSouthMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room")
	TObjectPtr<UStaticMeshComponent> WallEastMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room")
	TObjectPtr<UStaticMeshComponent> WallWestMeshComponent;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleOwnedEnemyBanked();

	void BindOwnedEnemyDelegate(AEnemyBase* Enemy);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room", meta = (AllowPrivateAccess = "true"))
	TArray<FRoomTargetZone> TargetZones;
};
