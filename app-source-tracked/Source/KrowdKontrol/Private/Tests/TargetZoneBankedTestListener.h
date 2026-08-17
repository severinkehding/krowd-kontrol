#pragma once

#include "CoreMinimal.h"
#include "TargetZoneBankedTestListener.generated.h"

// Test-only listener for ATargetZone::OnActorBanked. Dynamic multicast delegates
// (DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam) only bind to UFUNCTIONs via
// AddDynamic - no AddLambda - so counting/inspecting broadcasts in
// KrowdKontrolTargetZoneTest.cpp needs this rather than a capturing lambda. Used only
// by that test.
UCLASS()
class UTargetZoneBankedTestListener : public UObject
{
	GENERATED_BODY()

public:
	int32 CallCount = 0;

	UPROPERTY()
	TObjectPtr<AActor> LastBankedActor = nullptr;

	UFUNCTION()
	void HandleActorBanked(AActor* BankedActor);
};
