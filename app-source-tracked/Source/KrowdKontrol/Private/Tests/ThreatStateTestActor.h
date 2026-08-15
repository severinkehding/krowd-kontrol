#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ThreatState.h"
#include "ThreatStateTestActor.generated.h"

// Minimal test-only actor implementing IThreatState (issue #81), for
// KrowdKontrolThreatStateTest.cpp to toggle and confirm GetThreatState() reports
// back correctly. Used only by that test.
UCLASS()
class AThreatStateTestActor : public AActor, public IThreatState
{
	GENERATED_BODY()

public:
	void SetThreatState(EThreatState NewState);

	virtual EThreatState GetThreatState() const override;

private:
	EThreatState CurrentThreatState = EThreatState::Idle;
};
