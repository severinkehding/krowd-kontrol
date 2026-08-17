#include "OvercrowdAudioSubsystem.h"
#include "EngineUtils.h"
#include "AudioMixerBlueprintLibrary.h"
#include "AudioDeviceHandle.h"
#include "SubmixEffects/SubmixEffectFilter.h"
#include "GameFramework/Pawn.h"

void UOvercrowdAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	MuffleFilterPreset = NewObject<USubmixEffectFilterPreset>(this);
	MuffleFilterPreset->Settings.FilterType = ESubmixFilterType::LowPass;
	MuffleFilterPreset->Settings.FilterFrequency = MuffleFilterFrequencyHz;
}

void UOvercrowdAudioSubsystem::Deinitialize()
{
	// The submix effect chain override is applied to the main output submix, not to anything
	// owned/destroyed alongside this subsystem - clear it explicitly so a level transition or
	// PIE stop that happens while Muffled doesn't leave the next world's audio mix stuck
	// filtered (see WaveSpawnerComponent::EndPlay for the same teardown convention).
	if (CurrentMuffleState == EOvercrowdAudioMuffleState::Muffled)
	{
		if (GetWorld())
		{
			if (FAudioDeviceHandle AudioDevice = GetWorld()->GetAudioDevice())
			{
				UAudioMixerBlueprintLibrary::ClearSubmixEffectChainOverride(
					this, &AudioDevice->GetMainSubmixObject(), 0.0f);
			}
		}
	}
	Super::Deinitialize();
}

void UOvercrowdAudioSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!bHasBoundOvercrowdComponent)
	{
		TryBindOvercrowdComponent();
	}
}

TStatId UOvercrowdAudioSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UOvercrowdAudioSubsystem, STATGROUP_Tickables);
}

bool UOvercrowdAudioSubsystem::TryBindOvercrowdComponent()
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
			Overcrowd->OnPanicOverloadStateChanged.AddDynamic(this, &UOvercrowdAudioSubsystem::HandlePanicOverloadStateChanged);
			bHasBoundOvercrowdComponent = true;
			return true;
		}
	}
	return false;
}

void UOvercrowdAudioSubsystem::HandlePanicOverloadStateChanged(EPanicOverloadState NewState)
{
	SetMuffleState(NewState == EPanicOverloadState::Active
		? EOvercrowdAudioMuffleState::Muffled
		: EOvercrowdAudioMuffleState::Clear);
}

void UOvercrowdAudioSubsystem::SetMuffleState(EOvercrowdAudioMuffleState NewState)
{
	if (NewState == CurrentMuffleState)
	{
		return;
	}

	// Flip before broadcasting (see MusicSubsystem::SetMusicState) so a re-entrant listener
	// sees CurrentMuffleState already updated.
	CurrentMuffleState = NewState;

	bool bAppliedSubmixOverride = false;
	if (GetWorld())
	{
		if (FAudioDeviceHandle AudioDevice = GetWorld()->GetAudioDevice())
		{
			USoundSubmix& MainSubmix = AudioDevice->GetMainSubmixObject();
			if (NewState == EOvercrowdAudioMuffleState::Muffled)
			{
				UAudioMixerBlueprintLibrary::SetSubmixEffectChainOverride(
					this, &MainSubmix, { MuffleFilterPreset }, MuffleFadeTimeSeconds);
			}
			else
			{
				UAudioMixerBlueprintLibrary::ClearSubmixEffectChainOverride(
					this, &MainSubmix, MuffleFadeTimeSeconds);
			}
			bAppliedSubmixOverride = true;
		}
	}

	if (!bAppliedSubmixOverride && !bHasWarnedMissingAudioDevice)
	{
		// CurrentMuffleState/the broadcast below still reflect the requested state even though
		// the submix call was skipped (see MusicSubsystem::PlayTrackForState's
		// bHasWarnedMissingTrack precedent for this same "logically succeeded, side effect
		// didn't" shape) - log once so a missing world/audio device window is diagnosable
		// instead of silently invisible.
		bHasWarnedMissingAudioDevice = true;
		UE_LOG(LogTemp, Warning,
			TEXT("UOvercrowdAudioSubsystem: no world/audio device available - muffle state changed to %d but the submix override could not be applied."),
			static_cast<int32>(NewState));
	}

	OnOvercrowdAudioMuffleStateChanged.Broadcast(CurrentMuffleState);
}
