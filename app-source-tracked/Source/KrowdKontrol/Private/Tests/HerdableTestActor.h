#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Herdable.h"
#include "HerdableTestActor.generated.h"

// Minimal test-only actor implementing IHerdable (issue #79), for
// KrowdKontrolHerdableTest.cpp to toggle and confirm IsControlled() /
// GetHerdColourTag() report back correctly. Used only by that test.
UCLASS()
class AHerdableTestActor : public AActor, public IHerdable
{
	GENERATED_BODY()

public:
	void SetControlled(bool bNewControlled);
	void SetHerdColourTag(FName NewColourTag);

	virtual bool IsControlled() const override;
	virtual FName GetHerdColourTag() const override;

private:
	bool bIsControlled = false;
	FName HerdColourTag = NAME_None;
};
