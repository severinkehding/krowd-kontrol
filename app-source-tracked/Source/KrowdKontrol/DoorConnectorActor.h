#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DoorConnectorActor.generated.h"

class ARoomActor;
class UStaticMeshComponent;

// Placeable, hand-authoring-era building block declaring that two rooms are connected
// by a door (PRD 05 REQ-1/REQ-2, issue #39). "Exactly two rooms" is a structural
// guarantee of the two named properties below, not a runtime-checked invariant on a
// collection. Structural/topology only - no door visual/mesh is required by the
// issue's acceptance criteria, so this gets a bare USceneComponent root and nothing
// else.
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

	// Positions/rotates/scales ConnectorFloorMeshComponent to span the straight line
	// between RoomA and RoomB's live GetActorLocation(), or hides it if the door
	// doesn't yet connect two valid, distinct rooms. Called from both OnConstruction
	// (editor placement/property-edit visibility) and BeginPlay (guarantees
	// correctness at actual play time regardless of load-time construction-script
	// timing) - also safe and idempotent to call directly from tests.
	UFUNCTION(BlueprintCallable, Category = "Door Connector")
	void RecomputeConnectorGeometry();

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
};
