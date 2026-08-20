#include "OvercrowdVisualEffectSubsystem.h"
#include "EngineUtils.h"
#include "CameraModifier_OvercrowdDistortion.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Pawn.h"

void UOvercrowdVisualEffectSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!bHasBoundOvercrowdComponent)
	{
		TryBindOvercrowdComponent();
	}
	if (!DistortionModifier)
	{
		TryBindCameraManager();
	}
}

TStatId UOvercrowdVisualEffectSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UOvercrowdVisualEffectSubsystem, STATGROUP_Tickables);
}

bool UOvercrowdVisualEffectSubsystem::TryBindOvercrowdComponent()
{
	if (bHasBoundOvercrowdComponent)
	{
		return true;
	}
	if (!GetWorld())
	{
		return false;
	}

	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		if (UOvercrowdDetectionComponent* Overcrowd = It->FindComponentByClass<UOvercrowdDetectionComponent>())
		{
			Overcrowd->OnPanicOverloadStateChanged.AddDynamic(this, &UOvercrowdVisualEffectSubsystem::HandlePanicOverloadStateChanged);
			bHasBoundOvercrowdComponent = true;
			return true;
		}
	}
	return false;
}

bool UOvercrowdVisualEffectSubsystem::TryBindCameraManager()
{
	if (DistortionModifier)
	{
		return true;
	}
	if (!GetWorld())
	{
		return false;
	}

	for (TActorIterator<APlayerController> It(GetWorld()); It; ++It)
	{
		if (APlayerCameraManager* CameraManager = It->PlayerCameraManager)
		{
			UCameraModifier* Existing = CameraManager->FindCameraModifierByClass(UCameraModifier_OvercrowdDistortion::StaticClass());
			DistortionModifier = Existing
				? Cast<UCameraModifier_OvercrowdDistortion>(Existing)
				: Cast<UCameraModifier_OvercrowdDistortion>(CameraManager->AddNewCameraModifier(UCameraModifier_OvercrowdDistortion::StaticClass()));
			if (DistortionModifier)
			{
				DistortionModifier->EaseInSeconds = DistortionEaseInSeconds;
				DistortionModifier->EaseOutSeconds = DistortionEaseOutSeconds;
				DistortionModifier->EaseExponent = DistortionEaseExponent;
				DistortionModifier->MaxSceneFringeIntensity = MaxSceneFringeIntensity;
				DistortionModifier->MaxVignetteIntensity = MaxVignetteIntensity;
				return true;
			}
		}
	}
	return false;
}

void UOvercrowdVisualEffectSubsystem::HandlePanicOverloadStateChanged(EPanicOverloadState NewState)
{
	SetVisualDistortionState(NewState == EPanicOverloadState::Active
		? EOvercrowdVisualDistortionState::Distorted
		: EOvercrowdVisualDistortionState::Clear);
}

void UOvercrowdVisualEffectSubsystem::SetVisualDistortionState(EOvercrowdVisualDistortionState NewState)
{
	if (NewState == CurrentState)
	{
		return;
	}

	// Flip before broadcasting (see OvercrowdAudioSubsystem::SetMuffleState) so a
	// re-entrant listener sees CurrentState already updated.
	CurrentState = NewState;

	if (DistortionModifier)
	{
		DistortionModifier->SetEngaged(NewState == EOvercrowdVisualDistortionState::Distorted);
	}
	else if (!bHasWarnedMissingCameraManager)
	{
		// CurrentState/the broadcast below still reflect the requested state even though
		// the modifier call was skipped (see OvercrowdAudioSubsystem::SetMuffleState's
		// bHasWarnedMissingAudioDevice precedent for this same "logically succeeded, side
		// effect didn't" shape) - log once so a missing camera manager window is
		// diagnosable instead of silently invisible.
		bHasWarnedMissingCameraManager = true;
		UE_LOG(LogTemp, Warning,
			TEXT("UOvercrowdVisualEffectSubsystem: no PlayerCameraManager available - visual distortion state changed to %d but the camera modifier could not be applied."),
			static_cast<int32>(NewState));
	}

	OnOvercrowdVisualDistortionStateChanged.Broadcast(CurrentState);
}
