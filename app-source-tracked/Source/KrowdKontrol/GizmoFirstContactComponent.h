#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AbilitySlot.h"
#include "GizmoFirstContactComponent.generated.h"

class AEnemyBase;
class UGizmoNarrativeSubsystem;

// PRD 07 REQ-2 / issue #59: the first time the player successfully casts Stun against
// an enemy, triggers a "first contact" Gizmo bark through UGizmoNarrativeSubsystem so
// the "wait, why do I recognize this robot" beat lands tied to the player's own action
// instead of an upfront exposition dump. Bound to
// UAbilityCastComponent::OnAbilityCastApplied the same way UAbilityCastVFXComponent
// (issue #67) is - see AFlatCamera3DPrototypePawn's constructor.
//
// Does no cast-counting of its own: UGizmoNarrativeSubsystem::TriggerBark already
// guarantees a registered bark fires at most once ever (GizmoNarrativeSubsystem.h/.cpp),
// so every Stun cast after the first is a silent, correct no-op through that guarantee
// alone.
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class KROWDKONTROL_API UGizmoFirstContactComponent : public UActorComponent
{
	GENERATED_BODY()

	// Grants the Automation Framework test direct access to CachedNarrativeSubsystem,
	// so a test can inject a directly-constructed UGizmoNarrativeSubsystem (mirroring
	// KrowdKontrolGizmoNarrativeSubsystemTest.cpp's NewObject<>(GameInstanceOuter)
	// construction) without depending on FAutomationEditorCommonUtils::CreateNewMap()'s
	// World having a live, subsystem-bearing GameInstance attached - unconfirmed for
	// that World type anywhere else in this codebase.
	friend class FKrowdKontrolGizmoFirstContactComponentTest;

public:
	UGizmoFirstContactComponent();

	// Bound to UAbilityCastComponent::OnAbilityCastApplied. Public (not just a private
	// delegate handler) so the Automation test can drive it directly, matching
	// FOnAbilityCastApplied's own signature exactly - same rationale as
	// UAbilityCastVFXComponent::HandleAbilityCastApplied.
	UFUNCTION()
	void HandleAbilityCastApplied(EAbilitySlot Ability, AEnemyBase* TargetEnemy);

	// Idempotently registers the first-contact bark with the resolved
	// UGizmoNarrativeSubsystem if it isn't already registered. Never re-registers an
	// already-registered ID - see GizmoNarrativeSubsystem.h's RegisterBark comment:
	// re-registering an already-fired BarkID would reset bHasBeenTriggered to false,
	// which would let this bark replay on a later pawn respawn/relevel. Called
	// automatically from BeginPlay(); exposed publicly (mirroring
	// UAbilityCastVFXComponent::InitializeCastVFX()'s rationale) so the Automation
	// Framework test can drive it deterministically without a full actor BeginPlay
	// lifecycle.
	UFUNCTION(BlueprintCallable, Category = "Gizmo Narrative")
	void InitializeFirstContactBark();

protected:
	virtual void BeginPlay() override;

private:
	UGizmoNarrativeSubsystem* ResolveNarrativeSubsystem();

	UPROPERTY()
	TObjectPtr<UGizmoNarrativeSubsystem> CachedNarrativeSubsystem;

	bool bHasInitializedFirstContactBark = false;

	// One-shot guard so a missing GameInstance/subsystem at resolve time only logs
	// once per component instance, matching
	// UOvercrowdAudioSubsystem::bHasWarnedMissingAudioDevice's convention.
	bool bHasWarnedMissingNarrativeSubsystem = false;

	static const FName FirstContactBarkID;
};
