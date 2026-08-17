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
		}
	}

	OnOvercrowdAudioMuffleStateChanged.Broadcast(CurrentMuffleState);
}
