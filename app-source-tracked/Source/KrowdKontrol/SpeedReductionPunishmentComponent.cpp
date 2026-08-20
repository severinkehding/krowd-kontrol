#include "SpeedReductionPunishmentComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Engine/World.h"
#include "TimerManager.h"

USpeedReductionPunishmentComponent::USpeedReductionPunishmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USpeedReductionPunishmentComponent::HandlePunishmentTriggered()
{
	if (!MovementComponent)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!World->GetTimerManager().IsTimerActive(SpeedReductionTimerHandle))
	{
		// Not already active - remember the real pre-punishment speed before
		// reducing it. If already active, OriginalMaxSpeed already holds that
		// real value; re-deriving it from the currently-reduced MaxSpeed (or
		// reapplying the multiplier) would compound the reduction on repeat
		// triggers, which REQ-3 explicitly forbids.
		OriginalMaxSpeed = MovementComponent->MaxSpeed;
		MovementComponent->MaxSpeed = OriginalMaxSpeed * SpeedMultiplierWhileActive;
	}

	// SetTimer on an already-active handle replaces it rather than stacking a
	// second one - this alone gives re-triggering its "refresh duration, don't
	// compound" behavior.
	World->GetTimerManager().SetTimer(
		SpeedReductionTimerHandle, this, &USpeedReductionPunishmentComponent::RestoreOriginalSpeed,
		SpeedReductionDurationSeconds, false);
}

void USpeedReductionPunishmentComponent::RestoreOriginalSpeed()
{
	if (MovementComponent)
	{
		MovementComponent->MaxSpeed = OriginalMaxSpeed;
	}
}

void USpeedReductionPunishmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SpeedReductionTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}
