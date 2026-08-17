#include "AbilityUnlockComponent.h"

namespace
{
	// Level 1 (Stun) is deliberately absent - it's already unlocked at construction,
	// not something NotifyLevelReached ever needs to grant. Matches MISSION.md's
	// 5-level reconciliation (operator decision 2026-08-17, resolving issue #69).
	const TMap<int32, EAbilitySlot>& GetLevelToAbilityMap()
	{
		static const TMap<int32, EAbilitySlot> LevelToAbility = {
			{ 2, EAbilitySlot::Sleep },
			{ 3, EAbilitySlot::Root },
			{ 4, EAbilitySlot::Fear },
			{ 5, EAbilitySlot::Snare },
		};
		return LevelToAbility;
	}
}

UAbilityUnlockComponent::UAbilityUnlockComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	SlotUnlocked.Init(false, NumAbilitySlots);
	SlotUnlocked[static_cast<int32>(EAbilitySlot::Stun)] = true;
}

bool UAbilityUnlockComponent::IsAbilityUnlocked(EAbilitySlot Ability) const
{
	const int32 Index = static_cast<int32>(Ability);
	return SlotUnlocked.IsValidIndex(Index) && SlotUnlocked[Index];
}

void UAbilityUnlockComponent::NotifyLevelReached(int32 LevelIndex)
{
	const EAbilitySlot* Found = GetLevelToAbilityMap().Find(LevelIndex);
	if (!Found)
	{
		return;
	}
	UnlockAbility(*Found);
}

void UAbilityUnlockComponent::UnlockAbility(EAbilitySlot Ability)
{
	const int32 Index = static_cast<int32>(Ability);
	if (!SlotUnlocked.IsValidIndex(Index) || SlotUnlocked[Index])
	{
		return;
	}
	SlotUnlocked[Index] = true;
	OnAbilityUnlocked.Broadcast(Ability);
}
