#include "AbilityData.h"
#include "ReservedGameplayColours.h"

namespace
{
	const FAbilityData& GetStun()
	{
		static const FAbilityData Data = {
			.Ability = EAbilitySlot::Stun,
			.BaseDurationSeconds = 3.0f,
			.Range = EAbilityRange::Short,
			.TargetType = EAbilityTargetType::ThrownCircle,
			.Colour = ReservedGameplayColours::GetWhite(),
			.ColourTag = ReservedGameplayColours::GetWhiteTag(),
			.bIsColourNeutral = true,
			.CounteredEnemyType = EEnemyType::RU_NNR,
			.bWakesEarlyOnOtherAbilityHit = false,
			.bAllowsAttackWhileControlled = false,
			.bFleesFromCasterWhileControlled = false,
			.bAllowsMovementWhileControlled = false,
			.ControlledSpeedMultiplier = 1.0f,
			.EffectDescription = NSLOCTEXT("AbilityData", "StunEffectDescription", "Stuns the target briefly; works on any enemy, no colour match needed."),
			.KeyBindingLabel = NSLOCTEXT("AbilityData", "StunKeyBinding", "LMB"),
		};
		return Data;
	}

	const FAbilityData& GetSleep()
	{
		static const FAbilityData Data = {
			.Ability = EAbilitySlot::Sleep,
			.BaseDurationSeconds = 5.0f,
			.Range = EAbilityRange::Long,
			.TargetType = EAbilityTargetType::ThrownCircle,
			.Colour = ReservedGameplayColours::GetBlue(),
			.ColourTag = ReservedGameplayColours::GetBlueTag(),
			.bIsColourNeutral = false,
			.CounteredEnemyType = EEnemyType::SN_1PR,
			.bWakesEarlyOnOtherAbilityHit = true,
			.bAllowsAttackWhileControlled = false,
			.bFleesFromCasterWhileControlled = false,
			.bAllowsMovementWhileControlled = false,
			.ControlledSpeedMultiplier = 1.0f,
			.EffectDescription = NSLOCTEXT("AbilityData", "SleepEffectDescription", "Puts the target to sleep; wakes early if hit by another ability."),
			.KeyBindingLabel = NSLOCTEXT("AbilityData", "SleepKeyBinding", "RMB"),
		};
		return Data;
	}

	const FAbilityData& GetRoot()
	{
		static const FAbilityData Data = {
			.Ability = EAbilitySlot::Root,
			.BaseDurationSeconds = 5.0f,
			.Range = EAbilityRange::Long,
			.TargetType = EAbilityTargetType::Line,
			.Colour = ReservedGameplayColours::GetTeal(),
			.ColourTag = ReservedGameplayColours::GetTealTag(),
			.bIsColourNeutral = false,
			.CounteredEnemyType = EEnemyType::TR_UPR,
			.bWakesEarlyOnOtherAbilityHit = false,
			.bAllowsAttackWhileControlled = true,
			.bFleesFromCasterWhileControlled = false,
			.bAllowsMovementWhileControlled = false,
			.ControlledSpeedMultiplier = 1.0f,
			.EffectDescription = NSLOCTEXT("AbilityData", "RootEffectDescription", "Roots a line of targets, controlling them; they can still attack while rooted and will trail you like any Controlled target (issue #214)."),
			.KeyBindingLabel = NSLOCTEXT("AbilityData", "RootKeyBinding", "Q"),
		};
		return Data;
	}

	const FAbilityData& GetFear()
	{
		static const FAbilityData Data = {
			.Ability = EAbilitySlot::Fear,
			.BaseDurationSeconds = 5.0f,
			.Range = EAbilityRange::Short,
			.TargetType = EAbilityTargetType::SelfCircle,
			.Colour = ReservedGameplayColours::GetOrange(),
			.ColourTag = ReservedGameplayColours::GetOrangeTag(),
			.bIsColourNeutral = false,
			.CounteredEnemyType = EEnemyType::B0_0MR,
			.bWakesEarlyOnOtherAbilityHit = false,
			.bAllowsAttackWhileControlled = false,
			.bFleesFromCasterWhileControlled = true,
			.bAllowsMovementWhileControlled = false,
			.ControlledSpeedMultiplier = 1.0f,
			.EffectDescription = NSLOCTEXT("AbilityData", "FearEffectDescription", "Frightens nearby targets into fleeing from you."),
			.KeyBindingLabel = NSLOCTEXT("AbilityData", "FearKeyBinding", "MMB"),
		};
		return Data;
	}

