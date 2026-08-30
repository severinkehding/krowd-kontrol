#include "CosmeticLocomotionComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequenceBase.h"
#include "GameFramework/Actor.h"

UCosmeticLocomotionComponent::UCosmeticLocomotionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UCosmeticLocomotionComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	if (!Owner || DeltaTime <= 0.0f)
	{
		return;
	}

	if (!TargetMesh)
	{
		TargetMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
		if (!TargetMesh)
		{
			// Nothing to drive - stop paying the tick cost. Deliberately no warning:
			// an editor-authored component briefly outliving its mesh during level
			// edits is routine, not an error worth log noise.
			SetComponentTickEnabled(false);
			return;
		}
	}

	const FVector OwnerLocation = Owner->GetActorLocation();
	if (!bHasLastOwnerLocation)
	{
		bHasLastOwnerLocation = true;
		LastOwnerLocation = OwnerLocation;
		return;
	}

	// Exponentially smoothed speed: raw per-frame speed of stop-go tick movement
	// (SetActorLocation in bursts) oscillates wildly around any threshold - the
	// 2026-08-30 playtest strobing. Alpha derives from the tau so smoothing is
	// framerate-independent.
	const float RawSpeed = FVector::Dist2D(OwnerLocation, LastOwnerLocation) / DeltaTime;
	LastOwnerLocation = OwnerLocation;
	const float Alpha = 1.0f - FMath::Exp(-DeltaTime / SpeedSmoothingTauSeconds);
	SmoothedSpeedUnitsPerSecond = FMath::Lerp(SmoothedSpeedUnitsPerSecond, RawSpeed, Alpha);

	// Hysteresis + minimum hold: enter Move only above the higher bar, leave it
	// only below the lower one, and never flip either way until the current state
	// has been visible long enough to read as intentional.
	SecondsSinceStateSwitch += DeltaTime;
	const bool bWantsMove = bInMoveState
		? SmoothedSpeedUnitsPerSecond > ExitMoveSpeedUnitsPerSecond
		: SmoothedSpeedUnitsPerSecond > EnterMoveSpeedUnitsPerSecond;
	if (bWantsMove != bInMoveState && SecondsSinceStateSwitch >= MinAnimHoldSeconds)
	{
		bInMoveState = bWantsMove;
		SecondsSinceStateSwitch = 0.0f;
	}

	if (bProceduralBob)
	{
		if (!bHasBasePose)
		{
			bHasBasePose = true;
			BaseRelativeLocation = TargetMesh->GetRelativeLocation();
			BaseRelativeRotation = TargetMesh->GetRelativeRotation();
		}
		// Blend toward/away from the move pose smoothly so starts and stops read
		// as a settle, not a snap; phase only advances while meaningfully moving,
		// so the bob dies out with the blend instead of marching in place.
		const float BlendTarget = bInMoveState ? 1.0f : 0.0f;
		MoveBlend01 = FMath::FInterpTo(MoveBlend01, BlendTarget, DeltaTime, 6.0f);
		if (MoveBlend01 > KINDA_SMALL_NUMBER)
		{
			BobPhaseRadians += DeltaTime * BobFrequencyHz * 2.0f * PI * MoveBlend01;
		}
		const float BobOffset = FMath::Abs(FMath::Sin(BobPhaseRadians)) * BobAmplitudeUnits * MoveBlend01;
		FRotator LeanedRotation = BaseRelativeRotation;
		LeanedRotation.Pitch += MoveLeanDegrees * MoveBlend01;
		TargetMesh->SetRelativeLocation(BaseRelativeLocation + FVector(0.f, 0.f, BobOffset));
		TargetMesh->SetRelativeRotation(LeanedRotation);
		return;
	}

	UAnimSequenceBase* DesiredAnim = bInMoveState ? MoveAnim.Get() : IdleAnim.Get();
	if (!DesiredAnim || DesiredAnim == CurrentAnim)
	{
		return;
	}
	CurrentAnim = DesiredAnim;
	TargetMesh->PlayAnimation(DesiredAnim, /*bLooping=*/true);
}
