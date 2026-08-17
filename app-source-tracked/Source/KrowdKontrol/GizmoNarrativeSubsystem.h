#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GizmoBark.h"
#include "GizmoNarrativeSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBarkTriggered, FName, BarkID, TArray<FString>, Lines);

// Reusable trigger point for PRD 07's one-sided Gizmo remote-call barks (issue #57).
// Foundation + placeholder milestone content only - no HUD wiring, no real
// gameplay-event wiring here (RegisterPlaceholderMilestoneBarks() below owns the
// placeholder bark text; nothing calls TriggerBarkForMilestone from a live level yet).
// See issue #57's Notes and the follow-up issues (#59/#61/#62) that build on top.
UCLASS()
class KROWDKONTROL_API UGizmoNarrativeSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// Registers the placeholder milestone barks once the subsystem is fully live.
	// Super::Initialize() must run first (engine subsystem-init convention).
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// Registers (or overwrites) a bark definition. A future bark-content issue calls
	// this once per bark at startup; nothing in this issue calls it from a live path.
	// Note: re-registering an already-fired BarkID resets bHasBeenTriggered to false,
	// since overwrite is a plain TMap::Add - no merge-in of prior fired state.
	UFUNCTION(BlueprintCallable, Category = "Gizmo Narrative")
	void RegisterBark(const FGizmoBark& BarkDefinition);

	// Triggers a registered bark by ID. Unknown IDs log a warning and no-op (never
	// crash). Already-fired IDs silently no-op - the "one-sided, no replay" delivery
	// model. Otherwise broadcasts OnBarkTriggered exactly once.
	UFUNCTION(BlueprintCallable, Category = "Gizmo Narrative")
	void TriggerBark(FName BarkID);

	// True if BarkID is registered and has already fired. False for an unregistered
	// ID (matches TriggerBark's own no-op treatment of unknown IDs).
	bool HasBarkFired(FName BarkID) const;

	// Registers the six placeholder story-beat barks this issue adds (issue #61,
	// PRD 07 REQ-3/REQ-5): the five narrative-arc milestones (Meet Krowd, Saving
	// Fellow Robots, Asleep for a Long Time, Hidden Enemy Revealed, Final Chapter)
	// plus a sixth, distinct entry for the Krowd age-reveal beat. Called automatically
	// from Initialize() for real engine-booted instances; exposed publicly (and made
	// idempotent, mirroring URoomEnemyBudgetController::InitializeRoom()'s rationale
	// in full) so the Automation Framework test can drive it deterministically without
	// relying on the engine's subsystem-collection lifecycle, which bare NewObject<>()
	// construction (as this class's own test uses) never invokes.
	UFUNCTION(BlueprintCallable, Category = "Gizmo Narrative")
	void RegisterPlaceholderMilestoneBarks();

	// Milestone-tag entry point for level/progression code (issue #61). MilestoneTag
	// IS the registered BarkID - this is a direct forward to TriggerBark(), so the
	// once-only-fire/unknown-tag-no-op-with-warning guarantees documented on
	// TriggerBark() above apply here unmodified. No real caller exists yet - the
	// room-pool/level-progression system this is meant for (PRD 05) is not yet built;
	// only this class's own unit test calls it directly today.
	UFUNCTION(BlueprintCallable, Category = "Gizmo Narrative")
	void TriggerBarkForMilestone(FName MilestoneTag);

	UPROPERTY(BlueprintAssignable, Category = "Gizmo Narrative")
	FOnBarkTriggered OnBarkTriggered;

private:
	UPROPERTY()
	TMap<FName, FGizmoBark> RegisteredBarks;

	bool bHasRegisteredPlaceholderMilestoneBarks = false;
};
