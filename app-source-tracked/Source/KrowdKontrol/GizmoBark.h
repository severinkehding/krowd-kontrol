#pragma once

#include "CoreMinimal.h"
#include "GizmoBark.generated.h"

// One narrative "bark" (PRD 07): a one-sided Gizmo remote-call line delivered exactly
// once and never replayed. See issue #57 - this struct is the plain data definition.
// UGizmoNarrativeSubsystem owns the registry and the trigger-once logic that flips
// bHasBeenTriggered for barks registered with it - but FGizmoBark's shape is also
// reused standalone (without the subsystem or its registry) by classes like
// APlaceholderTerminalActor (issue #62), so do not assume every bHasBeenTriggered
// flip in the codebase routes through this subsystem.
USTRUCT(BlueprintType)
struct FGizmoBark
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gizmo Bark")
	FName BarkID;

	// 2-4 lines of display text (issue #57's acceptance criteria). This is a content
	// guideline, not a runtime-enforced bound - UGizmoNarrativeSubsystem never
	// validates Lines.Num(), it broadcasts whatever's registered.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gizmo Bark")
	TArray<FString> Lines;

	// Prevents replay - flipped by whichever owner triggers this bark (e.g.
	// UGizmoNarrativeSubsystem::TriggerBark, or APlaceholderTerminalActor::Interact()
	// for a standalone instance), never meant to be set directly by a caller.
	UPROPERTY(BlueprintReadOnly, Category = "Gizmo Bark")
	bool bHasBeenTriggered = false;
};
