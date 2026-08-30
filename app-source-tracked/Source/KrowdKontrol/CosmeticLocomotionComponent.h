#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CosmeticLocomotionComponent.generated.h"

class USkeletalMeshComponent;
class UAnimSequenceBase;

// Cosmetic-only locomotion animation driver (operator art pass, 2026-08-30):
// swaps a sibling USkeletalMeshComponent between an Idle and a Move animation
// based on the OWNER ACTOR's real measured speed. Exists because this project's
// enemies are plain AActors moved directly by their own tick logic
// (SetActorLocation - no CharacterMovementComponent, no velocity), so stock
// AnimBlueprints like the Paragon packs' (which read TryGetPawnOwner()
// velocity) sit permanently in idle on them. Single-node playback only - no
// blending, no state machine - deliberately placeholder-grade, matching the
// cosmetic-mesh-over-collision-cube approach it accompanies.
//
// Switching is deliberately damped three ways (2026-08-30 playtest: a single
// raw threshold made stop-go chase movement strobe Idle<->Jog every few
// frames): the measured speed is exponentially smoothed, enter/exit use
// separate thresholds (hysteresis), and a clip must have played
// MinAnimHoldSeconds before it may be swapped.
//
// For rigs with NO animation clips (the Fab cute-robot FBX ships bare),
// bProceduralBob instead bobs and leans the mesh itself while moving - a
// placeholder-tier waddle that needs no retargeting. Gameplay-inert either
// way: reads the owner's transform, writes only rendered animation/mesh
// offsets - never movement, collision, or the enemy state machine.
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class KROWDKONTROL_API UCosmeticLocomotionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCosmeticLocomotionComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetic Locomotion")
	TObjectPtr<UAnimSequenceBase> IdleAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetic Locomotion")
	TObjectPtr<UAnimSequenceBase> MoveAnim;

	// Hysteresis pair: smoothed speed must rise ABOVE Enter to switch to MoveAnim,
	// and fall BELOW Exit to switch back. Keep Exit meaningfully lower than Enter
	// or the flapping this pair exists to prevent comes back.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetic Locomotion", meta = (ClampMin = "1.0"))
	float EnterMoveSpeedUnitsPerSecond = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetic Locomotion", meta = (ClampMin = "0.0"))
	float ExitMoveSpeedUnitsPerSecond = 60.0f;

	// A clip plays at least this long before the state may flip again.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetic Locomotion", meta = (ClampMin = "0.0"))
	float MinAnimHoldSeconds = 0.35f;

	// Exponential smoothing time constant for the measured speed (bigger = calmer).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetic Locomotion", meta = (ClampMin = "0.01"))
	float SpeedSmoothingTauSeconds = 0.15f;

	// Anim-less rigs: bob/lean the mesh procedurally while moving instead of
	// swapping clips. IdleAnim/MoveAnim are ignored when this is set.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetic Locomotion|Procedural")
	bool bProceduralBob = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetic Locomotion|Procedural", meta = (ClampMin = "0.0"))
	float BobAmplitudeUnits = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetic Locomotion|Procedural", meta = (ClampMin = "0.1"))
	float BobFrequencyHz = 2.2f;

	// Forward lean (degrees of pitch) blended in at full move state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetic Locomotion|Procedural")
	float MoveLeanDegrees = 6.0f;

private:
	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> TargetMesh;

	FVector LastOwnerLocation = FVector::ZeroVector;
	bool bHasLastOwnerLocation = false;

	float SmoothedSpeedUnitsPerSecond = 0.0f;
	bool bInMoveState = false;
	float SecondsSinceStateSwitch = 0.0f;

	// Procedural-bob bookkeeping: the mesh's authored relative pose, captured on
	// first tick so offsets are always applied against it (never accumulated).
	FVector BaseRelativeLocation = FVector::ZeroVector;
	FRotator BaseRelativeRotation = FRotator::ZeroRotator;
	bool bHasBasePose = false;
	float BobPhaseRadians = 0.0f;
	float MoveBlend01 = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> CurrentAnim;
};
