#pragma once

#include "CoreMinimal.h"
#include "AbilitySlot.h"

// Single source of truth for PRD 02 REQ-3 / issue #63: the 5 crowd-control abilities'
// locked base stats (duration, range category, target type, colour). These are locked
// GDD design values, not per-instance tunables - contrast with
// UAbilityCooldownComponent's AbilityCooldownDurations, which is deliberately
// EditDefaultsOnly because cooldown balance is still an open playtesting question.
// Colour always comes from ReservedGameplayColours::Get*() (MISSION.md Hard Invariant
// 3), never a local FLinearColor literal, so there is exactly one place a future
// art-direction ruling on RGB values has to change.
//
// No UENUM/USTRUCT/.generated.h - same rationale as ReservedGameplayColours.h: a
// plain namespace/struct doesn't need UHT reflection or Blueprint visibility, and
// nothing has asked for it yet. Revisit if a future issue needs Blueprint access.

enum class EAbilityRange : uint8
{
	Short,
	Medium,
	Long
};

enum class EAbilityTargetType : uint8
{
	Single,
	Area,
	Cone
};

struct FAbilityData
{
	EAbilitySlot Ability = EAbilitySlot::Stun;
	float BaseDurationSeconds = 0.0f;
	EAbilityRange Range = EAbilityRange::Short;
	EAbilityTargetType TargetType = EAbilityTargetType::Single;
	FLinearColor Colour = FLinearColor::Black;

	// True only for Stun (MISSION.md Hard Invariant 4: Stun has no countered-enemy
	// colour matchup). No enemy-type field exists here or anywhere else in the
	// codebase - adding one would itself be the enemy-targeting logic this issue's
	// scope explicitly excludes.
	bool bIsColourNeutral = false;
};

namespace AbilityData
{
	KROWDKONTROL_API const FAbilityData& Get(EAbilitySlot Ability);

	// All 5 abilities' data, in EAbilitySlot declaration order (Stun, Sleep, Root,
	// Fear, Snare).
	KROWDKONTROL_API TArray<FAbilityData> GetAll();
}
