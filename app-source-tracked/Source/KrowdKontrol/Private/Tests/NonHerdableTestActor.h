#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NonHerdableTestActor.generated.h"

class UBoxComponent;

// Test-only, non-IHerdable actor with a physical UBoxComponent collision volume, for
// KrowdKontrolTargetZoneTest.cpp to sweep into an ATargetZone and confirm
// HandleZoneOverlap's !Herdable branch never broadcasts OnActorBanked. Mirrors
// ATargetZoneTestActor's collision setup exactly but deliberately does not implement
// IHerdable - a plain APlaceholderCubeActor's static-mesh collision profile isn't
// confirmed to generate overlap events, so a dedicated OverlapAllDynamic fixture is
// used instead to guarantee the overlap actually fires. Used only by
// KrowdKontrolTargetZoneTest.cpp.
UCLASS()
class ANonHerdableTestActor : public AActor
{
	GENERATED_BODY()

public:
	ANonHerdableTestActor();

	UPROPERTY(VisibleAnywhere, Category = "Test")
	TObjectPtr<UBoxComponent> CollisionComponent;
};
