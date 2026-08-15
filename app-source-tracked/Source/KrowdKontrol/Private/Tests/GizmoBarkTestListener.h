#pragma once

#include "CoreMinimal.h"
#include "GizmoBarkTestListener.generated.h"

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

	UFUNCTION()
	void HandleBarkTriggered(FName BarkID, TArray<FString> Lines);
};
