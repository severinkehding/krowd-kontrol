#pragma once

#include "CoreMinimal.h"
#include "GizmoBarkTestListener.generated.h"

class UGizmoNarrativeSubsystem;

// Generic listener for FOnBarkTriggered-shaped delegates (used by
// UGizmoNarrativeSubsystem::OnBarkTriggered and any actor that reuses that
// delegate signature, e.g. APlaceholderTerminalActor::OnTerminalLogRevealed).
// Dynamic multicast delegates (DECLARE_DYNAMIC_MULTICAST_DELEGATE) only bind to
// UFUNCTIONs via AddDynamic - no AddLambda - so counting/inspecting broadcasts in
// tests needs this rather than a capturing lambda.
UCLASS()
class UGizmoBarkTestListener : public UObject
{
	GENERATED_BODY()

public:
	int32 CallCount = 0;
	FName LastBarkID;
	TArray<FString> LastLines;

	// When set, HandleBarkTriggered calls SubsystemToReenter->TriggerBark(ReentrantBarkID)
	// from inside the broadcast - lets a test prove the no-replay guard is safe against a
	// listener that re-enters TriggerBark on the same ID while it's still firing.
	UPROPERTY()
	TObjectPtr<UGizmoNarrativeSubsystem> SubsystemToReenter = nullptr;
	FName ReentrantBarkID;

	UFUNCTION()
	void HandleBarkTriggered(FName BarkID, TArray<FString> Lines);
};
