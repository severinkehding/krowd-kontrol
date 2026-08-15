#pragma once

#include "CoreMinimal.h"
#include "GizmoBarkTestListener.generated.h"

class UGizmoNarrativeSubsystem;

// Test-only listener for UGizmoNarrativeSubsystem::OnBarkTriggered. Dynamic
// multicast delegates (DECLARE_DYNAMIC_MULTICAST_DELEGATE) only bind to UFUNCTIONs
// via AddDynamic - no AddLambda - so counting/inspecting broadcasts in
// KrowdKontrolGizmoNarrativeSubsystemTest.cpp needs this rather than a capturing
// lambda. Used only by that test.
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
