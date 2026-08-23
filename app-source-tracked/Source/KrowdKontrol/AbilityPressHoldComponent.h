#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AbilitySlot.h"
#include "AbilityPressHoldComponent.generated.h"

class UAbilityCastComponent;
class UAbilityTargetingIndicatorComponent;

// Wires the locked press/hold indicator semantics (issue #265, docs/prd-cursor-aiming.md
// REQ-3) onto AFlatCamera3DPrototypePawn's five ability keys: a press immediately shows
// the shared UAbilityTargetingIndicatorComponent (AbilityData colour) and casts via
// UAbilityCastComponent::TryCastAbility unconditionally (existing cast gates/logic
// unchanged); holding the key past HoldThresholdSeconds - even while the ability is on
// cooldown - flips the indicator into a persistent hold-preview with no additional cast;
// releasing a held key never triggers a delayed cast. Owns no cast/cooldown/unlock logic
// itself, same "consumed, unchanged" boundary UAbilityTargetingIndicatorComponent.h
// documents for the shapes it renders.
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class KROWDKONTROL_API UAbilityPressHoldComponent : public UActorComponent
{
	GENERATED_BODY()

	friend class FKrowdKontrolAbilityPressHoldComponentTest;

public:
	UAbilityPressHoldComponent();

	static constexpr int32 NumAbilitySlots = static_cast<int32>(EAbilitySlot::Count);

	// Set by the owning pawn's constructor, same idiom as
	// PunishmentArbitrationComponent->AbilityLockoutComponent.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Press Hold")
	TObjectPtr<UAbilityCastComponent> CastComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Press Hold")
	TObjectPtr<UAbilityTargetingIndicatorComponent> IndicatorComponent;

	// How long the press-flash stays visible if the key is released (or never held past
	// HoldThresholdSeconds) - mirrors UAbilityTargetingIndicatorComponent::Flash()'s own
	// default duration value for visual consistency, even though this component never
	// calls Flash() itself (see AbilityPressHoldComponent.cpp for why).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability Press Hold", meta = (ClampMin = "0.0"))
	float PressFlashDurationSeconds = 0.15f;

	// How long a key must stay down before the flash becomes a persistent hold-preview.
	// Deliberately shorter than PressFlashDurationSeconds so a quick tap's flash is never
	// visually interrupted by a hold-preview transition - see the .cpp GOTCHA.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability Press Hold", meta = (ClampMin = "0.0"))
	float HoldThresholdSeconds = 0.1f;

	// bHasCursorTargetLocation/CursorTargetLocation stand in for a single optional
	// "const FVector* OptionalCursorTargetLocation" parameter (issue #257) - a raw
	// pointer to a non-UObject struct type is not a UFUNCTION-reflectable parameter
	// under UHT, so the option is expressed as two plain parameters instead. When
	// bHasCursorTargetLocation is true, this shows a CircleAtCursor indicator at
	// CursorTargetLocation and routes the cast through
	// UAbilityCastComponent::TryCastThrownAbilityAtLocation instead of the default
	// auto-nearest-target TryCastAbility path.
	UFUNCTION(BlueprintCallable, Category = "Ability Press Hold")
	void HandleAbilityKeyPressed(EAbilitySlot Ability, bool bHasCursorTargetLocation = false, FVector CursorTargetLocation = FVector::ZeroVector);

	UFUNCTION(BlueprintCallable, Category = "Ability Press Hold")
	void HandleAbilityKeyReleased(EAbilitySlot Ability);

	// Reflected per-slot state - the Automation test (and any future Unreal MCP-driven
	// E2E holdout, which per project memory can only read reflected UPROPERTY state, not
	// GPU pixels) asserts against this directly rather than any rendered output.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ability Press Hold")
	TArray<bool> bAbilityHoldPreviewActive;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// Timer callback, fires PressFlashDurationSeconds after a press.
	void HandlePressFlashComplete(EAbilitySlot Ability);

	// Timer callback, fires HoldThresholdSeconds after a press; only takes effect if the
	// key is still held.
	void BeginHoldPreview(EAbilitySlot Ability);

	TArray<bool> bAbilityKeyHeld;
	TArray<FTimerHandle> PressFlashTimerHandles;
	TArray<FTimerHandle> HoldThresholdTimerHandles;
};
