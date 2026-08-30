#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ScenicCircuitActor.generated.h"

class UStaticMeshComponent;

// Cosmetic-only ambient mover (operator art pass, 2026-08-30): drives a static
// mesh around a closed world-space waypoint loop at constant speed - the flying
// ships, road vehicles, and trains that make the Mars-colony backdrop feel
// alive. Deliberately dumb: distance-parameterised polyline lerp, optional
// facing along travel, optional hover bob. Gameplay-inert by construction: the
// mesh never collides, the actor touches no gameplay system, and it only ever
// writes its own transform. Levels place many instances sharing a loop shape,
// staggered with StartOffsetAlongPath.
UCLASS()
class KROWDKONTROL_API AScenicCircuitActor : public AActor
{
	GENERATED_BODY()

	friend class FKrowdKontrolScenicCircuitActorTest;

public:
	AScenicCircuitActor();

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenic Circuit")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	// Closed loop, world-space. Fewer than 2 points disables movement (the
	// actor just sits where placed - a valid "parked" configuration).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenic Circuit")
	TArray<FVector> Waypoints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenic Circuit", meta = (ClampMin = "0.0"))
	float SpeedUnitsPerSecond = 600.0f;

	// Yaw toward the current travel direction (plus YawOffsetDegrees for meshes
	// whose visual nose is not +X - most pack ships model forward as -Y).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenic Circuit")
	bool bFaceTravelDirection = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenic Circuit")
	float YawOffsetDegrees = 0.0f;

	// Stagger several movers around one shared loop shape.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenic Circuit", meta = (ClampMin = "0.0"))
	float StartOffsetAlongPath = 0.0f;

	// Gentle vertical hover bob for airborne craft; 0 disables (ground vehicles).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenic Circuit", meta = (ClampMin = "0.0"))
	float BobAmplitudeUnits = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenic Circuit", meta = (ClampMin = "0.01"))
	float BobFrequencyHz = 0.2f;

private:
	// Evaluates the loop at DistanceAlongPath and writes the actor transform.
	void ApplyPathPosition();

	float TotalPathLength() const;

	float DistanceAlongPath = 0.0f;
	float BobPhaseSeconds = 0.0f;
};
