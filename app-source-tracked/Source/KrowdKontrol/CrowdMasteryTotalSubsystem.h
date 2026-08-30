#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CrowdMasteryTotalSubsystem.generated.h"

class ULevelClearTimeSaveGame;
class UDataTable;
struct FMasteryTreeNode;
struct FMasterySkillBubble;
struct FMasteryModifierRow;

// docs/prd-crowd-mastery-persistence.md REQ-1, issue #327: sole authority for the
// Crowd Mastery total accumulated across every run cleared this play session.
// GameInstance-scoped (not world-scoped like UCrowdMasterySubsystem, which tracks
// only the current level's running peak) so the total survives level transitions,
// level reruns, and returns to the main menu within a single launch - the same
// "GameInstance owns the cross-level total" precedent ULevelClearTimeSubsystem
// establishes for personal-best clear times.
//
// Public API deliberately never calls GetWorld() or GetGameInstance() - mirrors
// ULevelClearTimeSubsystem's own rationale (see that class's
// SubscribeToLevelLifecycle() doc comment), so this subsystem stays directly
// NewObject<>()-testable with no UWorld/CreateNewMap() dependency. The
// real deposit call comes from UCrowdMasterySubsystem::HandleLevelClear(), which
// legitimately resolves this world's GameInstance since it is itself a
// UWorldSubsystem. Initialize()/LoadPersistedTotal()/PersistAccumulatedTotal()
// (PRD "Crowd Mastery Persistence" REQ-4, issue #330) only ever call slot-based
// UGameplayStatics functions, never GetWorld() or GetGameInstance(), so this rule
// still holds after cross-launch persistence was added.
//
// docs/prd-mastery-skill-tree.md REQ-1, issue #371: also the runtime authority for
// spending the accumulated total against the skill-tree data model (issue #370,
// MasteryTreeData.h). SpentPoints/UnlockedBubbleIds below are session-only in this
// issue - no persistence yet, that is a separate follow-up issue - and none of this
// new code violates the GetWorld()/GetGameInstance() rule above (MasteryTreeTable
// loads via LoadObject(), never GetWorld()).
UCLASS()
class KROWDKONTROL_API UCrowdMasteryTotalSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// Adds RunMasteryValue to the accumulated total and persists the new total to disk
	// (PRD "Crowd Mastery Persistence" REQ-4, issue #330). Negative input is clamped to
	// 0, same clamp-to-0 idiom ULevelClearTimeSubsystem::RecordCrowdMasteryCount uses
	// for the analogous per-level stat - never subtracted from the running total.
	UFUNCTION(BlueprintCallable, Category = "Crowd Mastery")
	void DepositRunMastery(int32 RunMasteryValue);

	// The accumulated Crowd Mastery total for this GameInstance's play session so far.
	UFUNCTION(BlueprintPure, Category = "Crowd Mastery")
	int32 GetAccumulatedTotal() const { return AccumulatedTotal; }

	// Zeroes the accumulated total and persists the reset to disk (PRD "Crowd Mastery
	// Persistence" REQ-4, issue #330), so a relaunch doesn't resurrect the pre-reset
	// value. REQ-3's reset control calls this.
	UFUNCTION(BlueprintCallable, Category = "Crowd Mastery")
	void ResetAccumulatedTotal();

	// Reads the persisted accumulated total from the shared save slot into
	// AccumulatedTotal (PRD "Crowd Mastery Persistence" REQ-4, issue #330). Called from
	// Initialize() at real GameInstance startup; public (not private, not called only
	// from Initialize()) so the Automation Framework test can drive it directly against
	// a bare NewObject<>()-constructed instance without a live FSubsystemCollectionBase.
	UFUNCTION(BlueprintCallable, Category = "Crowd Mastery")
	void LoadPersistedTotal();

	// Content asset: one row per skill-tree node (issue #370's MasteryTreeTable
	// family). EditDefaultsOnly for the same never-exercised-by-real-UGameInstance
	// Details-panel case ULevelSequenceSubsystem::LevelSequenceTable documents;
	// Initialize() below auto-loads /Game/Data/DT_MasteryTreeTable for real game
	// worlds. Public, so Automation tests inject an in-code NewObject<UDataTable>()
	// directly, no friendship needed.
	UPROPERTY(EditDefaultsOnly, Category = "Crowd Mastery")
	TObjectPtr<UDataTable> MasteryTreeTable;

	// Attempts to spend this bubble's PointCost against the available balance
	// (GetAccumulatedTotal() - GetSpentPoints()). Fails and leaves all state
	// unchanged if BubbleId is unknown, already unlocked, its prerequisite (owning
	// node's parent reached) is not met, or the available balance is insufficient.
	// Returns true only on an actual spend.
	UFUNCTION(BlueprintCallable, Category = "Crowd Mastery")
	bool TrySpendOnBubble(FName BubbleId);

	// True if BubbleId's owning node has no prerequisite (root node, ParentNodeId ==
	// NAME_None) or its parent node has already been reached (MasteryTreeData.h's
	// documented rule: at least one of the parent's bubbles is unlocked). False if
	// BubbleId is unknown.
	UFUNCTION(BlueprintPure, Category = "Crowd Mastery")
	bool IsPrerequisiteMet(FName BubbleId) const;

	// Every currently-unlocked bubble ID.
	UFUNCTION(BlueprintPure, Category = "Crowd Mastery")
	TArray<FName> GetUnlockedBubbles() const;

	// Every currently-unlocked bubble's EffectHookId, in no particular order. A bubble
	// whose owning node can no longer be resolved (MasteryTreeTable reassigned/edited
	// since unlock) is silently skipped, same fail-closed posture FindBubbleAndOwningNode
	// already documents.
	UFUNCTION(BlueprintPure, Category = "Crowd Mastery")
	TArray<FName> GetUnlockedEffectHookIds() const;

	// Full respec: zeroes SpentPoints and clears UnlockedBubbleIds. Never touches
	// AccumulatedTotal - the earned total stays the separate, already-existing
	// authority this respec doesn't affect.
	UFUNCTION(BlueprintCallable, Category = "Crowd Mastery")
	void RefundAllAndClearUnlocks();

	// The total currently spent across all unlocked bubbles.
	UFUNCTION(BlueprintPure, Category = "Crowd Mastery")
	int32 GetSpentPoints() const { return SpentPoints; }

	// Content asset: one row per modifier (issue #376's ModifierCatalogTable
	// family). Same EditDefaultsOnly / auto-load-in-Initialize()-for-real-worlds
	// shape MasteryTreeTable above already establishes. Public, so Automation tests
	// inject an in-code NewObject<UDataTable>() directly, no friendship needed.
	UPROPERTY(EditDefaultsOnly, Category = "Crowd Mastery")
	TObjectPtr<UDataTable> ModifierCatalogTable;

	// Adds ModifierId to the owned-modifiers inventory. Fails and leaves state
	// unchanged if ModifierId is unknown in ModifierCatalogTable or already owned.
	// Returns true only on an actual grant. Not called from anywhere in this issue
	// - the real earn trigger (level-clear acquisition) is a separate, later issue;
	// this is the minimal primitive that issue will call.
	UFUNCTION(BlueprintCallable, Category = "Crowd Mastery")
	bool GrantModifier(FName ModifierId);

	// Every currently-owned modifier ID.
	UFUNCTION(BlueprintPure, Category = "Crowd Mastery")
	TArray<FName> GetOwnedModifiers() const;

	// Attempts to slot ModifierId into one of BubbleId's up to MaxModifierSlotsPerBubble
	// modifier slots. Fails and leaves all state unchanged if: BubbleId is not
	// unlocked; ModifierId is unknown or not owned; or no open slot's pre-assigned
	// accepted category (FMasterySkillBubble::SlotAcceptedCategories, indexed by slot
	// position) matches ModifierId's Category - this covers both "already full" (no
	// open slot at all) and "category mismatch" (an open slot exists but its accepted
	// category differs) in one check. Returns true only on an actual slot.
	UFUNCTION(BlueprintCallable, Category = "Crowd Mastery")
	bool TrySlotModifier(FName BubbleId, FName ModifierId);

	// Removes ModifierId from BubbleId's slotted modifiers, if present. Returns
	// true only if an entry was actually removed.
	UFUNCTION(BlueprintCallable, Category = "Crowd Mastery")
	bool UnslotModifier(FName BubbleId, FName ModifierId);

	// BubbleId's currently slotted modifier IDs (0-2 entries). Empty if BubbleId
	// has no slotted modifiers or is unknown.
	UFUNCTION(BlueprintPure, Category = "Crowd Mastery")
	TArray<FName> GetSlottedModifiers(FName BubbleId) const;

