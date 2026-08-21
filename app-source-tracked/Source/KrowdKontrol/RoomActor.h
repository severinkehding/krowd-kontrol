#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemyType.h"
#include "RoomActor.generated.h"

class APlaceholderTargetZoneActor;
class ATargetZone;
class UStaticMeshComponent;

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

	// Self-heals a colour-tagged ATargetZone attached to each already-placed marker
	// in TargetZones that doesn't have one yet (issue #211) - the code-only path for
	// rooms placed/serialized before this class carried banking behaviour, mirroring
	// APlaceholderTargetZoneActor::EnsureBeaconHierarchy()'s "unconditionally re-check
	// and fix, safe to call more than once" shape. Called automatically from
	// BeginPlay(); exposed publicly (and made idempotent) so callers - including the
	// Automation Framework test - can trigger it deterministically without needing to
	// drive the engine's full actor BeginPlay lifecycle, same rationale
	// URoomEnemyBudgetController::InitializeRoom() documents for its own public
	// idempotent entry point.
	UFUNCTION(BlueprintCallable, Category = "Room")
	void EnsureBankingZonesWired();

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
	// Routes a banked regular enemy into its own TransitionToBanked() - the "room-
	// scope owner" subscriber the issue's Ask #3 calls out as an acceptable
	// alternative to the zone subscribing to itself. Deliberately does NOT call
	// URoomEnemyBudgetController::NotifyEnemyBanked() - that integration is separate,
	// out-of-scope future work per RoomEnemyBudgetController.h's own comment.
	UFUNCTION()
	void HandleZoneActorBanked(AActor* BankedActor);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room", meta = (AllowPrivateAccess = "true"))
	TArray<FRoomTargetZone> TargetZones;
};
