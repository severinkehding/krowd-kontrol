#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OvercrowdDetectionComponent.h"
#include "PunishmentArbitrationComponent.generated.h"

class UAbilityLockoutComponent;
class USpeedReductionPunishmentComponent;

// Enforces PRD "Punishment System (Punishments 1 & 2 + arbitration)" REQ-4 (issue #180,
// resolving closed issue #24): at most one of {Overcrowd, ability-lock, speed-reduction}
// is ever active at once, priority Overcrowd > ability-lock > speed-reduction. The owning
// pawn's constructor wires OnPunishmentTriggered (UPunishmentManagerComponent, issue #177)
// to THIS component only - not directly to AbilityLockoutComponent/
// SpeedReductionPunishmentComponent as before - so every contact-damage trigger passes
// through arbitration before either punishment can activate. Overcrowd
// (UOvercrowdDetectionComponent) is not gated the same way: it has its own independent
// trigger condition (crowd convergence, not contact damage) and always wins outright, so
// this component only listens to its OnPanicOverloadStateChanged to know when to preempt
// the other two, never to gate Overcrowd's own activation. UOvercrowdDetectionComponent is
// Blueprint-placed, not pawn-C++-constructed (see AbilityCastComponent.h's own comment on
// this same asymmetry), so OvercrowdComponent is resolved lazily in BeginPlay() via
// FindComponentByClass rather than wired in the pawn constructor like the other two.
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class KROWDKONTROL_API UPunishmentArbitrationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPunishmentArbitrationComponent();

	// Resolved in BeginPlay() via GetOwner()->FindComponentByClass - nullptr if this pawn
	// has no UOvercrowdDetectionComponent placed on it (e.g. via Blueprint). Every call
	// site null-checks before use.
	UPROPERTY()
	TObjectPtr<UOvercrowdDetectionComponent> OvercrowdComponent;

	// Wired explicitly by the owning pawn's constructor, same idiom
	// USpeedReductionPunishmentComponent::MovementComponent uses. Nullptr on pawns with no
	// ability-lock punishment at all (e.g. Paper2DPrototypePawn) - every call site
	// null-checks before use.
	UPROPERTY()
	TObjectPtr<UAbilityLockoutComponent> AbilityLockoutComponent;

	UPROPERTY()
	TObjectPtr<USpeedReductionPunishmentComponent> SpeedReductionComponent;

	// Bound to UPunishmentManagerComponent::OnPunishmentTriggered. Drops the trigger
	// entirely if Overcrowd is currently Active (priority 1, issue #180 AC: not queued).
	// Otherwise, if AbilityLockoutComponent exists on this pawn AND
	// UAbilityLockoutComponent::IsLockoutEnabledByCVar() (kk.Punishment.LockoutEnabled,
	// issue #181) is true, ends any active SpeedReductionComponent immediately (priority 2
	// preempts priority 3 on this shared trigger - there is no separate signal to tell
	// "this trigger is for ability-lock" vs "this trigger is for speed-reduction" apart, so
	// ability-lock, being higher priority, always wins whenever both exist) and activates
	// ability-lock. If no AbilityLockoutComponent exists on this pawn (Paper2DPrototypePawn),
	// or lockout is CVar-disabled, falls through to activating SpeedReductionComponent
	// normally instead - without this CVar check, disabling lockout for playtesting would
	// still preempt-and-cancel an active speed-reduction rather than let it run.
	UFUNCTION()
	void HandlePunishmentTriggered();

	// Bound to UOvercrowdDetectionComponent::OnPanicOverloadStateChanged in BeginPlay(). On
	// the Inactive->Active transition, force-ends AbilityLockoutComponent and
	// SpeedReductionComponent immediately if either is active, reverting their effects in
	// full - Overcrowd preempts both lower-priority punishments outright. No action on
	// Active->Inactive; recovery does not resurrect a preempted punishment (issue #180 AC:
	// "not queued").
	UFUNCTION()
	void HandlePanicOverloadStateChanged(EPanicOverloadState NewState);

protected:
	virtual void BeginPlay() override;

private:
	bool IsOvercrowdActive() const;
};