private:
	ULevelClearTimeSaveGame* LoadOrCreateSaveGame() const;
	void PersistAccumulatedTotal() const;

	// Scans MasteryTreeTable's row map for the node owning BubbleId. Returns false
	// (OutNode/OutBubble left null) if MasteryTreeTable is unset or BubbleId is not
	// found in any row.
	bool FindBubbleAndOwningNode(FName BubbleId, const FMasteryTreeNode*& OutNode, const FMasterySkillBubble*& OutBubble) const;

	// True if NodeRowName's row has at least one bubble in UnlockedBubbleIds. False
	// if MasteryTreeTable is unset or NodeRowName does not resolve to a row.
	bool IsNodeReached(FName NodeRowName) const;

	// Same rule as IsPrerequisiteMet(FName), for callers (TrySpendOnBubble) that
	// have already resolved Node via FindBubbleAndOwningNode and shouldn't re-scan
	// MasteryTreeTable to re-derive it.
	bool IsPrerequisiteMetForNode(const FMasteryTreeNode& Node) const;

	// True if MasteryTreeTable is assigned; false otherwise, logging the
	// missing-table warning exactly once via bHasWarnedMissingMasteryTreeTable.
	// Shared by every guarded entry point (FindBubbleAndOwningNode, IsNodeReached)
	// so the warn-once check and message live in one place.
	bool HasMasteryTreeTable() const;

	// Looks up ModifierId in ModifierCatalogTable's row map. Returns nullptr if
	// ModifierCatalogTable is unset or ModifierId is not found - same fail-closed
	// shape FindBubbleAndOwningNode already establishes for the tree table.
	const FMasteryModifierRow* FindModifierRow(FName ModifierId) const;

	// True if ModifierCatalogTable is assigned; false otherwise, logging the
	// missing-table warning exactly once via bHasWarnedMissingModifierCatalogTable.
	// Same warn-once shape HasMasteryTreeTable() already establishes.
	bool HasModifierCatalogTable() const;

	int32 AccumulatedTotal = 0;
	int32 SpentPoints = 0;
	TSet<FName> UnlockedBubbleIds;

	// Warn-once guard so a missing/unassigned MasteryTreeTable logs a single
	// diagnostic instead of spamming on every guarded call - same pattern as
	// ULevelBriefingSubsystem::bHasWarnedMissingBriefingTable.
	mutable bool bHasWarnedMissingMasteryTreeTable = false;

	// Every modifier ID the player owns, regardless of whether it is currently
	// slotted anywhere. Survives RefundAllAndClearUnlocks() (PRD REQ-5: respec
	// clears unlocks and slotted modifiers, not earned inventory).
	TSet<FName> OwnedModifierIds;

	// Per unlocked bubble, exactly MaxModifierSlotsPerBubble entries once the bubble
	// has ever been touched by TrySlotModifier, indexed by slot position (matching
	// FMasterySkillBubble::SlotAcceptedCategories's indexing) - NAME_None marks an
	// open slot. GetSlottedModifiers() filters NAME_None out before returning, so
	// callers still see a 0-2-entry, gap-free list. Cleared by
	// RefundAllAndClearUnlocks() alongside UnlockedBubbleIds.
	TMap<FName, TArray<FName>> SlottedModifiersByBubbleId;

	// Warn-once guard for ModifierCatalogTable, same pattern as
	// bHasWarnedMissingMasteryTreeTable above.
	mutable bool bHasWarnedMissingModifierCatalogTable = false;

	// Slots per unlocked skill bubble (PRD REQ-4: "Each unlocked skill has 2
	// modifier slots"). Also the fixed size TrySlotModifier initializes each
	// bubble's SlottedModifiersByBubbleId entry to, and the bound
	// FMasterySkillBubble::SlotAcceptedCategories is indexed against.
	static constexpr int32 MaxModifierSlotsPerBubble = 2;
};
