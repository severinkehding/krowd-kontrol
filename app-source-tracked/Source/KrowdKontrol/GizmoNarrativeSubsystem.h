#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GizmoBark.h"
#include "GizmoNarrativeSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBarkTriggered, FName, BarkID, TArray<FString>, Lines);

// Reusable trigger point for PRD 07's one-sided Gizmo remote-call barks (issue #57).
// Foundation only - no bark content, no HUD wiring, no gameplay-event wiring here;
// see issue #57's Notes and the follow-up issues (#59/#61/#62) that build on top.
UCLASS()
class KROWDKONTROL_API UGizmoNarrativeSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
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

	UPROPERTY(BlueprintAssignable, Category = "Gizmo Narrative")
	FOnBarkTriggered OnBarkTriggered;

private:
	UPROPERTY()
	TMap<FName, FGizmoBark> RegisteredBarks;
};
