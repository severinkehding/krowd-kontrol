#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AbilitySlot.h"
#include "AbilityMatchupSignalComponent.generated.h"

class AEnemyBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnAbilityMatchupSignal, EAbilitySlot, Ability, AEnemyBase*, TargetEnemy, bool, bWasColourMatched);

// PRD 09 REQ-5 / PRD 02 REQ-4's detection-only instrumentation hook (issue #37):
// classifies every successful ability cast as colour-matched or not against the
// target's real EEnemyType, and broadcasts its own FOnAbilityMatchupSignal so a
// later, separate onboarding-nudge issue can subscribe without re-deriving the
// ability-to-enemy-type colour pairing itself. Bound to
// UAbilityCastComponent::OnAbilityCastApplied the same way UGizmoFirstContactComponent
// (issue #59) is - see AFlatCamera3DPrototypePawn's constructor. No UI, no nudge
// logic, no new gameplay behavior.
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class KROWDKONTROL_API UAbilityMatchupSignalComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAbilityMatchupSignalComponent();

	UPROPERTY(BlueprintAssignable, Category = "Ability Matchup")
	FOnAbilityMatchupSignal OnAbilityMatchupSignal;

	// Bound to UAbilityCastComponent::OnAbilityCastApplied. Public (not just a private
	// delegate handler) so the Automation test can drive it directly, matching
	// FOnAbilityCastApplied's own signature exactly - same rationale as
	// UGizmoFirstContactComponent::HandleAbilityCastApplied.
	UFUNCTION()
	void HandleAbilityCastApplied(EAbilitySlot Ability, AEnemyBase* TargetEnemy);

private:
	// One-shot guard so a target missing UEnemyTypeIndicatorComponent (currently: any
	// ATrooperEnemy, or a bare test double) only logs once per component instance,
	// matching UGizmoFirstContactComponent::bHasWarnedMissingNarrativeSubsystem's
	// convention.
	bool bHasWarnedMissingEnemyTypeIndicator = false;
};
