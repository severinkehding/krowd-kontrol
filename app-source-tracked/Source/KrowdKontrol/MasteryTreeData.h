#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "MasteryTreeData.generated.h"

// docs/prd-mastery-skill-tree.md REQ-1, issue #370: the P-Organ-style skill tree's
// data schema. Mirrors LevelSequenceData.h/LevelBriefingData.h's established "one
// row per DataTable-authored content item, never hardcoded per-node C++" shape.
// This issue is data-only - no spend/refund logic and no UI read this yet; a later
// issue extends UCrowdMasteryTotalSubsystem to consume it (see PRD REQ-1's second
// half), and the separate P1 modifier-catalog issue resolves EffectHookId below
// into real gameplay effects.

// Which progression phase a node belongs to (PRD "Operator design decision": nodes
// are arranged across progression phases; higher tree phases also gate which
// modifier tiers are usable per REQ-4, though modifier tiers themselves are a
// separate P1 concern). Count is a sentinel, not a real phase - hidden so it never
// shows up in a Blueprint dropdown, same idiom as EAbilitySlot::Count.
UENUM(BlueprintType)
enum class EMasteryTreePhase : uint8
{
	Phase1,
	Phase2,
	Phase3,

	Count UMETA(Hidden)
};

// One of a node's 4 skill bubbles (PRD: "around each circle sit 4 smaller bubbles.
// Clicking a bubble spends mastery points to unlock that skill (one bubble = one
// skill)"). Embedded in FMasteryTreeNode::Bubbles below, not itself a DataTable row
// - a bubble has no independent existence outside its owning node.
USTRUCT(BlueprintType)
struct FMasterySkillBubble
{
	GENERATED_BODY()

	// Stable identifier for this bubble, unique across the whole tree - a later
	// issue's spend/unlock tracking and save-data key off this, not the row-relative
	// array index (so re-ordering bubbles in the editor can never silently re-target
	// an already-unlocked player's save data).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mastery Tree")
	FName BubbleId;

	// Player-facing skill name, rendered by the (separate, later) tree-screen UI.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mastery Tree")
	FText DisplayName;

	// Mastery points required to unlock this bubble (PRD: "Clicking a bubble spends
	// mastery points to unlock that skill").
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mastery Tree")
	int32 PointCost = 0;

	// Deferred effect identifier - intentionally not yet an enum of real gameplay
	// effects. The separate P1 modifier-catalog issue and the later spend-logic issue
	// resolve this into an actual effect (PRD REQ-3's starter examples: ability
	// cooldown reduction, Controlled-duration bonus, energy max increase, pen-zone
	// radius bonus, movement speed). Kept as a bare FName here so this issue never
	// has to guess at that later issue's real effect taxonomy.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mastery Tree")
	FName EffectHookId;
};

// One DataTable row per tree node (the DataTable's RowName is the node's own
// identifier, referenced by other nodes' ParentNodeId below - same "RowName IS the
// identifier" convention FLevelSequenceRow uses for map names). Authored with
// exactly 4 entries in Bubbles per the PRD's "4 smaller bubbles" per node - not
// enforced in code, since nothing consumes this data yet to enforce it against.
USTRUCT(BlueprintType)
struct FMasteryTreeNode : public FTableRowBase
{
	GENERATED_BODY()

	// The prerequisite node's RowName - a later spend-logic issue only allows
	// unlocking a bubble on this node once the parent node has at least one unlocked
	// bubble (issue #370: "unlock prerequisites (parent node must be reached)"). NAME_None
	// marks a root node with no prerequisite, mirroring FLevelSequenceRow::
	// NextLevelMapName's identical NAME_None-as-sentinel idiom.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mastery Tree")
	FName ParentNodeId;

	// Which progression phase this node belongs to (PRD's phase/tier gating).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mastery Tree")
	EMasteryTreePhase Phase = EMasteryTreePhase::Phase1;

	// This node's 4 skill bubbles, in display order.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mastery Tree")
	TArray<FMasterySkillBubble> Bubbles;
};
