#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AbilitySlot.h"
#include "TeachingPromptComponent.generated.h"

class AEnemyBase;
class UOnScreenPromptWidget;
class AKrowdKontrolPlayerController;

// Issue #219, PRD "Level Progression & Teaching Arc" REQ-3's core-loop half (the
// ability-unlock half already shipped as UAbilityUnlockPromptComponent, issue #220):
// four one-shot on-screen instruction prompts shown only in Level 1 - "STUN IT - PRESS
// 1", "IT FOLLOWS YOU - WALK", "DROP IT ON THE GLOWING PEN", "ROOM CLEAR - DOOR OPEN" -
// each tied to a real gameplay signal already wired elsewhere in this module (ability
// casts, enemy state transitions, target-zone banking, room-clear events). No new UI:
// reuses UOnScreenPromptWidget::ShowPrompt(), the same surface
// UAbilityMatchupNudgeComponent/UAbilityUnlockPromptComponent already drive. Bound
// alongside the pawn's other OnAbilityCastApplied subscribers in
// AFlatCamera3DPrototypePawn's constructor.
//
// One component owns all four prompts' state (not four separate components) because
// prompts 2 and 3 both need the same tracked "first controlled enemy" reference - see
// the plan's Design Decisions for the rejected four-component alternative.
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class KROWDKONTROL_API UTeachingPromptComponent : public UActorComponent
{
	GENERATED_BODY()

	// Grants the Automation Framework test direct access to this component's private
	// fire/dismiss-condition methods and state, so a headless test can drive each
	// prompt's conditions deterministically without a real per-frame Tick() loop -
	// same rationale AEnemyBase's own friend-class grants document.
	friend class FKrowdKontrolTeachingPromptComponentTest;

public:
	UTeachingPromptComponent();

	// Bound to UAbilityCastComponent::OnAbilityCastApplied in the pawn's constructor.
	// Public (not just a private delegate handler) so the Automation test can drive it
	// directly, matching FOnAbilityCastApplied's own signature exactly - same
	// rationale as UFirstStunBeaconComponent::HandleAbilityCastApplied.
	UFUNCTION()
	void HandleAbilityCastApplied(EAbilitySlot Ability, AEnemyBase* TargetEnemy);

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	// Bound to every ARoomActor::OnRoomClearedStateChanged found in the world at
	// BeginPlay - see the vacuous-room filter in HandleAnyRoomClearedStateChanged().
	UFUNCTION()
	void HandleAnyRoomClearedStateChanged();

	// Bound to the tracked first-controlled enemy's own OnEnemyBanked - see
	// HandleAbilityCastApplied().
	UFUNCTION()
	void HandleFirstControlledEnemyBanked();

	// Lazily resolves the world's AKrowdKontrolPlayerController-owned
	// OnScreenPromptWidgetInstance - mirrors
	// UAbilityMatchupNudgeComponent::ResolvePromptWidget()'s exact shape.
	UOnScreenPromptWidget* ResolvePromptWidget();

	// Fire condition for "STUN IT - PRESS 1": the first enemy in the world to reach
	// EThreatState::Hot. Polled from TickComponent (mirrors
	// UMusicSubsystem::IsAnyEnemyInCombat()'s TActorIterator scan shape) since no
	// "became hot" delegate exists.
	//
	// Deliberately NOT bound to UGizmoFirstContactComponent/UFirstStunBeaconComponent's
	// first-cast signal (pass-1 validation feedback, PR #306): both of those fire only
	// after the player's first successful Stun cast (see their own class comments -
	// UGizmoFirstContactComponent::HandleAbilityCastApplied /
	// UFirstStunBeaconComponent::HandleAbilityCastApplied are both bound to
	// UAbilityCastComponent::OnAbilityCastApplied), which would make this prompt appear
	// AFTER the player has already stunned something - inverting its teaching intent
	// (the prompt must appear BEFORE the first cast, not after). Verified directly
	// against both components' source before rejecting this alternative a second time.
	void CheckStunPromptFireCondition();

	// Dismiss condition for "IT FOLLOWS YOU - WALK": the tracked first-controlled
	// enemy is still Controlled and the owning pawn is moving.
	void CheckControlPromptDismissCondition();

	// Fire condition for "DROP IT ON THE GLOWING PEN": the tracked first-controlled
	// enemy has come within DropZoneProximityUnits of a real ATargetZone.
	void CheckDropPromptFireCondition();

	UPROPERTY()
	TObjectPtr<UOnScreenPromptWidget> CachedPromptWidget;

	// One-shot guard so a missing OnScreenPromptWidgetInstance at resolve time only
	// logs once per component instance, matching
	// UAbilityMatchupNudgeComponent::bHasWarnedMissingPromptWidget's convention.
	bool bHasWarnedMissingPromptWidget = false;

	// Resolved once in BeginPlay() via UAbilityUnlockLevelSubsystem::ParseLevelIndexFromMapName()
	// - every fire/dismiss condition below is a hard no-op outside Level 1.
	bool bIsLevel1 = false;

	bool bHasFiredStunPrompt = false;
	bool bHasDismissedStunPrompt = false;
	bool bHasFiredControlPrompt = false;
	bool bHasDismissedControlPrompt = false;
	bool bHasFiredDropPrompt = false;
	bool bHasDismissedDropPrompt = false;
	bool bHasFiredRoomClearPrompt = false;

	// The first enemy this component ever saw receive control - prompts 2 and 3 both
	// track this same instance (see this class's own top comment on why one component
	// owns all four prompts).
	TWeakObjectPtr<AEnemyBase> FirstControlledEnemy;

	// Proximity radius for the "DROP IT ON THE GLOWING PEN" fire condition - how close
	// the tracked controlled enemy must be to a real ATargetZone. AllowPrivateAccess
	// keeps this private in C++ (matches UAbilityMatchupNudgeComponent's
	// ConsecutiveNonMatchedCasts/bHasShownNudge precedent) while still exposing it to
	// Blueprint/reflection-based inspection.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Teaching Prompt", meta = (AllowPrivateAccess = "true"))
	float DropZoneProximityUnits = 600.0f;
};
