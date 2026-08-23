#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LevelFailComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLevelFailed);

// PRD "Run Lifecycle & Progression Signals" REQ-3 (issue #171). Listens to a
// PlayerEnergyComponent's OnEnergyChanged and broadcasts OnLevelFailed the
// moment energy reaches 0. Naturally exactly-once: OnEnergyChanged only fires
// on an actual change, and UPlayerEnergyComponent::ApplyContactDamage floors
// CurrentEnergy at 0, so a second hit at 0 energy never re-triggers this.
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class KROWDKONTROL_API ULevelFailComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULevelFailComponent();

	// Fires exactly once when energy reaches 0, no payload - consumers key off
	// "the run has failed", not the energy value.
	UPROPERTY(BlueprintAssignable, Category = "Level Fail")
	FOnLevelFailed OnLevelFailed;

	// Bound to a PlayerEnergyComponent's OnEnergyChanged in each pawn's
	// constructor. Broadcasts OnLevelFailed when NewEnergy reaches the floor.
	// Read-only fired-state for tick-driven systems that must freeze on level
	// fail but live outside the input stack (PR #279 review: the cursor-facing
	// path). Same stale-read-safe contract as other one-shot flags.
public:
	bool HasLevelFailed() const { return bHasFired; }

private:
	UFUNCTION()
	void HandleEnergyChanged(float NewEnergy);

private:
	// Local exactly-once guard, matching UFirstStunBeaconComponent::bHasTriggeredBeacon.
	// The header comment above documents that ApplyContactDamage's change-guard makes
	// this naturally exactly-once today, but that's an upstream implementation detail
	// this component shouldn't have to trust - this guard makes it correct on its own.
	bool bHasFired = false;
};
