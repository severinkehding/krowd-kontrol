#pragma once
#include "CoreMinimal.h"
#include "OvercrowdAudioSubsystem.h"
#include "OvercrowdMuffleStateTestListener.generated.h"

// Test-only listener for UOvercrowdAudioSubsystem::OnOvercrowdAudioMuffleStateChanged
// (issue #38). Dynamic multicast delegates only bind UFUNCTIONs via AddDynamic - no
// AddLambda - so counting broadcasts in KrowdKontrolOvercrowdAudioSubsystemTest.cpp needs
// this rather than a capturing lambda. Mirrors UMusicStateTestListener/
// UPanicOverloadStateTestListener. Used only by that test.
UCLASS()
class UOvercrowdMuffleStateTestListener : public UObject
{
	GENERATED_BODY()

public:
	int32 CallCount = 0;
	EOvercrowdAudioMuffleState LastState = EOvercrowdAudioMuffleState::Clear;

	UFUNCTION()
	void HandleOvercrowdAudioMuffleStateChanged(EOvercrowdAudioMuffleState NewState);
};
