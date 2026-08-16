#pragma once

#include "CoreMinimal.h"
#include "ReentrantTerminalListener.generated.h"

class APlaceholderTerminalActor;

// Test-only listener for APlaceholderTerminalActor::OnTerminalLogRevealed, scoped to
// proving Interact()'s flip-before-broadcast ordering is re-entrancy safe. Mirrors
// UGizmoBarkTestListener's SubsystemToReenter mechanism (GizmoBarkTestListener.h),
// but typed to APlaceholderTerminalActor instead of UGizmoNarrativeSubsystem - the
// two aren't interchangeable, so this is a small additive listener rather than a
// widened shared one. See KrowdKontrolPlaceholderTerminalActorTest.cpp.
UCLASS()
class UReentrantTerminalListener : public UObject
{
	GENERATED_BODY()

public:
	int32 CallCount = 0;

	// When set, HandleBarkTriggered calls ActorToReenter->Interact() from inside the
	// broadcast - lets a test prove the no-replay guard is safe against a listener
	// that re-enters Interact() on the same actor while it's still firing.
	UPROPERTY()
	TObjectPtr<APlaceholderTerminalActor> ActorToReenter = nullptr;

	UFUNCTION()
	void HandleBarkTriggered(FName BarkID, TArray<FString> Lines);
};
