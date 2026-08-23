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
	SelfCircle, // Fear
	Cone, // Snare
	Line, // Root
	ThrownCircle // Stun, Sleep - distinguished from each other by Range (Short/Long)
};

struct FAbilityData
{
	EAbilitySlot Ability = EAbilitySlot::Stun;
	float BaseDurationSeconds = 0.0f;
	EAbilityRange Range = EAbilityRange::Short;
	EAbilityTargetType TargetType = EAbilityTargetType::SelfCircle;
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

	// True only for Sleep: being hit by any other ability's application while this
	// ability's Controlled window is active ends it immediately (Controlled -> Alert)
	// instead of running its full duration; see AEnemyBase::ReceiveControl's
	// early-wake branch.
	bool bWakesEarlyOnOtherAbilityHit = false;

	// True for Root and Snare: a target Controlled by this ability keeps its own attack
	// behaviour (telegraph/tell/fire) running instead of having it silenced the instant
	// Controlled begins - see AEnemyBase::IsAttackBehaviorActive(). Root's attack runs
	// exactly as it would in Attack (ControlledSpeedMultiplier below stays 1.0f); Snare's
	// runs at the same scaled-down pace as its movement (issue #254).
	bool bAllowsAttackWhileControlled = false;

	// True only for Fear: a target Controlled by this ability actively moves away from
	// the caster's live position for the Controlled duration, instead of standing
	// immobile like every other ability's flavour - see AEnemyBase::TickFleeMovement().
	bool bFleesFromCasterWhileControlled = false;

	// True only for Snare: a target Controlled by this ability keeps closing distance
	// (AEnemyBase::TickChaseMovement) instead of freezing in place, scaled by
	// ControlledSpeedMultiplier below - see AEnemyBase::IsMovementBehaviorActive().
	// Unlike Root above (attack-only, full speed, still stops moving), Snare sets both
	// this flag and bAllowsAttackWhileControlled - it's a slow, not a freeze: the target
	// keeps both moving and attacking, just at half pace.
	bool bAllowsMovementWhileControlled = false;

	// Fraction of GetEffectiveMovementSpeedUnitsPerSecond()/attack-telegraph
	// DeltaSeconds retained while Controlled - consulted only when
	// bAllowsMovementWhileControlled or bAllowsAttackWhileControlled is true (Snare:
	// 0.5f, a 50% slow per issue #254; Root: 1.0f, i.e. its allowed attack runs
	// completely unmodified). 1.0f (full speed) is the safe inert default for every
	// ability that never reaches either flag - see AEnemyBase::GetControlledSpeedMultiplier().
	float ControlledSpeedMultiplier = 1.0f;

	// One-line effect description (issue #260, PRD 13 REQ-2) - the tooltip widget's
	// sole source for ability-flavour text; no other place in the codebase should
	// hardcode a per-ability description.
	FText EffectDescription;

	// Canonical display binding (operator ruling 2026-08-23, PRD 13 REQ-2): the
	// OG-GDD scheme - LMB=Stun, RMB=Sleep, Q=Root, E=Snare, MMB=Fear - not the live
	// 1-5 DefaultInput.ini mappings, which remain functional legacy alternates that
	// are deliberately never surfaced in UI. A full rebinding system is out of scope;
	// this is a static display label, not read from live input config.
	FText KeyBindingLabel;
};

namespace AbilityData
{
	KROWDKONTROL_API const FAbilityData& Get(EAbilitySlot Ability);

	// All 5 abilities' data, in EAbilitySlot declaration order (Stun, Sleep, Root,
	// Fear, Snare).
	KROWDKONTROL_API TArray<FAbilityData> GetAll();

	// Shared ALL-CAPS display strings for HUD prompt-style text (issue #37's
	// AbilityUnlockPromptComponent originated these; issue #249's quest-tracker
	// suggestion line is the second consumer - single source of truth, not a second
	// copy of either TMap).
	KROWDKONTROL_API const FString& GetDisplayName(EAbilitySlot Ability);
	KROWDKONTROL_API const FString& GetEnemyPluralDisplayName(EEnemyType EnemyType);
}