	const FAbilityData& GetSnare()
	{
		static const FAbilityData Data = {
			.Ability = EAbilitySlot::Snare,
			.BaseDurationSeconds = 4.0f,
			.Range = EAbilityRange::Medium,
			.TargetType = EAbilityTargetType::Cone,
			.Colour = ReservedGameplayColours::GetPurple(),
			.ColourTag = ReservedGameplayColours::GetPurpleTag(),
			.bIsColourNeutral = false,
			.CounteredEnemyType = EEnemyType::RU_NNR,
			.bWakesEarlyOnOtherAbilityHit = false,
			.bAllowsAttackWhileControlled = true,
			.bFleesFromCasterWhileControlled = false,
			.bAllowsMovementWhileControlled = true,
			// The only non-1.0f ControlledSpeedMultiplier today, and Snare is also the
			// one ability TickFollowMovement's gate excludes - so no reachable ability
			// data lets TickFollowMovement's movement branch run with a non-1.0f
			// multiplier (issue #214 review follow-up). Untested-by-necessity, not
			// untested-by-oversight; revisit if a future ability gets a non-1.0f
			// multiplier while still reaching TickFollowMovement.
			.ControlledSpeedMultiplier = 0.5f,
			.EffectDescription = NSLOCTEXT("AbilityData", "SnareEffectDescription", "Slows a cone of targets to half speed; they can still move and attack."),
			.KeyBindingLabel = NSLOCTEXT("AbilityData", "SnareKeyBinding", "E"),
		};
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

const FString& AbilityData::GetDisplayName(EAbilitySlot Ability)
{
	static const TMap<EAbilitySlot, FString> AbilityDisplayNames = {
		{ EAbilitySlot::Stun, TEXT("STUN") },
		{ EAbilitySlot::Sleep, TEXT("SLEEP") },
		{ EAbilitySlot::Root, TEXT("ROOT") },
		{ EAbilitySlot::Fear, TEXT("FEAR") },
		{ EAbilitySlot::Snare, TEXT("SNARE") },
	};
	return AbilityDisplayNames.FindChecked(Ability);
}

const FString& AbilityData::GetEnemyPluralDisplayName(EEnemyType EnemyType)
{
	static const TMap<EEnemyType, FString> EnemyPluralDisplayNames = {
		{ EEnemyType::RU_NNR, TEXT("RUNNERS") },
		{ EEnemyType::TR_UPR, TEXT("TROOPERS") },
		{ EEnemyType::B0_0MR, TEXT("BOMBERS") },
		{ EEnemyType::SN_1PR, TEXT("SNIPERS") },
	};
	return EnemyPluralDisplayNames.FindChecked(EnemyType);
}

FLinearColor AbilityData::GetChainColourForEnemyType(EEnemyType EnemyType)
{
	for (const FAbilityData& Data : GetAll())
	{
		if (!Data.bIsColourNeutral && Data.CounteredEnemyType == EnemyType)
		{
			return Data.Colour;
		}
	}
	// Every EEnemyType has exactly one colour-matched, non-neutral ability - the 4
	// locked enemy types (EnemyType.h) and the 4 non-Stun abilities' CounteredEnemyType
	// values are a 1:1 mapping (see KrowdKontrolAbilityColourMatchTest.cpp). Reaching
	// here is a programming error, not a normal runtime case - same idiom as
	// AbilityData::Get's own checkNoEntry() below.
	checkNoEntry();
	return ReservedGameplayColours::GetWhite();
}
