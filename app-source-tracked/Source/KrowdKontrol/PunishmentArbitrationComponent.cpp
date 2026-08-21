#include "PunishmentArbitrationComponent.h"
#include "AbilityLockoutComponent.h"
#include "SpeedReductionPunishmentComponent.h"
#include "GameFramework/Actor.h"

UPunishmentArbitrationComponent::UPunishmentArbitrationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPunishmentArbitrationComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		OvercrowdComponent = Owner->FindComponentByClass<UOvercrowdDetectionComponent>();
		if (OvercrowdComponent)
		{
			OvercrowdComponent->OnPanicOverloadStateChanged.AddDynamic(this, &UPunishmentArbitrationComponent::HandlePanicOverloadStateChanged);
		}
	}
}

void UPunishmentArbitrationComponent::HandlePunishmentTriggered()
{
	if (IsOvercrowdActive())
	{
		// Overcrowd (priority 1) is active - this contact-damage trigger is dropped
		// entirely per issue #180 AC, not queued for after Overcrowd ends.
		return;
	}

	if (AbilityLockoutComponent && UAbilityLockoutComponent::IsLockoutEnabledByCVar())
	{
		// Ability-lock (priority 2) always wins this shared trigger over speed-reduction
		// (priority 3) whenever both exist on this pawn - end any active speed-reduction
		// immediately rather than let it keep running alongside the ability-lock about to
		// start. Skipped (falls through to speed-reduction below) when
		// kk.Punishment.LockoutEnabled is 0 (issue #181) - otherwise this CVar would end an
		// active speed-reduction and then call a now-gated no-op, isolating nothing instead
		// of letting a developer isolate speed-reduction for playtesting as intended.
		if (SpeedReductionComponent)
		{
			SpeedReductionComponent->EndSpeedReduction();
		}
		AbilityLockoutComponent->HandlePunishmentTriggered();
		return;
	}

	// No ability-lock component on this pawn (e.g. Paper2DPrototypePawn), or lockout is
	// CVar-disabled for isolated playtesting (issue #181) - speed-reduction is the next
	// candidate and activates normally.
	if (SpeedReductionComponent)
	{
		SpeedReductionComponent->HandlePunishmentTriggered();
		return;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("UPunishmentArbitrationComponent: neither AbilityLockoutComponent nor SpeedReductionComponent is set on '%s' - punishment trigger ignored."),
		*GetNameSafe(GetOwner()));
}

void UPunishmentArbitrationComponent::HandlePanicOverloadStateChanged(EPanicOverloadState NewState)
{
	if (NewState != EPanicOverloadState::Active)
	{
		// Recovery (Active -> Inactive) does not resurrect a preempted punishment - issue
		// #180 AC: a dropped/ended lower-priority activation is never queued.
		return;
	}

	if (AbilityLockoutComponent)
	{
		AbilityLockoutComponent->EndAllLockouts();
	}
	if (SpeedReductionComponent)
	{
		SpeedReductionComponent->EndSpeedReduction();
	}
}

bool UPunishmentArbitrationComponent::IsOvercrowdActive() const
{
	return OvercrowdComponent && OvercrowdComponent->GetPanicOverloadState() == EPanicOverloadState::Active;
}
