#include "CameraModifier_OvercrowdDistortion.h"

void UCameraModifier_OvercrowdDistortion::ModifyPostProcess(float DeltaTime, float& PostProcessBlendWeight, FPostProcessSettings& PostProcessSettings)
{
	// Call the base implementation first (a no-op today, matches engine convention for
	// modifier chaining - keeps this future-proof against an engine version where it
	// isn't a no-op).
	Super::ModifyPostProcess(DeltaTime, PostProcessBlendWeight, PostProcessSettings);

	const float TargetProgress = bIsEngaged ? 1.0f : 0.0f;
	const float EaseSeconds = bIsEngaged ? EaseInSeconds : EaseOutSeconds;
	if (EaseSeconds > 0.0f)
	{
		const float Step = DeltaTime / EaseSeconds;
		RawProgress = FMath::Clamp(RawProgress + (bIsEngaged ? Step : -Step), 0.0f, 1.0f);
		CurrentAlpha = FMath::InterpEaseInOut(0.0f, 1.0f, RawProgress, EaseExponent);
	}
	else
	{
		RawProgress = TargetProgress;
		CurrentAlpha = RawProgress;
	}

	// The override fields are always set to their max value; PostProcessBlendWeight is
	// what actually ramps the perceived intensity from 0 to max - this is how
	// AddCachedPPBlend's weighted blend is intended to be driven (verified at
	// PlayerCameraManager.cpp:309-316). Do not additionally scale SceneFringeIntensity/
	// VignetteIntensity by CurrentAlpha - that would double-ease the curve.
	PostProcessSettings.bOverride_SceneFringeIntensity = true;
	PostProcessSettings.SceneFringeIntensity = MaxSceneFringeIntensity;
	PostProcessSettings.bOverride_VignetteIntensity = true;
	PostProcessSettings.VignetteIntensity = MaxVignetteIntensity;
	PostProcessBlendWeight = CurrentAlpha;
}
