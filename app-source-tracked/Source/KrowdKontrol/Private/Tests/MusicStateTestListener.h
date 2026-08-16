#pragma once
#include "CoreMinimal.h"
#include "MusicSubsystem.h"
#include "MusicStateTestListener.generated.h"

// Test-only listener for UMusicSubsystem::OnMusicStateChanged (issue #25). Dynamic
// multicast delegates only bind UFUNCTIONs via AddDynamic - no AddLambda - so
// counting broadcasts in KrowdKontrolMusicSubsystemTest.cpp needs this rather than a
// capturing lambda. Mirrors UEnemyBankedTestListener/UGizmoBarkTestListener. Used
// only by that test.
UCLASS()
class UMusicStateTestListener : public UObject
{
	GENERATED_BODY()

public:
	int32 CallCount = 0;
	EMusicState LastState = EMusicState::Calm;

	UFUNCTION()
	void HandleMusicStateChanged(EMusicState NewState);
};
