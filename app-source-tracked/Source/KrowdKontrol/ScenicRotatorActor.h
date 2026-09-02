#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ScenicRotatorActor.generated.h"

class UStaticMeshComponent;

// Cosmetic-only rotation animator (intro-scene direction, 2026-09-02): sweeps a
// static mesh from StartRotation to EndRotation over DurationSeconds, easing in
// and out, then either holds (Once) or reverses forever (PingPong - a scanning
// satellite dish). Same gameplay-inert contract as AScenicCircuitActor: no
// collision, no gameplay systems, only its own transform.
UENUM()
enum class EScenicRotatePlayMode : uint8
{
	Once,
	PingPong
};

UCLASS()
class KROWDKONTROL_API AScenicRotatorActor : public AActor
{
	GENERATED_BODY()

	friend class FKrowdKontrolScenicRotatorActorTest;

public:
	AScenicRotatorActor();

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenic Rotator")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenic Rotator")
	FRotator StartRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenic Rotator")
	FRotator EndRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenic Rotator", meta = (ClampMin = "0.05"))
	float DurationSeconds = 7.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenic Rotator")
	EScenicRotatePlayMode PlayMode = EScenicRotatePlayMode::PingPong;

private:
	// Applies the eased Start->End rotation for normalised progress [0..1].
	void ApplyProgress(float Alpha01);

	float ElapsedSeconds = 0.0f;
};
