#include "AbilityLockoutComponent.h"

UAbilityLockoutComponent::UAbilityLockoutComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	RemainingLockoutSeconds.Init(0.0f, NumAbilitySlots);
}

bool UAbilityLockoutComponent::IsAbilityLocked(EAbilitySlot Ability) const
{
	const int32 Index = static_cast<int32>(Ability);
	if (!RemainingLockoutSeconds.IsValidIndex(Index))
	{
		return false;
	}
	return RemainingLockoutSeconds[Index] > 0.0f;
}

float UAbilityLockoutComponent::GetRemainingLockoutSeconds(EAbilitySlot Ability) const
{
	const int32 Index = static_cast<int32>(Ability);
	if (!RemainingLockoutSeconds.IsValidIndex(Index))
	{
		return 0.0f;
	}
	return RemainingLockoutSeconds[Index];
}

void UAbilityLockoutComponent::HandleAbilityCastApplied(EAbilitySlot Ability, AEnemyBase* TargetEnemy)
{
	LastCastAbility = Ability;
}

void UAbilityLockoutComponent::HandlePunishmentTriggered()
{
	StartLockout(LastCastAbility);
}

void UAbilityLockoutComponent::StartLockout(EAbilitySlot Ability)
{
	const int32 Index = static_cast<int32>(Ability);
	if (!RemainingLockoutSeconds.IsValidIndex(Index))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UAbilityLockoutComponent::StartLockout: index %d invalid on '%s'."),
			Index, *GetNameSafe(this));
		return;
	}

	const bool bWasLocked = RemainingLockoutSeconds[Index] > 0.0f;
	RemainingLockoutSeconds[Index] = FMath::Max(0.0f, LockoutDurationSeconds);
	if (!bWasLocked && RemainingLockoutSeconds[Index] > 0.0f)
	{
		OnAbilityLockoutChanged.Broadcast(Ability, true);
	}
	RefreshIsLockoutActive();
}

void UAbilityLockoutComponent::EndAllLockouts()
{
	for (int32 Index = 0; Index < RemainingLockoutSeconds.Num(); ++Index)
	{
		if (RemainingLockoutSeconds[Index] > 0.0f)
		{
			RemainingLockoutSeconds[Index] = 0.0f;
			OnAbilityLockoutChanged.Broadcast(static_cast<EAbilitySlot>(Index), false);
		}
	}
	RefreshIsLockoutActive();
}

void UAbilityLockoutComponent::AdvanceLockouts(float DeltaSeconds)
{
	for (int32 Index = 0; Index < RemainingLockoutSeconds.Num(); ++Index)
	{
		if (RemainingLockoutSeconds[Index] > 0.0f)
		{
			RemainingLockoutSeconds[Index] = FMath::Max(0.0f, RemainingLockoutSeconds[Index] - DeltaSeconds);
			if (RemainingLockoutSeconds[Index] <= 0.0f)
			{
				OnAbilityLockoutChanged.Broadcast(static_cast<EAbilitySlot>(Index), false);
			}
		}
	}
	RefreshIsLockoutActive();
}

void UAbilityLockoutComponent::RefreshIsLockoutActive()
{
	bIsLockoutActive = false;
	for (float Remaining : RemainingLockoutSeconds)
	{
		if (Remaining > 0.0f)
		{
			bIsLockoutActive = true;
			break;
		}
	}
}

void UAbilityLockoutComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AdvanceLockouts(DeltaTime);
}
