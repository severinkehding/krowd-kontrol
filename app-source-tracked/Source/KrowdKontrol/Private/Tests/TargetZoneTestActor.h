#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Herdable.h"
#include "TargetZoneTestActor.generated.h"

class UBoxComponent;

// Test-only IHerdable actor with a physical UBoxComponent collision volume, for
// KrowdKontrolTargetZoneTest.cpp to sweep into an ATargetZone and confirm real
// overlap-driven banking. Deliberately distinct from AHerdableTestActor (issue #79),
// which has no collision component and is used only by KrowdKontrolHerdableTest.cpp's
// pure-contract test - giving that class a physical collision component would be an
// unrelated, silent scope change to another issue's test fixture. Used only by
// KrowdKontrolTargetZoneTest.cpp.
UCLASS()
class ATargetZoneTestActor : public AActor, public IHerdable
{
	GENERATED_BODY()

public:
	ATargetZoneTestActor();

	void SetControlled(bool bNewControlled);
	void SetHerdColourTag(FName NewColourTag);

	virtual bool IsControlled() const override;
	virtual FName GetHerdColourTag() const override;

	UPROPERTY(VisibleAnywhere, Category = "Test")
	TObjectPtr<UBoxComponent> CollisionComponent;

private:
	bool bIsControlled = false;
	FName HerdColourTag = NAME_None;
};
