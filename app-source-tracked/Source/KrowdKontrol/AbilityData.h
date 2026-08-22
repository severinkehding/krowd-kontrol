#pragma once

#include "CoreMinimal.h"
#include "AbilitySlot.h"
#include "EnemyType.h"

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

	// FName counterpart to Colour above - issue #211's IHerdable::GetHerdColourTag()
	// derivation reads this instead of re-deriving a second switch statement, same
	// "one source of truth" rationale Colour's own comment documents for FLinearColor.
	FName ColourTag = NAME_None;

	// True only for Stun (MISSION.md Hard Invariant 4: Stun has no countered-enemy
	// colour matchup). See CounteredEnemyType below for the actual countered-enemy
	// value the other 4 abilities carry.
	bool bIsColourNeutral = false;

	// The EEnemyType this ability's colour is matched against (MISSION.md Hard
	// Invariant 3's locked 5-colour channel). Only meaningful when
	// !bIsColourNeutral - Stun's value is unused/arbitrary, since MISSION.md Hard
	// Invariant 4 forbids giving Stun a real counter.
	EEnemyType CounteredEnemyType = EEnemyType::RU_NNR;
};

namespace AbilityData
{
	KROWDKONTROL_API const FAbilityData& Get(EAbilitySlot Ability);

	// All 5 abilities' data, in EAbilitySlot declaration order (Stun, Sleep, Root,
	// Fear, Snare).
	KROWDKONTROL_API TArray<FAbilityData> GetAll();
}
