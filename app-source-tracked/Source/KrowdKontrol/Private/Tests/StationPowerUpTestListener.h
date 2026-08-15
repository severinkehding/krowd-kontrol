#pragma once

#include "CoreMinimal.h"
#include "StationPowerUpTestListener.generated.h"

// Test-only listener for UStationPowerUpComponent::OnLightEnabled and
// OnPowerUpSequenceComplete. Dynamic multicast delegates
// (DECLARE_DYNAMIC_MULTICAST_DELEGATE) only bind to UFUNCTIONs via AddDynamic - no
// AddLambda - so counting/inspecting broadcasts in
// KrowdKontrolStationPowerUpComponentTest.cpp needs this rather than a capturing
// lambda. Used only by that test.
UCLASS()
class UStationPowerUpTestListener : public UObject
{
	GENERATED_BODY()

public:
	int32 LightEnabledCallCount = 0;
	int32 LastEnabledLightIndex = INDEX_NONE;
	TObjectPtr<AActor> LastEnabledLightActor = nullptr;
	int32 SequenceCompleteCallCount = 0;

	UFUNCTION()
	void HandleLightEnabled(int32 LightIndex, AActor* LightActor);

	UFUNCTION()
	void HandleSequenceComplete();
};
