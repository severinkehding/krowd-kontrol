#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ModifierData.generated.h"

// docs/prd-mastery-skill-tree.md REQ-4, issue #376: the modifier catalog a player
// slots (up to 2 per unlocked skill bubble, see UCrowdMasteryTotalSubsystem's
// TrySlotModifier/UnslotModifier). Mirrors MasteryTreeData.h's established "one
// row per DataTable-authored content item, never hardcoded per-modifier C++" shape.
// This issue is data/API-only - no UI reads this yet, and EffectHookId below is not
// yet resolved into a real gameplay effect (same deferred-resolution posture
// FMasterySkillBubble::EffectHookId already documents).

// Which of a skill's slots a modifier competes for anti-duplication against
// (UCrowdMasteryTotalSubsystem::TrySlotModifier rejects slotting two modifiers of
// the same Category onto the same bubble - see that function's doc comment for why
// this is an anti-duplication rule, not a fixed-per-slot-accepted-category rule).
// Four categories, not the PRD's three named examples (Survival/Attack/Ability) -
// ItemType is included now per the cited reference source so the enum doesn't need
// a breaking change once a later issue needs it. Count is a sentinel, not a real
// category - hidden so it never shows up in a Blueprint dropdown, same idiom as
// EMasteryTreePhase::Count.
UENUM(BlueprintType)
enum class EModifierCategory : uint8
{
	AttackType,
	SurvivalType,
	AbilityType,
	ItemType,

	Count UMETA(Hidden)
};

// Which progression tier a modifier belongs to (PRD REQ-4: higher tree phases gate
// which modifier tiers are usable). Stored and exposed by this issue but
// intentionally never gate-enforced here - that is a separate, later tier-gating
// issue's job (see ModifierData.h's issue #376 header comment above).
UENUM(BlueprintType)
enum class EModifierTier : uint8
{
	TierI,
	TierII,
	TierIII,

	Count UMETA(Hidden)
};

// One DataTable row per modifier (the DataTable's RowName is the modifier's own
// identifier, referenced elsewhere as a bare FName - same "RowName IS the
// identifier" convention FMasteryTreeNode uses for tree nodes).
USTRUCT(BlueprintType)
struct FMasteryModifierRow : public FTableRowBase
{
	GENERATED_BODY()

	// Which anti-duplication bucket this modifier occupies among a skill's own
	// slotted modifiers (see EModifierCategory above).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crowd Mastery")
	EModifierCategory Category = EModifierCategory::AttackType;

	// Progression tier this modifier belongs to - stored/exposed only, not
	// gate-enforced by this issue (see EModifierTier above).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crowd Mastery")
	EModifierTier Tier = EModifierTier::TierI;

	// Player-facing modifier name, rendered by the (separate, later) slotting UI.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crowd Mastery")
	FText DisplayName;

	// Deferred effect identifier - intentionally not yet an enum of real gameplay
	// effects, same posture FMasterySkillBubble::EffectHookId already documents. A
	// later issue resolves this into an actual effect once slotted modifiers are
	// wired into gameplay.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crowd Mastery")
	FName EffectHookId;
};
