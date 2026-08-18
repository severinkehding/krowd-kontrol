#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AbilitySlot.h"
#include "AbilityCastVFXComponent.generated.h"

class UPointLightComponent;
class AEnemyBase;

// Ability-side half of REQ-2's colour-match telegraph (issue #67): flashes a
// placeholder point light, tinted to AbilityData::Get(Ability).Colour, at the
// cast's target location whenever UAbilityCastComponent::OnAbilityCastApplied
// fires. Deliberately does not touch the target's own eye-glow components - see
// issue #67's scope note (enemy-side colouring is separate PRD 03/11 territory).
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class KROWDKONTROL_API UAbilityCastVFXComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAbilityCastVFXComponent();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ability Cast VFX")
	TObjectPtr<UPointLightComponent> CastFlashLightComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability Cast VFX")
	float CastFlashIntensity = 2000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability Cast VFX")
	float CastFlashDurationSeconds = 0.35f;

	// Lazily creates and attaches CastFlashLightComponent. Called automatically from
	// BeginPlay(); exposed publicly (and made idempotent) so callers - including the
	// Automation Framework test - can drive it deterministically without a full
	// actor BeginPlay lifecycle. Mirrors UEnemyTypeIndicatorComponent::
	// InitializeMarkerVisual()'s shape.
	UFUNCTION(BlueprintCallable, Category = "Ability Cast VFX")
	void InitializeCastVFX();

	// Bound to UAbilityCastComponent::OnAbilityCastApplied. Public (not just a
	// private delegate handler) so the Automation test can drive the colour mapping
	// directly, matching FOnAbilityCastApplied's own signature exactly.
	UFUNCTION()
	void HandleAbilityCastApplied(EAbilitySlot Ability, AEnemyBase* TargetEnemy);

protected:
	virtual void BeginPlay() override;

private:
	void ClearCastFlash();

	FTimerHandle CastFlashTimerHandle;
	bool bHasInitializedCastVFX = false;
};
