#pragma once
#include "CoreMinimal.h"
#include "OvercrowdDetectionComponent.h"
#include "PanicOverloadStateTestListener.generated.h"

// Test-only listener for UOvercrowdDetectionComponent::OnPanicOverloadStateChanged
// (issue #16). Dynamic multicast delegates only bind UFUNCTIONs via AddDynamic - no
// AddLambda - so counting broadcasts in
// KrowdKontrolOvercrowdDetectionComponentTest.cpp needs this rather than a capturing
// lambda. Mirrors UMusicStateTestListener/UEnemyBankedTestListener. Used only by that
// test.
UCLASS()
class UPanicOverloadStateTestListener : public UObject
{
	GENERATED_BODY()

public:
	int32 CallCount = 0;
	EPanicOverloadState LastState = EPanicOverloadState::Inactive;

	UFUNCTION()
	void HandlePanicOverloadStateChanged(EPanicOverloadState NewState);
};
