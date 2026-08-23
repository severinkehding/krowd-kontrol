#include "AbilityCooldownComponent.h"

UAbilityCooldownComponent::UAbilityCooldownComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	AbilityCooldownDurations.Init(DefaultAbilityCooldownSeconds, NumAbilitySlots);
	RemainingCooldownSeconds.Init(0.0f, NumAbilitySlots);
}

bool UAbilityCooldownComponent::TryStartCooldown(EAbilitySlot AbilitySlot)
{
	const int32 Index = static_cast<int32>(AbilitySlot);
	if (!RemainingCooldownSeconds.IsValidIndex(Index))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UAbilityCooldownComponent::TryStartCooldown: index %d invalid on '%s'."),
			Index, *GetNameSafe(this));
		return false;
	}

	if (RemainingCooldownSeconds[Index] > 0.0f)
	{
		return false; // still on cooldown - recast blocked, no state change
	}

	const float ConfiguredDuration = AbilityCooldownDurations.IsValidIndex(Index) ? AbilityCooldownDurations[Index] : DefaultAbilityCooldownSeconds;
	RemainingCooldownSeconds[Index] = FMath::Max(0.0f, ConfiguredDuration);
	if (RemainingCooldownSeconds[Index] > 0.0f)
	{
		OnAbilityCooldownChanged.Broadcast(AbilitySlot, true);
	}
	return true;
}

void UAbilityCooldownComponent::AdvanceCooldowns(float DeltaSeconds)
{
	for (int32 Index = 0; Index < RemainingCooldownSeconds.Num(); ++Index)
	{
		if (RemainingCooldownSeconds[Index] > 0.0f)
		{
			RemainingCooldownSeconds[Index] = FMath::Max(0.0f, RemainingCooldownSeconds[Index] - DeltaSeconds);
			if (RemainingCooldownSeconds[Index] <= 0.0f)
			{
				OnAbilityCooldownChanged.Broadcast(static_cast<EAbilitySlot>(Index), false);
			}
		}
	}
}

bool UAbilityCooldownComponent::IsOnCooldown(EAbilitySlot AbilitySlot) const
{
	const int32 Index = static_cast<int32>(AbilitySlot);
	if (!RemainingCooldownSeconds.IsValidIndex(Index))
	{
		return false;
	}
	return RemainingCooldownSeconds[Index] > 0.0f;
}

float UAbilityCooldownComponent::GetRemainingCooldownSeconds(EAbilitySlot AbilitySlot) const
{
	const int32 Index = static_cast<int32>(AbilitySlot);
	if (!RemainingCooldownSeconds.IsValidIndex(Index))
	{
		return 0.0f;
	}
	return RemainingCooldownSeconds[Index];
}

void UAbilityCooldownComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AdvanceCooldowns(DeltaTime);
}
