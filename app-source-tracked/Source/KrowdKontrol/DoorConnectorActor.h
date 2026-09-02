#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DoorConnectorActor.generated.h"

class ARoomActor;
class UStaticMeshComponent;
class UPointLightComponent;
class UBoxComponent;

// Placeable, hand-authoring-era building block declaring that two rooms are connected
// by a door (PRD 05 REQ-1/REQ-2, issue #39). "Exactly two rooms" is a structural
// guarantee of the two named properties below, not a runtime-checked invariant on a
// collection. No longer topology-only: it also carries a floor strip spanning the
// connector (issue #187), a visible doorway marker - mesh + point light (issue #191) -
// and, since issue #218, a real blocking collision (GateBlockingComponent) that gates
// passage until GatingRoom's owned enemies are all Banked - so a bare USceneComponent
// root is no longer the whole picture.
UCLASS()
class KROWDKONTROL_API ADoorConnectorActor : public AActor
{
	GENERATED_BODY()

public:
	ADoorConnectorActor();

	// Per-placed-instance references to rooms already in the level, not a
	// Blueprint-class-default value.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Door Connector")
	TObjectPtr<ARoomActor> RoomA;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Door Connector")
	TObjectPtr<ARoomActor> RoomB;

	// False until both RoomA and RoomB are set to distinct rooms - a door can't
	// meaningfully connect a room to itself.
	bool ConnectsValidRooms() const { return RoomA != nullptr && RoomB != nullptr && RoomA != RoomB; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door Connector")
	float ConnectorFloorWidth = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door Connector")
	float ConnectorFloorThickness = 20.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door Connector")
	TObjectPtr<UStaticMeshComponent> ConnectorFloorMeshComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door Connector")
	float DoorMarkerHeight = 150.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door Connector")
	TObjectPtr<UStaticMeshComponent> DoorMarkerMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door Connector")
	TObjectPtr<UPointLightComponent> DoorMarkerLightComponent;

	// Sliding door leaves (operator scene brief, 2026-09-02): the packs' matched
	// left/right pair (SM_door_002 / SM_door_006, whose pivots both sit at the
	// closed meeting edge). Purely visual - NoCollision; the gate's actual
	// blocking stays GateBlockingComponent's job. Closed while the gate is
	// locked; once IsGateOpen(), they slide apart whenever the player pawn or
	// any enemy comes within DoorProximityRadius and glide shut again after.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door Connector")
	TObjectPtr<UStaticMeshComponent> DoorPanelLeftComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door Connector")
	TObjectPtr<UStaticMeshComponent> DoorPanelRightComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door Connector", meta = (ClampMin = "50.0"))
	float DoorProximityRadius = 450.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door Connector", meta = (ClampMin = "10.0"))
	float DoorSlideDistance = 165.f;

	// The room whose OwnedEnemies gate this door. When unset (the common, real-level
	// case), BeginPlay derives it as whichever of RoomA/RoomB sits closer to the level
	// entrance (lower world X) - the room this door is the exit from - so real placed
	// doors (which already carry RoomA/RoomB for their connector visuals) gate correctly
	// with no additional per-instance authoring. A level author can still hand-set this
	// to override the heuristic for a future non-linear chain.
	// Note: RefreshGateState() applies blocking collision independent of RoomA/RoomB - a
	// hand-set GatingRoom without valid RoomA/RoomB produces an invisible blocking wall
	// (no connector floor/marker to show for it). Keep RoomA/RoomB set whenever GatingRoom
	// is hand-overridden.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Door Connector")
	TObjectPtr<ARoomActor> GatingRoom;

	// True once the gate has ever been open (GatingRoom null or cleared); latches
	// permanently from that point on so a door already opened for the player never
	// re-closes behind them (issue #218 AC3), even if GatingRoom->IsRoomCleared() later
	// flips back to false. False (impassable) until the first time that happens.
	bool IsGateOpen() const { return bIsGateOpen; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door Connector")
	TObjectPtr<UBoxComponent> GateBlockingComponent;

	// Issue #243 / PRD Room Encounter Flow REQ-1 AC2: height/thickness of the two
	// always-on (never gated, unlike GateBlockingComponent) invisible blocking volumes
	// flanking ConnectorFloorMeshComponent on either side, so the player can't drift
	// sideways off the connector strip. They span only the corridor gap between the two
	// rooms' floor perimeters (not the full centre-to-centre distance - see
	// RecomputeConnectorGeometry()). Kept independently tunable rather than reading
	// ARoomActor's RoomWallHeight/RoomWallThickness - same reasoning
	// ConnectorFloorThickness/DoorMarkerHeight already use (a door can connect two
	// rooms with different wall dimensions).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door Connector")
	float CorridorGuardRailHeight = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door Connector")
	float CorridorGuardRailThickness = 20.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door Connector")
	TObjectPtr<UBoxComponent> CorridorGuardRailAComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door Connector")
	TObjectPtr<UBoxComponent> CorridorGuardRailBComponent;

	// Positions/rotates/scales ConnectorFloorMeshComponent and DoorMarkerMeshComponent/
	// DoorMarkerLightComponent to span/mark the straight line between RoomA and RoomB's
	// live GetActorLocation(), or hides both if the door doesn't yet connect two valid,
	// distinct rooms. Called from both OnConstruction (editor placement/property-edit
	// visibility) and BeginPlay (guarantees correctness at actual play time regardless
	// of load-time construction-script timing) - also safe and idempotent to call
	// directly from tests.
	UFUNCTION(BlueprintCallable, Category = "Door Connector")
	void RecomputeConnectorGeometry();

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	// Shared by RecomputeConnectorGeometry()'s two early-out cases (no valid rooms,
	// zero-length span) so the floor/marker mesh/marker light stay in lockstep without
	// repeating the same three SetVisibility(false) calls at each call site.
	void HideConnectorVisuals();

	// Recomputes bIsGateOpen live from GatingRoom's IsRoomCleared() and the
	// player-beyond-door term (reconciled AC3/AC4 rule - see the .cpp comment), and
	// applies the result to GateBlockingComponent's collision. UFUNCTION so it can bind
	// directly to ARoomActor::OnRoomClearedStateChanged (a dynamic multicast delegate);
	// also called directly from BeginPlay. Safe/idempotent to call repeatedly.
	UFUNCTION()
	void RefreshGateState();

	// Advances the sliding-leaf animation one frame: picks the open/closed target
	// from gate + proximity state, eases DoorPanelSlide01 toward it, and applies
	// the leaf offsets around the cached door plane.
	void TickDoorPanels(float DeltaSeconds);

	bool bIsGateOpen = true;

	friend class FKrowdKontrolDoorConnectorSlidingPanelTest;

	// Door-plane frame cached by RecomputeConnectorGeometry() for the per-frame
	// panel animation (world-space centre and the lateral slide direction).
	FVector DoorPlaneCenter = FVector::ZeroVector;
	FVector DoorLateralDirection = FVector::RightVector;
	FRotator DoorPanelRotation = FRotator::ZeroRotator;
	bool bDoorPlaneValid = false;
	float DoorPanelSlide01 = 0.f;
};
