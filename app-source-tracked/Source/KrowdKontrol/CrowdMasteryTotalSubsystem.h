#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CrowdMasteryTotalSubsystem.generated.h"

class ULevelClearTimeSaveGame;
class UDataTable;
struct FMasteryTreeNode;
struct FMasterySkillBubble;

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

	// Full respec: zeroes SpentPoints and clears UnlockedBubbleIds. Never touches
	// AccumulatedTotal - the earned total stays the separate, already-existing
	// authority this respec doesn't affect.
	UFUNCTION(BlueprintCallable, Category = "Crowd Mastery")
	void RefundAllAndClearUnlocks();

	// The total currently spent across all unlocked bubbles.
	UFUNCTION(BlueprintPure, Category = "Crowd Mastery")
	int32 GetSpentPoints() const { return SpentPoints; }

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

	int32 AccumulatedTotal = 0;
	int32 SpentPoints = 0;
	TSet<FName> UnlockedBubbleIds;

	// Warn-once guard so a missing/unassigned MasteryTreeTable logs a single
	// diagnostic instead of spamming on every guarded call - same pattern as
	// ULevelBriefingSubsystem::bHasWarnedMissingBriefingTable.
	mutable bool bHasWarnedMissingMasteryTreeTable = false;
};
