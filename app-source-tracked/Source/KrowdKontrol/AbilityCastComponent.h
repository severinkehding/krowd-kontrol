#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AbilitySlot.h"
#include "AbilityCastComponent.generated.h"

class AEnemyBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAbilityCastApplied, EAbilitySlot, Ability, AEnemyBase*, TargetEnemy);

// The single production entry point that finally calls AEnemyBase::ReceiveControl()
// (issue #12's hook, previously called only from tests) - see MISSION.md, PRD 02, and
// the at-least-8 previously-rejected issues (#65/#67/#59/#48/#50/#52/#121/#29) blocked
// on this exact gap. TryCastAbility(EAbilitySlot) gates the attempt through
// UAbilityUnlockComponent::IsAbilityUnlocked (locked abilities can't cast) and
// UAbilityCooldownComponent::TryStartCooldown (the documented, sole legal cast-gating
// point - see AbilityCooldownComponent.h), does minimal automatic targeting (nearest
// hot - Alert or Attack - enemy within CastRangeUnits, mirroring
// UOvercrowdDetectionComponent::CountHotUncontrolledEnemiesNearby's scan shape), and
// applies control via AEnemyBase::ReceiveControl(). Broadcasts OnAbilityCastApplied
// exactly once per successful cast so other systems (Gizmo bark trigger #59,
// instrumentation #37 - neither wired up by this issue) can subscribe later.
//
// Attached to the player pawn via CreateDefaultSubobject in
// AFlatCamera3DPrototypePawn's constructor, alongside AbilityUnlockComponent and
// AbilityCooldownComponent - not Blueprint-wired like UOvercrowdDetectionComponent,
// since a cast input needs a guaranteed-present component on the one playable pawn.
// Reads GetOwner()'s location directly, same as UOvercrowdDetectionComponent does.
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class KROWDKONTROL_API UAbilityCastComponent : public UActorComponent
{
	GENERATED_BODY()

	// Automation Framework test access. FindNearestValidTarget's targeting logic
	// (nearest-of-two, out-of-range exclusion, wrong-state exclusion) is verified
	// indirectly through TryCastAbility's return value and the target's resulting
	// GetEnemyState() - see KrowdKontrolAbilityCastComponentTest.cpp cases (e)-(g).
	friend class FKrowdKontrolAbilityCastComponentTest;

public:
	UAbilityCastComponent();

	// Flat range applied to all 5 abilities alike - per-ability range (mapping
	// EAbilityRange Short/Medium/Long to distinct concrete distances) is explicitly
	// out of this issue's scope; see AbilityData.h's EAbilityRange for the future
	// extension point.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability Cast", meta = (ClampMin = "0.0"))
	float CastRangeUnits = 1500.0f;

	// Gate order: IsAbilityUnlocked -> IsOnCooldown (read-only) -> IsAbilityLocked
	// (read-only, optional - see AbilityLockoutComponent.h; absence of the component is
	// NOT a gate failure, unlike Unlock/Cooldown) -> nearest-hot-enemy-in-range search
	// -> TryStartCooldown -> ReceiveControl -> broadcast. A whiff (no valid target in
	// range) does NOT consume the cooldown - TryStartCooldown is only called once a
	// target is already confirmed. Returns false and changes nothing if any gate fails
	// or no target is found.
	UFUNCTION(BlueprintCallable, Category = "Ability Cast")
	bool TryCastAbility(EAbilitySlot Ability);

	// Fires exactly once per successful TryCastAbility call, after ReceiveControl has
	// already been applied to TargetEnemy.
	UPROPERTY(BlueprintAssignable, Category = "Ability Cast")
	FOnAbilityCastApplied OnAbilityCastApplied;

private:
	// Nearest AEnemyBase within CastRangeUnits of GetOwner() whose GetEnemyState() is
	// Alert or Attack (Idle/Controlled/Banked are never valid targets) - mirrors
	// UOvercrowdDetectionComponent::CountHotUncontrolledEnemiesNearby's
	// TActorIterator<AEnemyBase> + Alert/Attack filter + DistSquared shape. Returns
	// nullptr if no such enemy exists.
	AEnemyBase* FindNearestValidTarget() const;
};
