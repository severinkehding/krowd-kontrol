#include "AbilityData.h"
#include "ReservedGameplayColours.h"

namespace
{
	const FAbilityData& GetStun()
	{
		static const FAbilityData Data = { EAbilitySlot::Stun, 3.0f, EAbilityRange::Short,
			EAbilityTargetType::ThrownCircle, ReservedGameplayColours::GetWhite(),
			ReservedGameplayColours::GetWhiteTag(), true };
		return Data;
	}

	const FAbilityData& GetSleep()
	{
		static const FAbilityData Data = { EAbilitySlot::Sleep, 5.0f, EAbilityRange::Long,
			EAbilityTargetType::ThrownCircle, ReservedGameplayColours::GetBlue(),
			ReservedGameplayColours::GetBlueTag(), false, EEnemyType::SN_1PR };
		return Data;
	}

	const FAbilityData& GetRoot()
	{
		static const FAbilityData Data = { EAbilitySlot::Root, 5.0f, EAbilityRange::Long,
			EAbilityTargetType::Line, ReservedGameplayColours::GetTeal(),
			ReservedGameplayColours::GetTealTag(), false, EEnemyType::TR_UPR };
		return Data;
	}

	const FAbilityData& GetFear()
	{
		static const FAbilityData Data = { EAbilitySlot::Fear, 5.0f, EAbilityRange::Short,
			EAbilityTargetType::SelfCircle, ReservedGameplayColours::GetOrange(),
			ReservedGameplayColours::GetOrangeTag(), false, EEnemyType::B0_0MR };
		return Data;
	}

	const FAbilityData& GetSnare()
	{
		static const FAbilityData Data = { EAbilitySlot::Snare, 4.0f, EAbilityRange::Medium,
			EAbilityTargetType::Cone, ReservedGameplayColours::GetPurple(),
			ReservedGameplayColours::GetPurpleTag(), false, EEnemyType::RU_NNR };
		return Data;
	}
}

const FAbilityData& AbilityData::Get(EAbilitySlot Ability)
{
	switch (Ability)
	{
	case EAbilitySlot::Stun:
		return GetStun();
	case EAbilitySlot::Sleep:
		return GetSleep();
	case EAbilitySlot::Root:
		return GetRoot();
	case EAbilitySlot::Fear:
		return GetFear();
	case EAbilitySlot::Snare:
		return GetSnare();
	default:
		// EAbilitySlot::Count is a hidden sentinel, never a valid input - a caller
		// reaching here is a programming error, not a normal runtime case.
		checkNoEntry();
		return GetStun();
	}
}

TArray<FAbilityData> AbilityData::GetAll()
{
	return { GetStun(), GetSleep(), GetRoot(), GetFear(), GetSnare() };
}
