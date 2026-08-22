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

	// Enemies this room must clear before its gated door(s) open. Auto-discovered in
	// BeginPlay via nearest-room-by-distance over every AEnemyBase in the world (issue
	// #218) - the same rule KrowdKontrolLevelTestUtils::FindNearestRoom already uses and
	// the real levels' structural regression tests already trust, so real placed enemies
	// wire up correctly with zero .umap authoring. AddOwnedEnemy() extends this at
	// runtime for enemies added later (e.g. a future wave spawn).
	const TArray<TObjectPtr<AEnemyBase>>& GetOwnedEnemies() const { return OwnedEnemies; }

	// True once every currently-owned enemy has reached Banked, or is already being
	// destroyed (see HandleOwnedEnemyDestroyed) - vacuously true for an empty list, so a
	// room with nothing to clear never gates its own doors.
	UFUNCTION(BlueprintCallable, Category = "Room|Enemies")
	bool IsRoomCleared() const;

	// Adds Enemy to this room's ownership (no-op if already owned or invalid) and binds
	// its OnEnemyBanked/OnDestroyed so the room's cleared-state is re-evaluated when it
	// banks or is otherwise removed. Broadcasts OnRoomClearedStateChanged immediately - a
	// room that was previously cleared becomes un-cleared the moment a new enemy is
	// added, re-gating any door bound to it. This is the hook a future wave-spawner
	// integration calls; this issue's own tests call it directly to simulate that
	// without depending on UWaveSpawnerComponent (unwired to ARoomActor today).
	UFUNCTION(BlueprintCallable, Category = "Room|Enemies")
	void AddOwnedEnemy(AEnemyBase* Enemy);

	// Fires whenever IsRoomCleared() may have changed - after an owned enemy banks, or
	// after AddOwnedEnemy() adds a not-yet-banked enemy. ADoorConnectorActor::GatingRoom
	// binds to this rather than polling.
	UPROPERTY(BlueprintAssignable, Category = "Room|Enemies")
	FOnRoomClearedStateChanged OnRoomClearedStateChanged;

	// Nearest-room-by-distance: the same "which room owns this actor" rule
	// KrowdKontrolLevelTestUtils::FindNearestRoom documents and now delegates to (rather
	// than duplicating the comparison), so BeginPlay's auto-discovery below and the test
	// suite's expectations can never drift apart.
	static ARoomActor* FindNearestRoom(const AActor* Actor, const TArray<ARoomActor*>& Rooms);

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
	// Bound to each owned enemy's OnEnemyBanked.
	UFUNCTION()
	void HandleOwnedEnemyBanked();

	// Bound to each owned enemy's AActor::OnDestroyed - a room whose last un-banked
	// enemy is destroyed by something other than banking (e.g. editor/engine-level
	// destruction) still re-evaluates and opens its gated door instead of soft-locking.
	// AActor::OnDestroyed fires synchronously while the actor is still IsValid() (not
	// yet garbage), which is why IsRoomCleared() also checks IsActorBeingDestroyed(),
	// not just enemy state.
	UFUNCTION()
	void HandleOwnedEnemyDestroyed(AActor* DestroyedActor);

	void BindOwnedEnemyDelegate(AEnemyBase* Enemy);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room", meta = (AllowPrivateAccess = "true"))
	TArray<FRoomTargetZone> TargetZones;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room|Enemies", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<AEnemyBase>> OwnedEnemies;
};
