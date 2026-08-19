#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AbilitySlot.h"
#include "AbilityMatchupNudgeComponent.generated.h"

class AEnemyBase;
class UOnScreenPromptWidget;
class AKrowdKontrolPlayerController;

// PRD 09 REQ-5 / PRD 02 REQ-4's onboarding nudge (issue #40): consumes
// UAbilityMatchupSignalComponent::OnAbilityMatchupSignal (issue #37) and, after
// NonMatchedCastThreshold consecutive non-colour-matched successful casts, shows a
// brief reminder via the already-merged UOnScreenPromptWidget::ShowPrompt() (issue
// #34/PR #113), exactly once per pawn instance. Bound to
// UAbilityMatchupSignalComponent::OnAbilityMatchupSignal the same way
// UFirstStunBeaconComponent (issue #29) is bound to
// UAbilityCastComponent::OnAbilityCastApplied - see AFlatCamera3DPrototypePawn's
// constructor.
//
// No detection/classification logic and no new UI of its own - both already exist.
// Like UFirstStunBeaconComponent, the "fires exactly once" guarantee is a simple
// local bool guard rather than a GameInstance-subsystem registration: no
// respawn/relevel mechanic exists anywhere in this codebase today for
// AFlatCamera3DPrototypePawn, so there is nothing for the guard to need to survive.
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class KROWDKONTROL_API UAbilityMatchupNudgeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAbilityMatchupNudgeComponent();

	// Bound to UAbilityMatchupSignalComponent::OnAbilityMatchupSignal. Public (not
	// just a private delegate handler) so the Automation test can drive it directly,
	// matching FOnAbilityMatchupSignal's own signature exactly - same rationale as
	// UFirstStunBeaconComponent::HandleAbilityCastApplied.
	UFUNCTION()
	void HandleAbilityMatchupSignal(EAbilitySlot Ability, AEnemyBase* TargetEnemy, bool bWasColourMatched);

private:
	// Lazily resolves the world's AKrowdKontrolPlayerController-owned
	// OnScreenPromptWidgetInstance - mirrors UGizmoFirstContactComponent::
	// ResolveNarrativeSubsystem()'s "resolve external dependency lazily, cache once
	// found, warn exactly once if never found" shape. Unlike that component, this one
	// is never called more than once per instance: HandleAbilityMatchupSignal() sets
	// bHasShownNudge before its single call here, so there is no retry-on-a-later-call
	// path (matches UFirstStunBeaconComponent's set-before-attempt precedent instead).
	UOnScreenPromptWidget* ResolvePromptWidget();

	UPROPERTY()
	TObjectPtr<UOnScreenPromptWidget> CachedPromptWidget;

	// VisibleAnywhere/BlueprintReadOnly so MCP-driven QA can inspect nudge-threshold
	// progress on the live component instance instead of only confirming it's wired up.
	// AllowPrivateAccess keeps these private in C++ (matches ARoomActor::TargetZones'
	// precedent) while still exposing them to Blueprint/reflection-based inspection.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ability Matchup Nudge", meta = (AllowPrivateAccess = "true"))
	int32 ConsecutiveNonMatchedCasts = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ability Matchup Nudge", meta = (AllowPrivateAccess = "true"))
	bool bHasShownNudge = false;

	// One-shot guard so a missing OnScreenPromptWidgetInstance at resolve time only
	// logs once per component instance, matching
	// UGizmoFirstContactComponent::bHasWarnedMissingNarrativeSubsystem's convention.
	bool bHasWarnedMissingPromptWidget = false;

	// Implementation decision left open by the issue itself (noted in the PR body per
	// its own instruction): 3 consecutive non-colour-matched successful casts.
	static constexpr int32 NonMatchedCastThreshold = 3;
};
