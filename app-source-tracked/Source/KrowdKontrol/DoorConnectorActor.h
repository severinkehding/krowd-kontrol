#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DoorConnectorActor.generated.h"

class ARoomActor;
class UStaticMeshComponent;
class UPointLightComponent;

// Placeable, hand-authoring-era building block declaring that two rooms are connected
// by a door (PRD 05 REQ-1/REQ-2, issue #39). "Exactly two rooms" is a structural
// guarantee of the two named properties below, not a runtime-checked invariant on a
// collection. No longer topology-only: it also carries a floor strip spanning the
// connector (issue #187) and a visible doorway marker - mesh + point light (issue
// #191) - so a bare USceneComponent root is no longer the whole picture.
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

private:
	// Shared by RecomputeConnectorGeometry()'s two early-out cases (no valid rooms,
	// zero-length span) so the floor/marker mesh/marker light stay in lockstep without
	// repeating the same three SetVisibility(false) calls at each call site.
	void HideConnectorVisuals();
};
