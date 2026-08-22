#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AbilitySlot.h"
#include "AbilityUnlockPromptComponent.generated.h"

class UOnScreenPromptWidget;
class AKrowdKontrolPlayerController;

// PRD "Level Progression & Teaching Arc" REQ-3's ability-unlock half (issue #220):
// consumes UAbilityUnlockComponent::OnAbilityUnlocked and, on each broadcast, shows a
// one-shot UOnScreenPromptWidget::ShowPrompt() naming the newly unlocked ability, its
// key, and its colour-matched countered enemy type ("SLEEP — PRESS 2 — STRONG VS
// SNIPERS"). Bound to UAbilityUnlockComponent::OnAbilityUnlocked the same way
// UAbilityMatchupNudgeComponent (issue #40) is bound to
// UAbilityMatchupSignalComponent::OnAbilityMatchupSignal - see
// AFlatCamera3DPrototypePawn's constructor.
//
// No detection/unlock logic and no new UI of its own - both already exist.
// Fire-once-per-ability-per-run falls straight out of
// UAbilityUnlockComponent::UnlockAbility()'s own idempotency guard (OnAbilityUnlocked
// only ever broadcasts once per ability per component instance), so no additional
// one-shot guard is needed here.
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class KROWDKONTROL_API UAbilityUnlockPromptComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAbilityUnlockPromptComponent();

	// Bound to UAbilityUnlockComponent::OnAbilityUnlocked. Public (not just a private
	// delegate handler) so the Automation test can drive it directly, matching
	// FOnAbilityUnlocked's own signature exactly - same rationale as
	// UAbilityMatchupNudgeComponent::HandleAbilityMatchupSignal.
	UFUNCTION()
	void HandleAbilityUnlocked(EAbilitySlot Ability);

	// Shows any prompt(s) that arrived before a resolvable OnScreenPromptWidget
	// existed (issue #235 E2E fix): on a real level-arrival flow,
	// UAbilityUnlockLevelSubsystem::HandleLevelBegin() can call NotifyLevelReached()
	// - and so broadcast OnAbilityUnlocked - before
	// AKrowdKontrolPlayerController::CreateHUDWidgets() has created
	// OnScreenPromptWidgetInstance, since OnAbilityUnlocked only ever broadcasts once
	// per ability (UAbilityUnlockComponent::UnlockAbility()'s idempotency guard) a
	// prompt missed at that moment would otherwise be lost forever. Called from
	// AKrowdKontrolPlayerController::WireWidgetsToPawn(), which - like
	// UAbilityUnlockLevelSubsystem::RetryPendingUnlockForPawn() - is guaranteed to run
	// again once the widget exists regardless of the OnPossess/BeginPlay race.
	void FlushPendingPrompts();

private:
	// Lazily resolves the world's AKrowdKontrolPlayerController-owned
	// OnScreenPromptWidgetInstance - mirrors
	// UAbilityMatchupNudgeComponent::ResolvePromptWidget()'s "resolve external
	// dependency lazily, cache once found, warn exactly once if never found" shape.
	UOnScreenPromptWidget* ResolvePromptWidget();

	// Builds and shows the wording for Ability on Widget - shared by
	// HandleAbilityUnlocked's immediate path and FlushPendingPrompts' deferred path.
	void ShowPromptForAbility(UOnScreenPromptWidget* Widget, EAbilitySlot Ability);

	UPROPERTY()
	TObjectPtr<UOnScreenPromptWidget> CachedPromptWidget;

	// One-shot guard so a missing OnScreenPromptWidgetInstance at resolve time only
	// logs once per component instance, matching
	// UAbilityMatchupNudgeComponent::bHasWarnedMissingPromptWidget's convention.
	bool bHasWarnedMissingPromptWidget = false;

	// Abilities whose prompt couldn't be shown yet because no OnScreenPromptWidget
	// was resolvable at broadcast time - drained by FlushPendingPrompts() once one is.
	TArray<EAbilitySlot> PendingAbilities;
};
