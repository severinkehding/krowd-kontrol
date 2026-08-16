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

	// Optional: if set, HandleMusicStateChanged() records GetMusicState() read
	// through this pointer during the callback, into ObservedStateDuringBroadcast -
	// proves the "flip CurrentState before broadcasting" re-entrancy guarantee
	// documented in MusicSubsystem.cpp::SetMusicState() actually holds.
	UPROPERTY()
	TObjectPtr<UMusicSubsystem> WatchedSubsystem;

	EMusicState ObservedStateDuringBroadcast = EMusicState::Calm;

	UFUNCTION()
	void HandleMusicStateChanged(EMusicState NewState);
};
