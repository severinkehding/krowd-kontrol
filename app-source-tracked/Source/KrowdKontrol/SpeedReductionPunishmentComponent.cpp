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
		UE_LOG(LogTemp, Warning,
			TEXT("USpeedReductionPunishmentComponent: MovementComponent is unset on '%s' - punishment trigger ignored."),
			*GetNameSafe(GetOwner()));
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
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("USpeedReductionPunishmentComponent: MovementComponent is unset on '%s' - speed restore skipped."),
			*GetNameSafe(GetOwner()));
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

bool USpeedReductionPunishmentComponent::IsSpeedReductionTimerActive() const
{
	if (const UWorld* World = GetWorld())
	{
		return World->GetTimerManager().IsTimerActive(SpeedReductionTimerHandle);
	}
	return false;
}
