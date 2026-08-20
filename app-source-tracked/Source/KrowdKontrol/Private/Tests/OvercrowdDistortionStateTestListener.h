#pragma once
#include "CoreMinimal.h"
#include "OvercrowdVisualEffectSubsystem.h"
#include "OvercrowdDistortionStateTestListener.generated.h"

// Test-only listener for UOvercrowdVisualEffectSubsystem::OnOvercrowdVisualDistortionStateChanged
// (issue #20). Dynamic multicast delegates only bind UFUNCTIONs via AddDynamic - no
// AddLambda - so counting broadcasts in the visual-effect-subsystem/sync tests needs
// this rather than a capturing lambda. Mirrors UOvercrowdMuffleStateTestListener. Used
// only by those tests.
UCLASS()
class UOvercrowdDistortionStateTestListener : public UObject
{
	GENERATED_BODY()

public:
	int32 CallCount = 0;
	EOvercrowdVisualDistortionState LastState = EOvercrowdVisualDistortionState::Clear;

	UFUNCTION()
	void HandleOvercrowdVisualDistortionStateChanged(EOvercrowdVisualDistortionState NewState);
};
