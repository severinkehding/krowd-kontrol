#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "OvercrowdDetectionComponent.h"
#include "OvercrowdVisualEffectSubsystem.generated.h"

class UCameraModifier_OvercrowdDistortion;

// Exactly 2 states, mirroring OvercrowdAudioSubsystem.h's EOvercrowdAudioMuffleState
// placement convention.
UENUM(BlueprintType)
enum class EOvercrowdVisualDistortionState : uint8
{
	Clear,
	Distorted
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOvercrowdVisualDistortionStateChanged, EOvercrowdVisualDistortionState, NewState);

// Drives a screen-space distortion treatment (chromatic aberration + vignette, via
// UCameraModifier_OvercrowdDistortion) on/off in sync with UOvercrowdDetectionComponent::
// OnPanicOverloadStateChanged (issue #16) - the visual counterpart to
// UOvercrowdAudioSubsystem's audio muffling (issue #38), added by issue #20's final
// rescoping (audio is not touched here; see that issue's plan Scope Decision).
//
// Structurally mirrors UOvercrowdAudioSubsystem exactly (bind-via-TActorIterator,
// delegate-driven state flip, BlueprintAssignable change delegate, friend-class test
// access, no-op-on-same-state guard, flip-before-broadcast ordering). Deliberately does
// NOT override Initialize/Deinitialize (unlike the audio sibling) - it has no persistent
// world-level side effect that needs explicit teardown, since the
// UCameraModifier_OvercrowdDistortion instance is owned by (and torn down with) the
// APlayerCameraManager itself (ModifierList.Empty() on EndPlay, PlayerCameraManager.cpp).
UCLASS()
class KROWDKONTROL_API UOvercrowdVisualEffectSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

	// Grants the Automation Framework tests direct access to DistortionModifier, which
	// they read directly to assert bIsEngaged/CurrentAlpha are wired correctly - beyond
	// that, no special access beyond what's already public (GetVisualDistortionState(),
	// TryBindOvercrowdComponent(), TryBindCameraManager()), declared for parity with
	// OvercrowdAudioSubsystem's friend-test convention.
	friend class FKrowdKontrolOvercrowdVisualEffectSubsystemTest;
	friend class FKrowdKontrolOvercrowdAudioVisualSyncTest;

public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	EOvercrowdVisualDistortionState GetVisualDistortionState() const { return CurrentState; }

	// Fires every time CurrentState actually changes (never on a no-op).
	UPROPERTY(BlueprintAssignable, Category = "Overcrowd Visual Effect")
	FOnOvercrowdVisualDistortionStateChanged OnOvercrowdVisualDistortionStateChanged;

	// Searches the world's pawns for the first UOvercrowdDetectionComponent and binds
	// HandlePanicOverloadStateChanged to its OnPanicOverloadStateChanged. Idempotent - a
	// no-op returning true if already bound. Called automatically from Tick() every frame
	// until it succeeds; exposed publicly so the Automation Framework test can drive it
	// deterministically without a real tick loop.
	UFUNCTION(BlueprintCallable, Category = "Overcrowd Visual Effect")
	bool TryBindOvercrowdComponent();

	// Searches the world's player controllers for one with a live PlayerCameraManager and
	// adds (or finds an already-added) UCameraModifier_OvercrowdDistortion to it.
	// Deliberately does not use UGameplayStatics::GetPlayerCameraManager - that resolves
	// through a LocalPlayer, which a bare FAutomationEditorCommonUtils::CreateNewMap()
	// test world never populates (see this issue's plan Test Bootstrap Gotcha section).
	// Idempotent - a no-op returning true if already bound. Called automatically from
	// Tick() every frame until it succeeds; exposed publicly for the same reason as
	// TryBindOvercrowdComponent above.
	UFUNCTION(BlueprintCallable, Category = "Overcrowd Visual Effect")
	bool TryBindCameraManager();

	// Seconds CurrentAlpha takes to ease from 0 to 1 once engaged, copied onto
	// DistortionModifier at bind time. Placeholder default, not a locked design value -
	// same rationale AbilityCooldownComponent::DefaultAbilityCooldownSeconds's comment
	// documents.
	UPROPERTY(EditDefaultsOnly, Category = "Overcrowd Visual Effect", meta = (ClampMin = "0.0"))
	float DistortionEaseInSeconds = 1.0f;

	// Seconds CurrentAlpha takes to ease from 1 back to 0 once disengaged, copied onto
	// DistortionModifier at bind time. Placeholder default, not a locked design value.
	UPROPERTY(EditDefaultsOnly, Category = "Overcrowd Visual Effect", meta = (ClampMin = "0.0"))
	float DistortionEaseOutSeconds = 1.5f;

	// Easing-curve exponent, copied onto DistortionModifier at bind time. Placeholder
	// default, not a locked design value.
	UPROPERTY(EditDefaultsOnly, Category = "Overcrowd Visual Effect", meta = (ClampMin = "0.0"))
	float DistortionEaseExponent = 2.0f;

	// Chromatic-aberration intensity at full engagement, copied onto DistortionModifier
	// at bind time. Placeholder default, not a locked design value.
	UPROPERTY(EditDefaultsOnly, Category = "Overcrowd Visual Effect", meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float MaxSceneFringeIntensity = 3.0f;

	// Vignette intensity at full engagement, copied onto DistortionModifier at bind time.
	// Placeholder default, not a locked design value.
	UPROPERTY(EditDefaultsOnly, Category = "Overcrowd Visual Effect", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxVignetteIntensity = 0.6f;

private:
	UFUNCTION()
	void HandlePanicOverloadStateChanged(EPanicOverloadState NewState);

	void SetVisualDistortionState(EOvercrowdVisualDistortionState NewState);

	UPROPERTY()
	TObjectPtr<UCameraModifier_OvercrowdDistortion> DistortionModifier;

	EOvercrowdVisualDistortionState CurrentState = EOvercrowdVisualDistortionState::Clear;

	bool bHasBoundOvercrowdComponent = false;

	// One-shot guard so a missing camera manager at SetVisualDistortionState() time only
	// logs once per UOvercrowdVisualEffectSubsystem instance, matching
	// OvercrowdAudioSubsystem::bHasWarnedMissingAudioDevice.
	bool bHasWarnedMissingCameraManager = false;
};
