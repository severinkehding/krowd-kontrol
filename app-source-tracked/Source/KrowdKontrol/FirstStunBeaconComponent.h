#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AbilitySlot.h"
#include "FirstStunBeaconComponent.generated.h"

class AEnemyBase;
class APlaceholderTargetZoneActor;

// PRD 09 REQ-2 / issue #29: the instant the player lands their first successful Stun
// cast of the session, the nearest APlaceholderTargetZoneActor's beacon light
// intensifies - a live, in-world visual cue teaching "herd" as the next beat after
// the tutorial teaches "control". Bound to
// UAbilityCastComponent::OnAbilityCastApplied the same way UGizmoFirstContactComponent
// (issue #59) is - see AFlatCamera3DPrototypePawn's constructor.
//
// Unlike UGizmoFirstContactComponent, the "fires exactly once" guarantee here is a
// simple local bool guard rather than a GameInstance-subsystem registration: no
// respawn/relevel mechanic exists anywhere in this codebase today for
// AFlatCamera3DPrototypePawn, so there is nothing for the guard to need to survive.
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class KROWDKONTROL_API UFirstStunBeaconComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFirstStunBeaconComponent();

	// Bound to UAbilityCastComponent::OnAbilityCastApplied. Public (not just a private
	// delegate handler) so the Automation test can drive it directly, matching
	// FOnAbilityCastApplied's own signature exactly - same rationale as
	// UGizmoFirstContactComponent::HandleAbilityCastApplied.
	UFUNCTION()
	void HandleAbilityCastApplied(EAbilitySlot Ability, AEnemyBase* TargetEnemy);

private:
	// Nearest APlaceholderTargetZoneActor to GetOwner(), or nullptr if none exist in
	// the world - mirrors UAbilityCastComponent::FindNearestValidTarget()'s
	// TActorIterator + DistSquared scan shape, with no range/state filter.
	APlaceholderTargetZoneActor* FindNearestTargetZone() const;

	bool bHasTriggeredBeacon = false;
};
