#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraModifier.h"
#include "CameraModifier_OvercrowdDistortion.generated.h"

// Runtime post-process modifier driving the Overcrowd (Panic Overload) screen-distortion
// visual treatment (issue #20). Named per the engine's own UCameraModifier_* subclass
// convention (UCameraModifier_CameraShake is the engine's own example), not this
// codebase's UOvercrowd<Domain>Subsystem family - both are internally consistent with
// their respective base-class conventions.
//
// Owned and driven by UOvercrowdVisualEffectSubsystem, which calls SetEngaged() on every
// Panic Overload state transition. This class never reads UOvercrowdDetectionComponent or
// any delegate directly - it only eases CurrentAlpha toward whichever target SetEngaged()
// last requested and writes that alpha into the post-process blend weight every frame via
// ModifyPostProcess, mirroring UOvercrowdAudioSubsystem's own separation between
// "state machine" (the subsystem) and "effect application" (here, the submix override).
UCLASS()
class KROWDKONTROL_API UCameraModifier_OvercrowdDistortion : public UCameraModifier
{
	GENERATED_BODY()

	// Grants the visual-effect-subsystem test and the audio/visual sync test direct read
	// of CurrentAlpha/bIsEngaged for assertions beyond the public GetCurrentAlpha() getter,
	// mirroring OvercrowdAudioSubsystem.h's friend-grant rationale (reading internal
	// state directly, not just the public surface).
	friend class FKrowdKontrolOvercrowdVisualEffectSubsystemTest;
	friend class FKrowdKontrolOvercrowdAudioVisualSyncTest;

public:
	// True = ease CurrentAlpha toward 1.0 (full distortion), false = ease toward 0.0
	// (clear). Does not itself touch FPostProcessSettings - that only happens per-frame
	// in ModifyPostProcess.
	void SetEngaged(bool bNewEngaged) { bIsEngaged = bNewEngaged; }

	// For the test to assert convergence without needing friend access to a private
	// member - mirrors GetMuffleState()/GetPanicOverloadState()'s public-getter convention.
	float GetCurrentAlpha() const { return CurrentAlpha; }

	// Seconds CurrentAlpha takes to ease from 0 to 1 once engaged. Placeholder default,
	// not a locked design value - same rationale AbilityCooldownComponent::
	// DefaultAbilityCooldownSeconds's comment documents.
	UPROPERTY(EditDefaultsOnly, Category = "Overcrowd Distortion", meta = (ClampMin = "0.0"))
	float EaseInSeconds = 1.0f;

	// Seconds CurrentAlpha takes to ease from 1 back to 0 once disengaged. Deliberately
	// slower than EaseInSeconds - "tension lingers a beat longer than it resolves" per
	// the PRD's Open Questions steer toward a dramatic, non-punitive read. Placeholder
	// default, not a locked design value.
	UPROPERTY(EditDefaultsOnly, Category = "Overcrowd Distortion", meta = (ClampMin = "0.0"))
	float EaseOutSeconds = 1.5f;

	// Exponent passed to FMath::InterpEaseInOut - the easing-curve knob the PRD's Open
	// Questions asked for. Placeholder default, not a locked design value.
	UPROPERTY(EditDefaultsOnly, Category = "Overcrowd Distortion", meta = (ClampMin = "0.0"))
	float EaseExponent = 2.0f;

	// Chromatic-aberration intensity at full engagement (CurrentAlpha == 1.0). Engine
	// UIMax is 5.0; kept mid-range deliberately to avoid an overwhelming, punitive-
	// feeling extreme. Placeholder default, not a locked design value.
	UPROPERTY(EditDefaultsOnly, Category = "Overcrowd Distortion", meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float MaxSceneFringeIntensity = 3.0f;

	// Vignette intensity at full engagement (CurrentAlpha == 1.0). Engine UIMax is 1.0.
	// Placeholder default, not a locked design value.
	UPROPERTY(EditDefaultsOnly, Category = "Overcrowd Distortion", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxVignetteIntensity = 0.6f;

protected:
	// Override signature verified at CameraModifier.h:175. Protected on the base class -
	// tests instead call the public ModifyCamera(DeltaTime, FMinimalViewInfo&) entry
	// point (verified public at CameraModifier.h:95), which internally calls this.
	virtual void ModifyPostProcess(float DeltaTime, float& PostProcessBlendWeight, FPostProcessSettings& PostProcessSettings) override;

private:
	bool bIsEngaged = false;

	// 0..1 linear progress toward the current target (bIsEngaged ? 1.0 : 0.0), accumulated
	// directly from DeltaTime/EaseSeconds each frame. Drives CurrentAlpha via
	// InterpEaseInOut every frame but is never itself fed through the ease curve - keeping
	// this separate from CurrentAlpha avoids feeding an already-eased value back in as next
	// frame's linear input, which would self-damp toward a near-zero fixed point.
	float RawProgress = 0.0f;
	float CurrentAlpha = 0.0f;
};
